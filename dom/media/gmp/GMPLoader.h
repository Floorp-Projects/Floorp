/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMP_LOADER_H__
#define GMP_LOADER_H__

#include <stdint.h>
#include "gmp-entrypoints.h"

namespace mozilla {
namespace gmp {

class SandboxStarter {
public:
  virtual ~SandboxStarter() {}
  virtual void Start() = 0;
};

#if (defined(XP_LINUX) || defined(XP_MACOSX))
#define SANDBOX_NOT_STATICALLY_LINKED_INTO_PLUGIN_CONTAINER 1
#endif

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
class GMPLoader {
public:
  virtual ~GMPLoader() {}

  // Calculates the device-bound node id, then activates the sandbox,
  // then loads the GMP library and (if applicable) passes the bound node id
  // to the GMP.
  virtual bool Load(const char* aLibPath,
                    uint32_t aLibPathLen,
                    char* aOriginSalt,
                    uint32_t aOriginSaltLen,
                    const GMPPlatformAPI* aPlatformAPI) = 0;

  // Retrieves an interface pointer from the GMP.
  virtual GMPErr GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI) = 0;

  // Calls the GMPShutdown function exported by the GMP lib, and unloads the
  // plugin library.
  virtual void Shutdown() = 0;

#ifdef SANDBOX_NOT_STATICALLY_LINKED_INTO_PLUGIN_CONTAINER
  // Encapsulates starting the sandbox on Linux and MacOSX.
  // TODO: Remove this, and put sandbox in plugin-container on all platforms.
  virtual void SetStartSandboxStarter(SandboxStarter* aStarter) = 0;
#endif
};

// On Desktop, this function resides in plugin-container.
// On Mobile, this function resides in XUL.
GMPLoader* CreateGMPLoader(SandboxStarter* aStarter);

} // namespace gmp
} // namespace mozilla

#endif // GMP_LOADER_H__
