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

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#include "mozilla/sandboxTarget.h"
#endif

#if defined(HASH_NODE_ID_WITH_DEVICE_ID)
// In order to provide EME plugins with a "device binding" capability,
// in the parent we generate and store some random bytes as salt for every
// (origin, urlBarOrigin) pair that uses EME. We store these bytes so
// that every time we revisit the same origin we get the same salt.
// We send this salt to the child on startup. The child collects some
// device specific data and munges that with the salt to create the
// "node id" that we expose to EME plugins. It then overwrites the device
// specific data, and activates the sandbox.
#include "rlz/lib/machine_id.h"
#include "rlz/lib/string_utils.h"
#include "sha256.h"
#endif

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
  std::string nodeId;
#ifdef HASH_NODE_ID_WITH_DEVICE_ID
  if (aOriginSaltLen > 0) {
    string16 deviceId;
    int volumeId;
    if (!rlz_lib::GetRawMachineId(&deviceId, &volumeId)) {
      return false;
    }

    SHA256Context ctx;
    SHA256_Begin(&ctx);
    SHA256_Update(&ctx, (const uint8_t*)aOriginSalt, aOriginSaltLen);
    SHA256_Update(&ctx, (const uint8_t*)deviceId.c_str(), deviceId.size() * sizeof(string16::value_type));
    SHA256_Update(&ctx, (const uint8_t*)&volumeId, sizeof(int));
    uint8_t digest[SHA256_LENGTH] = {0};
    unsigned int digestLen = 0;
    SHA256_End(&ctx, digest, &digestLen, SHA256_LENGTH);

    // Overwrite all data involved in calculation as it could potentially
    // identify the user, so there's no chance a GMP can read it and use
    // it for identity tracking.
    memset(&ctx, 0, sizeof(ctx));
    memset(aOriginSalt, 0, aOriginSaltLen);
    volumeId = 0;
    memset(&deviceId[0], '*', sizeof(string16::value_type) * deviceId.size());
    deviceId = L"";

    if (!rlz_lib::BytesToString(digest, SHA256_LENGTH, &nodeId)) {
      return false;
    }
  } else
#endif
  {
    nodeId = std::string(aOriginSalt, aOriginSalt + aOriginSaltLen);
  }

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

  GMPSetNodeIdFunc setNodeIdFunc = reinterpret_cast<GMPSetNodeIdFunc>(PR_FindFunctionSymbol(mLib, "GMPSetNodeId"));
  if (setNodeIdFunc) {
    setNodeIdFunc(nodeId.c_str(), nodeId.size());
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

