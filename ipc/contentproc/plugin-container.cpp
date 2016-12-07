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

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
class MacSandboxStarter : public mozilla::gmp::SandboxStarter {
public:
    bool Start(const char *aLibPath) override {
      std::string err;
      bool rv = mozilla::StartMacSandbox(mInfo, err);
      if (!rv) {
        fprintf(stderr, "sandbox_init() failed! Error \"%s\"\n", err.c_str());
      }
      return rv;
    }
    void SetSandboxInfo(MacSandboxInfo* aSandboxInfo) override {
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

#ifdef XP_WIN
    // For plugins, this is done in PluginProcessChild::Init, as we need to
    // avoid it for unsupported plugins.  See PluginProcessChild::Init for
    // the details.
    if (XRE_GetProcessType() != GeckoProcessType_Plugin) {
        mozilla::SanitizeEnvironmentVariables();
        SetDllDirectoryW(L"");
    }
#endif
#if !defined(XP_LINUX) && defined(MOZ_PLUGIN_CONTAINER)
    // On Windows and MacOS, the GMPLoader lives in plugin-container, so that its
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
