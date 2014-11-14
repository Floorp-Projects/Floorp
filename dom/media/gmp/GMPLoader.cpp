/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPLoader.h"
#include <stdio.h>
#include "mozilla/Attributes.h"
#include "mozilla/NullPtr.h"
#include "gmp-entrypoints.h"
#include "prlink.h"

#include <string>

namespace mozilla {
namespace gmp {

class GMPLoaderImpl : public GMPLoader {
public:
  explicit GMPLoaderImpl(SandboxStarter* aStarter)
    : mSandboxStarter(aStarter)
  {}
  virtual ~GMPLoaderImpl() {}

  virtual bool Load(const char* aLibPath,
                    uint32_t aLibPathLen,
                    char* aOriginSalt,
                    uint32_t aOriginSaltLen,
                    const GMPPlatformAPI* aPlatformAPI) MOZ_OVERRIDE;

  virtual GMPErr GetAPI(const char* aAPIName,
                        void* aHostAPI,
                        void** aPluginAPI) MOZ_OVERRIDE;

  virtual void Shutdown() MOZ_OVERRIDE;

#ifdef SANDBOX_NOT_STATICALLY_LINKED_INTO_PLUGIN_CONTAINER
  virtual void SetStartSandboxStarter(SandboxStarter* aStarter) MOZ_OVERRIDE {
    mSandboxStarter = aStarter;
  }
#endif

private:
  PRLibrary* mLib;
  GMPGetAPIFunc mGetAPIFunc;
  SandboxStarter* mSandboxStarter;
};

GMPLoader* CreateGMPLoader(SandboxStarter* aStarter) {
  return static_cast<GMPLoader*>(new GMPLoaderImpl(aStarter));
}

bool
GMPLoaderImpl::Load(const char* aLibPath,
                    uint32_t aLibPathLen,
                    char* aOriginSalt,
                    uint32_t aOriginSaltLen,
                    const GMPPlatformAPI* aPlatformAPI)
{
  std::string nodeId(aOriginSalt, aOriginSalt + aOriginSaltLen);

  // TODO (subsequent patch): Hash node id with device id, send to GMP.

#if defined(MOZ_GMP_SANDBOX)
  // Start the sandbox now that we've generated the device bound node id.
  // This must happen after the node id is bound to the device id, as
  // generating the device id requires privileges.
  if (mSandboxStarter) {
    mSandboxStarter->Start();
  }
#endif

  // Load the GMP.
  PRLibSpec libSpec;
  libSpec.value.pathname = aLibPath;
  libSpec.type = PR_LibSpec_Pathname;
  mLib = PR_LoadLibraryWithFlags(libSpec, 0);
  if (!mLib) {
    return false;
  }

  GMPInitFunc initFunc = reinterpret_cast<GMPInitFunc>(PR_FindFunctionSymbol(mLib, "GMPInit"));
  if (!initFunc) {
    return false;
  }

  if (initFunc(aPlatformAPI) != GMPNoErr) {
    return false;
  }

  mGetAPIFunc = reinterpret_cast<GMPGetAPIFunc>(PR_FindFunctionSymbol(mLib, "GMPGetAPI"));
  if (!mGetAPIFunc) {
    return false;
  }

  return true;
}

GMPErr
GMPLoaderImpl::GetAPI(const char* aAPIName,
                      void* aHostAPI,
                      void** aPluginAPI)
{
  return mGetAPIFunc ? mGetAPIFunc(aAPIName, aHostAPI, aPluginAPI)
                     : GMPGenericErr;
}

void
GMPLoaderImpl::Shutdown()
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

} // namespace gmp
} // namespace mozilla

