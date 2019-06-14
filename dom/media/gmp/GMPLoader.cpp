/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPLoader.h"
#include <stdio.h>
#include "mozilla/Attributes.h"
#include "gmp-entrypoints.h"
#include "prlink.h"
#include "prenv.h"
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#  include "mozilla/sandboxTarget.h"
#  include "mozilla/sandboxing/SandboxInitialization.h"
#  include "mozilla/sandboxing/sandboxLogging.h"
#endif
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/Sandbox.h"
#  include "mozilla/SandboxInfo.h"
#endif

#include <string>

#ifdef XP_WIN
#  include "windows.h"
#endif

namespace mozilla {
namespace gmp {
class PassThroughGMPAdapter : public GMPAdapter {
 public:
  ~PassThroughGMPAdapter() override {
    // Ensure we're always shutdown, even if caller forgets to call
    // GMPShutdown().
    GMPShutdown();
  }

  void SetAdaptee(PRLibrary* aLib) override { mLib = aLib; }

  GMPErr GMPInit(const GMPPlatformAPI* aPlatformAPI) override {
    if (!mLib) {
      return GMPGenericErr;
    }
    GMPInitFunc initFunc =
        reinterpret_cast<GMPInitFunc>(PR_FindFunctionSymbol(mLib, "GMPInit"));
    if (!initFunc) {
      return GMPNotImplementedErr;
    }
    return initFunc(aPlatformAPI);
  }

  GMPErr GMPGetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI,
                   uint32_t aDecryptorId) override {
    if (!mLib) {
      return GMPGenericErr;
    }
    GMPGetAPIFunc getapiFunc = reinterpret_cast<GMPGetAPIFunc>(
        PR_FindFunctionSymbol(mLib, "GMPGetAPI"));
    if (!getapiFunc) {
      return GMPNotImplementedErr;
    }
    return getapiFunc(aAPIName, aHostAPI, aPluginAPI);
  }

  void GMPShutdown() override {
    if (mLib) {
      GMPShutdownFunc shutdownFunc = reinterpret_cast<GMPShutdownFunc>(
          PR_FindFunctionSymbol(mLib, "GMPShutdown"));
      if (shutdownFunc) {
        shutdownFunc();
      }
      PR_UnloadLibrary(mLib);
      mLib = nullptr;
    }
  }

 private:
  PRLibrary* mLib = nullptr;
};

bool GMPLoader::Load(const char* aUTF8LibPath, uint32_t aUTF8LibPathLen,
                     const GMPPlatformAPI* aPlatformAPI, GMPAdapter* aAdapter) {
  if (!getenv("MOZ_DISABLE_GMP_SANDBOX") && mSandboxStarter &&
      !mSandboxStarter->Start(aUTF8LibPath)) {
    return false;
  }

  // Load the GMP.
  PRLibSpec libSpec;
#ifdef XP_WIN
  int pathLen = MultiByteToWideChar(CP_UTF8, 0, aUTF8LibPath, -1, nullptr, 0);
  if (pathLen == 0) {
    return false;
  }

  auto widePath = MakeUnique<wchar_t[]>(pathLen);
  if (MultiByteToWideChar(CP_UTF8, 0, aUTF8LibPath, -1, widePath.get(),
                          pathLen) == 0) {
    return false;
  }

  libSpec.value.pathname_u = widePath.get();
  libSpec.type = PR_LibSpec_PathnameU;
#else
  libSpec.value.pathname = aUTF8LibPath;
  libSpec.type = PR_LibSpec_Pathname;
#endif
  PRLibrary* lib = PR_LoadLibraryWithFlags(libSpec, 0);
  if (!lib) {
    return false;
  }

  mAdapter.reset((!aAdapter) ? new PassThroughGMPAdapter() : aAdapter);
  mAdapter->SetAdaptee(lib);

  if (mAdapter->GMPInit(aPlatformAPI) != GMPNoErr) {
    return false;
  }

  return true;
}

GMPErr GMPLoader::GetAPI(const char* aAPIName, void* aHostAPI,
                         void** aPluginAPI, uint32_t aDecryptorId) {
  return mAdapter->GMPGetAPI(aAPIName, aHostAPI, aPluginAPI, aDecryptorId);
}

void GMPLoader::Shutdown() {
  if (mAdapter) {
    mAdapter->GMPShutdown();
  }
}

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
void GMPLoader::SetSandboxInfo(MacSandboxInfo* aSandboxInfo) {
  if (mSandboxStarter) {
    mSandboxStarter->SetSandboxInfo(aSandboxInfo);
  }
}
#endif

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
class WinSandboxStarter : public mozilla::gmp::SandboxStarter {
 public:
  bool Start(const char* aLibPath) override {
    // Cause advapi32 to load before the sandbox is turned on, as
    // Widevine version 970 and later require it and the sandbox
    // blocks it on Win7.
    unsigned int dummy_rand;
    rand_s(&dummy_rand);

    mozilla::SandboxTarget::Instance()->StartSandbox();
    return true;
  }
};
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
class MacSandboxStarter : public mozilla::gmp::SandboxStarter {
 public:
  bool Start(const char* aLibPath) override {
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

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
namespace {
class LinuxSandboxStarter : public mozilla::gmp::SandboxStarter {
 private:
  LinuxSandboxStarter() {}
  friend mozilla::detail::UniqueSelector<LinuxSandboxStarter>::SingleObject
  mozilla::MakeUnique<LinuxSandboxStarter>();

 public:
  static UniquePtr<SandboxStarter> Make() {
    if (mozilla::SandboxInfo::Get().CanSandboxMedia()) {
      return MakeUnique<LinuxSandboxStarter>();
    }
    // Sandboxing isn't possible, but the parent has already
    // checked that this plugin doesn't require it.  (Bug 1074561)
    return nullptr;
  }
  bool Start(const char* aLibPath) override {
    mozilla::SetMediaPluginSandbox(aLibPath);
    return true;
  }
};
}  // anonymous namespace
#endif  // XP_LINUX && MOZ_SANDBOX

static UniquePtr<SandboxStarter> MakeSandboxStarter() {
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  return mozilla::MakeUnique<WinSandboxStarter>();
#elif defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  return mozilla::MakeUnique<MacSandboxStarter>();
#elif defined(XP_LINUX) && defined(MOZ_SANDBOX)
  return LinuxSandboxStarter::Make();
#else
  return nullptr;
#endif
}

GMPLoader::GMPLoader() : mSandboxStarter(MakeSandboxStarter()) {}

bool GMPLoader::CanSandbox() const { return !!mSandboxStarter; }

}  // namespace gmp
}  // namespace mozilla
