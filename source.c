#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#define VERSION "1.0-full"
#define TMP_DIR "/data/data/com.termux/files/usr/tmp/zipapt_workdir"
#define LIST_PATH "/data/data/com.termux/files/usr/share/zipapt"

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void make_dir(const char *dir) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", dir);
    system(cmd);
}

static void clean_temp_dir(void) {
    system("rm -rf " TMP_DIR);
}

static void extract_archive(const char *zdeb) {
    clean_temp_dir();
    make_dir(TMP_DIR);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "unzip -o '%s' -d '%s'", zdeb, TMP_DIR);
    system(cmd);
}

static void install_all_deb(void) {
    system("find " TMP_DIR " -name '*.deb' -exec apt install -y {} \\; >/dev/null 2>&1");
}

static void reinstall_all_deb(void) {
    system("find " TMP_DIR " -name '*.deb' -exec apt reinstall -y {} \\; >/dev/null 2>&1");
}

static void save_package_list(const char *pkgname) {
    make_dir(LIST_PATH);
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.list", LIST_PATH, pkgname);

    FILE *f = fopen(path, "w");
    if (!f) return;

    system("find " TMP_DIR " -name '*.deb' | sed 's/.*\\///;s/_.*//' > " TMP_DIR "/deblist.txt");
    char line[256];
    FILE *in = fopen(TMP_DIR "/deblist.txt", "r");
    if (in) {
        while (fgets(line, sizeof(line), in)) {
            strtok(line, "\n");
            if (strlen(line) > 0)
                fprintf(f, "%s\n", line);
        }
        fclose(in);
    }
    fclose(f);
}

static void uninstall_package(const char *pkgname) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.list", LIST_PATH, pkgname);
    if (!file_exists(path)) {
        fprintf(stderr, "zipapt: package '%s' not installed\n", pkgname);
        return;
    }
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "xargs -a '%s' apt remove -y", path);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "rm -f '%s'", path);
    system(cmd);
    printf("zipapt: uninstalled '%s'\n", pkgname);
}

static void ko_install(const char *zdeb) {
    if (getuid() != 0) {
        fprintf(stderr, "zipapt: ko-install requires root\n");
        return;
    }
    extract_archive(zdeb);
    system("find " TMP_DIR " -name '*.deb' -exec dpkg -x {} / \\; >/dev/null 2>&1");
    clean_temp_dir();
    printf("zipapt: ko-install done\n");
}

static void show_help(void) {
    puts(
"zipapt - Termux .zdeb package manager (C cli)\n"
"Version: " VERSION "\n"
"Usage:\n"
"  zipapt install  FILE.zdeb -n PKGNAME    Install all deb inside zdeb\n"
"  zipapt uninstall PKGNAME                Uninstall all deb from PKGNAME\n"
"  zipapt reinstall FILE.zdeb              Reinstall deb from zdeb\n"
"  zipapt ko-install FILE.zdeb -n PKGNAME  Install to Android root (need root)\n"
    );
}

int main(int argc, char *argv[]) {
    unsetenv("LD_PRELOAD");

    if (argc < 2) {
        show_help();
        return 1;
    }

    if (strcmp(argv[1], "install") == 0) {
        if (argc != 5 || strcmp(argv[3], "-n") != 0) {
            show_help();
            return 1;
        }
        extract_archive(argv[2]);
        install_all_deb();
        save_package_list(argv[4]);
        clean_temp_dir();
        printf("zipapt: install success\n");
        return 0;
    }

    if (strcmp(argv[1], "uninstall") == 0) {
        if (argc != 3) {
            show_help();
            return 1;
        }
        uninstall_package(argv[2]);
        return 0;
    }

    if (strcmp(argv[1], "reinstall") == 0) {
        if (argc != 3) {
            show_help();
            return 1;
        }
        extract_archive(argv[2]);
        reinstall_all_deb();
        clean_temp_dir();
        printf("zipapt: reinstall success\n");
        return 0;
    }

    if (strcmp(argv[1], "ko-install") == 0) {
        if (argc != 5 || strcmp(argv[3], "-n") != 0) {
            show_help();
            return 1;
        }
        ko_install(argv[2]);
        return 0;
    }

    show_help();
    return 1;
}
