/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPLoader.h"
#include <stdio.h>
#include "mozilla/Attributes.h"
#include "gmp-entrypoints.h"
#include "prlink.h"
#include "prenv.h"

#include <string>

#ifdef XP_WIN
#include "windows.h"
#endif

#include "GMPDeviceBinding.h"

namespace mozilla {
namespace gmp {

class GMPLoaderImpl : public GMPLoader {
public:
  explicit GMPLoaderImpl(SandboxStarter* aStarter)
    : mSandboxStarter(aStarter)
    , mAdapter(nullptr)
  {}
  virtual ~GMPLoaderImpl() {}

  bool Load(const char* aUTF8LibPath,
            uint32_t aUTF8LibPathLen,
            char* aOriginSalt,
            uint32_t aOriginSaltLen,
            const GMPPlatformAPI* aPlatformAPI,
            GMPAdapter* aAdapter) override;

  GMPErr GetAPI(const char* aAPIName,
                void* aHostAPI,
                void** aPluginAPI,
                uint32_t aDecryptorId) override;

  void Shutdown() override;

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  void SetSandboxInfo(MacSandboxInfo* aSandboxInfo) override;
#endif

private:
  SandboxStarter* mSandboxStarter;
  UniquePtr<GMPAdapter> mAdapter;
};

UniquePtr<GMPLoader> CreateGMPLoader(SandboxStarter* aStarter) {
  return MakeUnique<GMPLoaderImpl>(aStarter);
}

class PassThroughGMPAdapter : public GMPAdapter {
public:
  ~PassThroughGMPAdapter() {
    // Ensure we're always shutdown, even if caller forgets to call GMPShutdown().
    GMPShutdown();
  }

  void SetAdaptee(PRLibrary* aLib) override
  {
    mLib = aLib;
  }

  GMPErr GMPInit(const GMPPlatformAPI* aPlatformAPI) override
  {
    if (!mLib) {
      return GMPGenericErr;
    }
    GMPInitFunc initFunc = reinterpret_cast<GMPInitFunc>(PR_FindFunctionSymbol(mLib, "GMPInit"));
    if (!initFunc) {
      return GMPNotImplementedErr;
    }
    return initFunc(aPlatformAPI);
  }

  GMPErr GMPGetAPI(const char* aAPIName,
                   void* aHostAPI,
                   void** aPluginAPI,
                   uint32_t aDecryptorId) override
  {
    if (!mLib) {
      return GMPGenericErr;
    }
    GMPGetAPIFunc getapiFunc = reinterpret_cast<GMPGetAPIFunc>(PR_FindFunctionSymbol(mLib, "GMPGetAPI"));
    if (!getapiFunc) {
      return GMPNotImplementedErr;
    }
    return getapiFunc(aAPIName, aHostAPI, aPluginAPI);
  }

  void GMPShutdown() override
  {
    if (mLib) {
      GMPShutdownFunc shutdownFunc = reinterpret_cast<GMPShutdownFunc>(PR_FindFunctionSymbol(mLib, "GMPShutdown"));
      if (shutdownFunc) {
        shutdownFunc();
      }
      PR_UnloadLibrary(mLib);
      mLib = nullptr;
    }
  }

  void GMPSetNodeId(const char* aNodeId, uint32_t aLength) override
  {
    if (!mLib) {
      return;
    }
    GMPSetNodeIdFunc setNodeIdFunc = reinterpret_cast<GMPSetNodeIdFunc>(PR_FindFunctionSymbol(mLib, "GMPSetNodeId"));
    if (setNodeIdFunc) {
      setNodeIdFunc(aNodeId, aLength);
    }
  }

private:
  PRLibrary* mLib = nullptr;
};

bool
GMPLoaderImpl::Load(const char* aUTF8LibPath,
                    uint32_t aUTF8LibPathLen,
                    char* aOriginSalt,
                    uint32_t aOriginSaltLen,
                    const GMPPlatformAPI* aPlatformAPI,
                    GMPAdapter* aAdapter)
{
  std::string nodeId;
  if (!CalculateGMPDeviceId(aOriginSalt, aOriginSaltLen, nodeId)) {
    return false;
  }

  // Start the sandbox now that we've generated the device bound node id.
  // This must happen after the node id is bound to the device id, as
  // generating the device id requires privileges.
  if (mSandboxStarter && !mSandboxStarter->Start(aUTF8LibPath)) {
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
  if (MultiByteToWideChar(CP_UTF8, 0, aUTF8LibPath, -1, widePath.get(), pathLen) == 0) {
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

  GMPInitFunc initFunc = reinterpret_cast<GMPInitFunc>(PR_FindFunctionSymbol(lib, "GMPInit"));
  if ((initFunc && aAdapter) ||
      (!initFunc && !aAdapter)) {
    // Ensure that if we're dealing with a GMP we do *not* use an adapter
    // provided from the outside world. This is important as it means we
    // don't call code not covered by Adobe's plugin-container voucher
    // before we pass the node Id to Adobe's GMP.
    return false;
  }

  // Note: PassThroughGMPAdapter's code must remain in this file so that it's
  // covered by Adobe's plugin-container voucher.
  mAdapter.reset((!aAdapter) ? new PassThroughGMPAdapter() : aAdapter);
  mAdapter->SetAdaptee(lib);

  if (mAdapter->GMPInit(aPlatformAPI) != GMPNoErr) {
    return false;
  }

  mAdapter->GMPSetNodeId(nodeId.c_str(), nodeId.size());

  return true;
}

GMPErr
GMPLoaderImpl::GetAPI(const char* aAPIName,
                      void* aHostAPI,
                      void** aPluginAPI,
                      uint32_t aDecryptorId)
{
  return mAdapter->GMPGetAPI(aAPIName, aHostAPI, aPluginAPI, aDecryptorId);
}

void
GMPLoaderImpl::Shutdown()
{
  if (mAdapter) {
    mAdapter->GMPShutdown();
  }
}

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
void
GMPLoaderImpl::SetSandboxInfo(MacSandboxInfo* aSandboxInfo)
{
  if (mSandboxStarter) {
    mSandboxStarter->SetSandboxInfo(aSandboxInfo);
  }
}
#endif
} // namespace gmp
} // namespace mozilla

