/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMAdapter.h"
#include "content_decryption_module.h"
#include "VideoUtils.h"
#include "gmp-api/gmp-entrypoints.h"
#include "gmp-api/gmp-decryption.h"
#include "gmp-api/gmp-video-codec.h"
#include "gmp-api/gmp-platform.h"
#include "WidevineUtils.h"
#include "GMPLog.h"

// Declared in WidevineAdapter.cpp.
extern const GMPPlatformAPI* sPlatform;

namespace mozilla {

void
ChromiumCDMAdapter::SetAdaptee(PRLibrary* aLib)
{
  mLib = aLib;
}

void*
ChromiumCdmHost(int aHostInterfaceVersion, void* aUserData)
{
  CDM_LOG("ChromiumCdmHostFunc(%d, %p)", aHostInterfaceVersion, aUserData);
  if (aHostInterfaceVersion != cdm::Host_8::kVersion) {
    return nullptr;
  }
  return static_cast<cdm::Host_8*>(aUserData);
}

#define STRINGIFY(s) _STRINGIFY(s)
#define _STRINGIFY(s) #s

GMPErr
ChromiumCDMAdapter::GMPInit(const GMPPlatformAPI* aPlatformAPI)
{
  CDM_LOG("ChromiumCDMAdapter::GMPInit");
  sPlatform = aPlatformAPI;
  if (!mLib) {
    return GMPGenericErr;
  }

  auto init = reinterpret_cast<decltype(::INITIALIZE_CDM_MODULE)*>(
    PR_FindFunctionSymbol(mLib, STRINGIFY(INITIALIZE_CDM_MODULE)));
  if (!init) {
    return GMPGenericErr;
  }

  CDM_LOG(STRINGIFY(INITIALIZE_CDM_MODULE)"()");
  init();

  return GMPNoErr;
}

GMPErr
ChromiumCDMAdapter::GMPGetAPI(const char* aAPIName,
                              void* aHostAPI,
                              void** aPluginAPI,
                              uint32_t aDecryptorId)
{
  CDM_LOG("ChromiumCDMAdapter::GMPGetAPI(%s, 0x%p, 0x%p, %u) this=0x%p",
          aAPIName,
          aHostAPI,
          aPluginAPI,
          aDecryptorId,
          this);
  if (!strcmp(aAPIName, CHROMIUM_CDM_API)) {
    auto create = reinterpret_cast<decltype(::CreateCdmInstance)*>(
      PR_FindFunctionSymbol(mLib, "CreateCdmInstance"));
    if (!create) {
      CDM_LOG("ChromiumCDMAdapter::GMPGetAPI(%s, 0x%p, 0x%p, %u) this=0x%p "
              "FAILED to find CreateCdmInstance",
              aAPIName,
              aHostAPI,
              aPluginAPI,
              aDecryptorId,
              this);
      return GMPGenericErr;
    }

    auto cdm = reinterpret_cast<cdm::ContentDecryptionModule_8*>(
      create(cdm::ContentDecryptionModule_8::kVersion,
             kEMEKeySystemWidevine.get(),
             kEMEKeySystemWidevine.Length(),
             &ChromiumCdmHost,
             aHostAPI));
    if (!cdm) {
      CDM_LOG("ChromiumCDMAdapter::GMPGetAPI(%s, 0x%p, 0x%p, %u) this=0x%p "
              "FAILED to create cdm",
              aAPIName,
              aHostAPI,
              aPluginAPI,
              aDecryptorId,
              this);
      return GMPGenericErr;
    }
    CDM_LOG("cdm: 0x%p", cdm);
    *aPluginAPI = cdm;
  }
  return *aPluginAPI ? GMPNoErr : GMPNotImplementedErr;
}

void
ChromiumCDMAdapter::GMPShutdown()
{
  CDM_LOG("ChromiumCDMAdapter::GMPShutdown()");

  decltype(::DeinitializeCdmModule)* deinit;
  deinit = (decltype(deinit))(PR_FindFunctionSymbol(mLib, "DeinitializeCdmModule"));
  if (deinit) {
    CDM_LOG("DeinitializeCdmModule()");
    deinit();
  }
}

/* static */
bool
ChromiumCDMAdapter::Supports(int32_t aModuleVersion,
                             int32_t aInterfaceVersion,
                             int32_t aHostVersion)
{
  return aModuleVersion == CDM_MODULE_VERSION &&
         aInterfaceVersion == cdm::ContentDecryptionModule_8::kVersion &&
         aHostVersion == cdm::Host_8::kVersion;
}

} // namespace mozilla
