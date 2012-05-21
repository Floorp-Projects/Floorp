/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <android/log.h>

int
main(int argc, char* argv[])
{
    // Check for the absolute minimum number of args we need to move
    // forward here. We expect the last arg to be the child process type.
    if (argc < 2)
        return 1;

    void *mozloader_handle = dlopen("libmozglue.so", RTLD_LAZY);
    if (!mozloader_handle) {
      __android_log_print(ANDROID_LOG_ERROR, "GeckoChildLoad",
                          "Couldn't load mozloader because %s", dlerror());
        return 1;
    }

    typedef int (*ChildProcessInit_t)(int, char**);
    ChildProcessInit_t fChildProcessInit =
        (ChildProcessInit_t)dlsym(mozloader_handle, "ChildProcessInit");
    if (!fChildProcessInit) {
        __android_log_print(ANDROID_LOG_ERROR, "GeckoChildLoad",
                            "Couldn't load cpi_t because %s", dlerror());
        return 1;
    }

    return fChildProcessInit(argc, argv);
}
