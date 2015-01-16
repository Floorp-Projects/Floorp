#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>

#ifndef B2G_NAME
#define B2G_NAME "b2g-bin"
#endif
#ifndef GAIA_PATH
#define GAIA_PATH "gaia/profile"
#endif
#define NOMEM "Could not allocate enough memory"

void error(char* msg){
    fprintf(stderr, "ERROR: %s\n", msg);
}

int main(int argc, char* argv[], char* envp[]){
    char* cwd = NULL;
    char* full_path = NULL;
    char* full_profile_path = NULL;
    printf("Starting %s\n", B2G_NAME);
    cwd = realpath(dirname(argv[0]), NULL);
    full_path = (char*) malloc(strlen(cwd) + strlen(B2G_NAME) + 2);
    if (!full_path) {
        error(NOMEM);
        return -2;
    }
    full_profile_path = (char*) malloc(strlen(cwd) + strlen(GAIA_PATH) + 2);
    if (!full_profile_path) {
        free(full_path);
        error(NOMEM);
        return -2;
    }
    sprintf(full_path, "%s/%s", cwd, B2G_NAME);
    sprintf(full_profile_path, "%s/%s", cwd, GAIA_PATH);
    free(cwd);
    printf("Running: %s --profile %s\n", full_path, full_profile_path);
    fflush(stdout);
    fflush(stderr);
    // XXX: yes, the printf above says --profile and this execle uses -profile.
    // Bug 1088430 will change the execle to use --profile.
    execle(full_path, full_path, "-profile", full_profile_path, NULL, envp);
    error("unable to start");
    perror(argv[0]);
    free(full_path);
    free(full_profile_path);
    return -1;
}
