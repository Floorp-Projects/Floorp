/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPCOM.h"
#include "nsXULAppAPI.h"

// FIXME/cjones testing
#if !defined(OS_WIN)
#include <unistd.h>
#endif

#ifdef XP_WIN
#include <windows.h>
// we want a wmain entry point
// but we don't want its DLL load protection, because we'll handle it here
#define XRE_DONT_PROTECT_DLL_LOAD
#include "nsWindowsWMain.cpp"
#include "nsSetDllDirectory.h"
#endif

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
#include "sandbox/base/basictypes.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#endif

#ifdef MOZ_WIDGET_GONK
# include <sys/time.h>
# include <sys/resource.h> 

# include <binder/ProcessState.h>

# ifdef LOGE_IF
#  undef LOGE_IF
# endif

# include <android/log.h>
# define LOGE_IF(cond, ...) \
     ( (CONDITION(cond)) \
     ? ((void)__android_log_print(ANDROID_LOG_ERROR, \
       "Gecko:MozillaRntimeMain", __VA_ARGS__)) \
     : (void)0 )

#endif

#ifdef MOZ_NUWA_PROCESS
#include <binder/ProcessState.h>
#include "ipc/Nuwa.h"
#endif

#ifdef MOZ_WIDGET_GONK
static void
InitializeBinder(void *aDummy) {
    // Change thread priority to 0 only during calling ProcessState::self().
    // The priority is registered to binder driver and used for default Binder
    // Thread's priority. 
    // To change the process's priority to small value need's root permission.
    int curPrio = getpriority(PRIO_PROCESS, 0);
    int err = setpriority(PRIO_PROCESS, 0, 0);
    MOZ_ASSERT(!err);
    LOGE_IF(err, "setpriority failed. Current process needs root permission.");
    android::ProcessState::self()->startThreadPool();
    setpriority(PRIO_PROCESS, 0, curPrio);
}
#endif

int
main(int argc, char* argv[])
{
    bool isNuwa = false;
    bool isSandboxEnabled = false;
    for (int i = 1; i < argc; i++) {
        isNuwa |= strcmp(argv[i], "-nuwa") == 0;
        isSandboxEnabled |= strcmp(argv[i], "-sandbox") == 0;
    }

#ifdef MOZ_NUWA_PROCESS
    if (isNuwa) {
        PrepareNuwaProcess();
    }
#endif

#ifdef MOZ_WIDGET_GONK
    // This creates a ThreadPool for binder ipc. A ThreadPool is necessary to
    // receive binder calls, though not necessary to send binder calls.
    // ProcessState::Self() also needs to be called once on the main thread to
    // register the main thread with the binder driver.

#ifdef MOZ_NUWA_PROCESS
    if (!isNuwa) {
        InitializeBinder(nullptr);
    } else {
        NuwaAddFinalConstructor(&InitializeBinder, nullptr);
    }
#else
    InitializeBinder(nullptr);
#endif
#endif

#if defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
    if (isSandboxEnabled) {
        sandbox::TargetServices* target_service =
            sandbox::SandboxFactory::GetTargetServices();
        if (!target_service) {
            return 1;
        }

        sandbox::ResultCode result = target_service->Init();
        if (result != sandbox::SBOX_ALL_OK) {
            return 2;
        }

        // Initialization is finished, switch to the lowered token
        target_service->LowerToken();
    }
#endif

    // Check for the absolute minimum number of args we need to move
    // forward here. We expect the last arg to be the child process type.
    if (argc < 1)
      return 3;
    GeckoProcessType proctype = XRE_StringToChildProcessType(argv[--argc]);

#ifdef XP_WIN
    // For plugins, this is done in PluginProcessChild::Init, as we need to
    // avoid it for unsupported plugins.  See PluginProcessChild::Init for
    // the details.
    if (proctype != GeckoProcessType_Plugin) {
        mozilla::SanitizeEnvironmentVariables();
        SetDllDirectory(L"");
    }
#endif

    nsresult rv = XRE_InitChildProcess(argc, argv, proctype);
    NS_ENSURE_SUCCESS(rv, 1);

    return 0;
}
