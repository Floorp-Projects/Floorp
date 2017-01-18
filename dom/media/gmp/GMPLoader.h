/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMP_LOADER_H__
#define GMP_LOADER_H__

#include <stdint.h>
#include "prlink.h"
#include "gmp-entrypoints.h"
#include "mozilla/UniquePtr.h"

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
#include "mozilla/Sandbox.h"
#endif

namespace mozilla {
namespace gmp {

class SandboxStarter {
public:
  virtual ~SandboxStarter() {}
  virtual bool Start(const char* aLibPath) = 0;
#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  // On OS X we need to set Mac-specific sandbox info just before we start the
  // sandbox, which we don't yet know when the GMPLoader and SandboxStarter
  // objects are created.
  virtual void SetSandboxInfo(MacSandboxInfo* aSandboxInfo) = 0;
#endif
};

// Interface that adapts a plugin to the GMP API.
class GMPAdapter {
public:
  virtual ~GMPAdapter() {}
  // Sets the adapted to plugin library module.
  // Note: the GMPAdapter is responsible for calling PR_UnloadLibrary on aLib
  // when it's finished with it.
  virtual void SetAdaptee(PRLibrary* aLib) = 0;

  // These are called in place of the corresponding GMP API functions.
  virtual GMPErr GMPInit(const GMPPlatformAPI* aPlatformAPI) = 0;
  virtual GMPErr GMPGetAPI(const char* aAPIName,
                           void* aHostAPI,
                           void** aPluginAPI,
                           uint32_t aDecryptorId) = 0;
  virtual void GMPShutdown() = 0;
  virtual void GMPSetNodeId(const char* aNodeId, uint32_t aLength) = 0;
};

// Encapsulates generating the device-bound node id, activating the sandbox,
// loading the GMP, and passing the node id to the GMP (in that order).
//
// In Desktop Gecko, the implementation of this lives in plugin-container,
// and is passed into XUL code from on startup. The GMP IPC child protocol actor
// uses this interface to load and retrieve interfaces from the GMPs.
//
// In Desktop Gecko the implementation lives in the plugin-container so that
// it can be covered by DRM vendor's voucher.
//
// On Android the GMPLoader implementation lives in libxul (because for the time
// being GMPLoader relies upon NSPR, which we can't use in plugin-container
// on Android).
//
// There is exactly one GMPLoader per GMP child process, and only one GMP
// per child process (so the GMPLoader only loads one GMP).
//
// Load() takes an optional GMPAdapter which can be used to adapt non-GMPs
// to adhere to the GMP API.
class GMPLoader {
public:
  virtual ~GMPLoader() {}

  // Calculates the device-bound node id, then activates the sandbox,
  // then loads the GMP library and (if applicable) passes the bound node id
  // to the GMP. If aAdapter is non-null, the lib path is assumed to be
  // a non-GMP, and the adapter is initialized with the lib and the adapter
  // is used to interact with the plugin.
  virtual bool Load(const char* aUTF8LibPath,
                    uint32_t aLibPathLen,
                    char* aOriginSalt,
                    uint32_t aOriginSaltLen,
                    const GMPPlatformAPI* aPlatformAPI,
                    GMPAdapter* aAdapter = nullptr) = 0;

  // Retrieves an interface pointer from the GMP.
  virtual GMPErr GetAPI(const char* aAPIName,
                        void* aHostAPI,
                        void** aPluginAPI,
                        uint32_t aDecryptorId) = 0;

  // Calls the GMPShutdown function exported by the GMP lib, and unloads the
  // plugin library.
  virtual void Shutdown() = 0;

#if defined(XP_MACOSX) && defined(MOZ_GMP_SANDBOX)
  // On OS X we need to set Mac-specific sandbox info just before we start the
  // sandbox, which we don't yet know when the GMPLoader and SandboxStarter
  // objects are created.
  virtual void SetSandboxInfo(MacSandboxInfo* aSandboxInfo) = 0;
#endif
};

// On Desktop, this function resides in plugin-container.
// On Mobile, this function resides in XUL.
UniquePtr<GMPLoader> CreateGMPLoader(UniquePtr<SandboxStarter> aStarter);

} // namespace gmp
} // namespace mozilla

#endif // GMP_LOADER_H__
