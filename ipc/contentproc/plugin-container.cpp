/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPCOM.h"
#include "nsXULAppAPI.h"
#include "nsAutoPtr.h"

#ifdef XP_WIN
#include <windows.h>
// we want a wmain entry point
// but we don't want its DLL load protection, because we'll handle it here
#define XRE_DONT_PROTECT_DLL_LOAD
#include "nsWindowsWMain.cpp"
#include "nsSetDllDirectory.h"
#else
// FIXME/cjones testing
#include <unistd.h>
#endif

#include "GMPLoader.h"

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#include "mozilla/sandboxing/SandboxInitialization.h"
#include "mozilla/sandboxing/sandboxLogging.h"
#endif

#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
#include "mozilla/Sandbox.h"
#include "mozilla/SandboxInfo.h"
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

# ifdef MOZ_CONTENT_SANDBOX
# include "mozilla/Sandbox.h"
# endif

#endif // MOZ_WIDGET_GONK

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

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
class WinSandboxStarter : public mozilla::gmp::SandboxStarter {
public:
    virtual bool Start(const char *aLibPath) override {
        if (IsSandboxedProcess()) {
            mozilla::sandboxing::LowerSandbox();
        }
        return true;
    }
};
#endif

#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
class LinuxSandboxStarter : public mozilla::gmp::SandboxStarter {
    LinuxSandboxStarter() { }
public:
    static SandboxStarter* Make() {
        if (mozilla::SandboxInfo::Get().CanSandboxMedia()) {
            return new LinuxSandboxStarter();
        } else {
            // Sandboxing isn't possible, but the parent has already
            // checked that this plugin doesn't require it.  (Bug 1074561)
            return nullptr;
        }
    }
    virtual bool Start(const char *aLibPath) override {
        mozilla::SetMediaPluginSandbox(aLibPath);
        return true;
    }
};
#endif

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
class MacSandboxStarter : public mozilla::gmp::SandboxStarter {
public:
    virtual bool Start(const char *aLibPath) override {
      std::string err;
      bool rv = mozilla::StartMacSandbox(mInfo, err);
      if (!rv) {
        fprintf(stderr, "sandbox_init() failed! Error \"%s\"\n", err.c_str());
      }
      return rv;
    }
    virtual void SetSandboxInfo(MacSandboxInfo* aSandboxInfo) override {
      mInfo = *aSandboxInfo;
    }
private:
  MacSandboxInfo mInfo;
};
#endif

mozilla::gmp::SandboxStarter*
MakeSandboxStarter()
{
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
    return new WinSandboxStarter();
#elif defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
    return LinuxSandboxStarter::Make();
#elif defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
    return new MacSandboxStarter();
#else
    return nullptr;
#endif
}

int
content_process_main(int argc, char* argv[])
{
    // Check for the absolute minimum number of args we need to move
    // forward here. We expect the last arg to be the child process type.
    if (argc < 1) {
      return 3;
    }

    bool isNuwa = false;
    for (int i = 1; i < argc; i++) {
        isNuwa |= strcmp(argv[i], "-nuwa") == 0;
    }

    XREChildData childData;

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
    if (IsSandboxedProcess()) {
        childData.sandboxTargetServices =
            mozilla::sandboxing::GetInitializedTargetServices();
        if (!childData.sandboxTargetServices) {
            return 1;
        }

        childData.ProvideLogFunction = mozilla::sandboxing::ProvideLogFunction;
    }
#endif

    XRE_SetProcessType(argv[--argc]);

#ifdef MOZ_NUWA_PROCESS
    if (isNuwa) {
        PrepareNuwaProcess();
    }
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
    // This has to happen while we're still single-threaded, and on
    // B2G that means before the Android Binder library is
    // initialized.  Additional special handling is needed for Nuwa:
    // the Nuwa process itself needs to be unsandboxed, and the same
    // single-threadedness condition applies to its children; see also
    // AfterNuwaFork().
    mozilla::SandboxEarlyInit(XRE_GetProcessType(), isNuwa);
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

#ifdef XP_WIN
    // For plugins, this is done in PluginProcessChild::Init, as we need to
    // avoid it for unsupported plugins.  See PluginProcessChild::Init for
    // the details.
    if (XRE_GetProcessType() != GeckoProcessType_Plugin) {
        mozilla::SanitizeEnvironmentVariables();
        SetDllDirectoryW(L"");
    }
#endif
#if !defined(MOZ_WIDGET_ANDROID) && !defined(MOZ_WIDGET_GONK) && defined(MOZ_PLUGIN_CONTAINER)
    // On desktop, the GMPLoader lives in plugin-container, so that its
    // code can be covered by an EME/GMP vendor's voucher.
    nsAutoPtr<mozilla::gmp::SandboxStarter> starter(MakeSandboxStarter());
    if (XRE_GetProcessType() == GeckoProcessType_GMPlugin) {
        childData.gmpLoader = mozilla::gmp::CreateGMPLoader(starter);
    }
#endif
    nsresult rv = XRE_InitChildProcess(argc, argv, &childData);
    NS_ENSURE_SUCCESS(rv, 1);

    return 0;
}
