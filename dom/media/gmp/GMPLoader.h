/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMP_LOADER_H__
#define GMP_LOADER_H__

#include <stdint.h>
#include "prlink.h"
#include "gmp-entrypoints.h"
#include "mozilla/UniquePtr.h"
#include "nsString.h"

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include "mozilla/Sandbox.h"
#endif

namespace mozilla {
namespace gmp {

class SandboxStarter {
 public:
  virtual ~SandboxStarter() = default;
  virtual bool Start(const char* aLibPath) = 0;
};

// Interface that adapts a plugin to the GMP API.
class GMPAdapter {
 public:
  virtual ~GMPAdapter() = default;
  // Sets the adapted to plugin library module.
  // Note: the GMPAdapter is responsible for calling PR_UnloadLibrary on aLib
  // when it's finished with it.
  virtual void SetAdaptee(PRLibrary* aLib) = 0;

  // These are called in place of the corresponding GMP API functions.
  virtual GMPErr GMPInit(const GMPPlatformAPI* aPlatformAPI) = 0;
  // The `aKeySystem` arg is used to specify the key system when loading CDMs,
  // and will be ignored by non-CDM GMPs. It is not part of the public GMP API
  // Gecko exposes.
  virtual GMPErr GMPGetAPI(const char* aAPIName, void* aHostAPI,
                           void** aPluginAPI, const nsCString& aKeySystem) = 0;
  virtual void GMPShutdown() = 0;
};

// Encapsulates activating the sandbox, and loading the GMP.
// Load() takes an optional GMPAdapter which can be used to adapt non-GMPs
// to adhere to the GMP API.
class GMPLoader {
 public:
  GMPLoader();

  // Activates the sandbox, then loads the GMP library. If aAdapter is
  // non-null, the lib path is assumed to be a non-GMP, and the adapter
  // is initialized with the lib and the adapter is used to interact with
  // the plugin.
  bool Load(const char* aUTF8LibPath, uint32_t aLibPathLen,
            const GMPPlatformAPI* aPlatformAPI, GMPAdapter* aAdapter = nullptr);

  // Retrieves an interface pointer from the GMP. If the GMP is loading a CDM,
  // aKeySystem is passed to the CDM to allow for key system specific
  // configuration by the CDM.
  GMPErr GetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI,
                const nsCString& aKeySystem);

  // Calls the GMPShutdown function exported by the GMP lib, and unloads the
  // plugin library.
  void Shutdown();

  bool CanSandbox() const;

 private:
  UniquePtr<SandboxStarter> mSandboxStarter;
  UniquePtr<GMPAdapter> mAdapter;
};

}  // namespace gmp
}  // namespace mozilla

#endif  // GMP_LOADER_H__
