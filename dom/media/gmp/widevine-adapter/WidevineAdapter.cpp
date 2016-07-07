/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineAdapter.h"
#include "content_decryption_module.h"
#include "WidevineDecryptor.h"
#include "WidevineUtils.h"
#include "WidevineVideoDecoder.h"
#include "gmp-api/gmp-entrypoints.h"
#include "gmp-api/gmp-decryption.h"
#include "gmp-api/gmp-video-codec.h"
#include "gmp-api/gmp-platform.h"
#include "mozilla/StaticPtr.h"

static const GMPPlatformAPI* sPlatform = nullptr;

namespace mozilla {

const char* WidevineKeySystem = "com.widevine.alpha";
StaticRefPtr<CDMWrapper> sCDMWrapper;

GMPErr GMPGetCurrentTime(GMPTimestamp* aOutTime) {
  return sPlatform->getcurrenttime(aOutTime);
}

// Call on main thread only.
GMPErr GMPSetTimerOnMainThread(GMPTask* aTask, int64_t aTimeoutMS) {
  return sPlatform->settimer(aTask, aTimeoutMS);
}

GMPErr GMPCreateRecord(const char* aRecordName,
                       uint32_t aRecordNameSize,
                       GMPRecord** aOutRecord,
                       GMPRecordClient* aClient)
{
  return sPlatform->createrecord(aRecordName, aRecordNameSize, aOutRecord, aClient);
}

void
WidevineAdapter::SetAdaptee(PRLibrary* aLib)
{
  mLib = aLib;
}

void* GetCdmHost(int aHostInterfaceVersion, void* aUserData)
{
  Log("GetCdmHostFunc(%d, %p)", aHostInterfaceVersion, aUserData);
  WidevineDecryptor* decryptor = reinterpret_cast<WidevineDecryptor*>(aUserData);
  MOZ_ASSERT(decryptor);
  return static_cast<cdm::Host_8*>(decryptor);
}

#define STRINGIFY(s) _STRINGIFY(s)
#define _STRINGIFY(s) #s

GMPErr
WidevineAdapter::GMPInit(const GMPPlatformAPI* aPlatformAPI)
{
#ifdef ENABLE_WIDEVINE_LOG
  if (getenv("GMP_LOG_FILE")) {
    // Clear log file.
    FILE* f = fopen(getenv("GMP_LOG_FILE"), "w");
    if (f) {
      fclose(f);
    }
  }
#endif

  sPlatform = aPlatformAPI;
  if (!mLib) {
    return GMPGenericErr;
  }

  auto init = reinterpret_cast<decltype(::INITIALIZE_CDM_MODULE)*>(
    PR_FindFunctionSymbol(mLib, STRINGIFY(INITIALIZE_CDM_MODULE)));
  if (!init) {
    return GMPGenericErr;
  }

  Log(STRINGIFY(INITIALIZE_CDM_MODULE)"()");
  init();

  return GMPNoErr;
}

GMPErr
WidevineAdapter::GMPGetAPI(const char* aAPIName,
                           void* aHostAPI,
                           void** aPluginAPI)
{
  Log("WidevineAdapter::GMPGetAPI(%s, 0x%p, 0x%p) this=0x%p",
      aAPIName, aHostAPI, aPluginAPI, this);
  if (!strcmp(aAPIName, GMP_API_DECRYPTOR)) {
    if (sCDMWrapper) {
      // We only support one CDM instance per GMP process. Fail!
      Log("WidevineAdapter::GMPGetAPI() Tried to create more than once CDM per process! FAIL!");
      return GMPQuotaExceededErr;
    }
    auto create = reinterpret_cast<decltype(::CreateCdmInstance)*>(
      PR_FindFunctionSymbol(mLib, "CreateCdmInstance"));
    if (!create) {
      Log("WidevineAdapter::GMPGetAPI(%s, 0x%p, 0x%p) this=0x%p FAILED to find CreateCdmInstance",
        aAPIName, aHostAPI, aPluginAPI, this);
      return GMPGenericErr;
    }

    WidevineDecryptor* decryptor = new WidevineDecryptor();

    auto cdm = reinterpret_cast<cdm::ContentDecryptionModule*>(
      create(cdm::ContentDecryptionModule::kVersion,
             WidevineKeySystem,
             strlen(WidevineKeySystem),
             &GetCdmHost,
             decryptor));
    if (!cdm) {
      Log("WidevineAdapter::GMPGetAPI(%s, 0x%p, 0x%p) this=0x%p FAILED to create cdm",
          aAPIName, aHostAPI, aPluginAPI, this);
      return GMPGenericErr;
    }
    Log("cdm: 0x%x", cdm);
    sCDMWrapper = new CDMWrapper(cdm);
    decryptor->SetCDM(RefPtr<CDMWrapper>(sCDMWrapper));
    *aPluginAPI = decryptor;

  } else if (!strcmp(aAPIName, GMP_API_VIDEO_DECODER)) {
    if (!sCDMWrapper) {
      Log("WidevineAdapter::GMPGetAPI(%s, 0x%p, 0x%p) this=0x%p No cdm for video decoder",
          aAPIName, aHostAPI, aPluginAPI, this);
      return GMPGenericErr;
    }
    *aPluginAPI = new WidevineVideoDecoder(static_cast<GMPVideoHost*>(aHostAPI),
                                           RefPtr<CDMWrapper>(sCDMWrapper));

  }
  return *aPluginAPI ? GMPNoErr : GMPNotImplementedErr;
}

void
WidevineAdapter::GMPShutdown()
{
  Log("WidevineAdapter::GMPShutdown()");

  decltype(::DeinitializeCdmModule)* deinit;
  deinit = (decltype(deinit))(PR_FindFunctionSymbol(mLib, "DeinitializeCdmModule"));
  if (deinit) {
    Log("DeinitializeCdmModule()");
    deinit();
  }
}

void
WidevineAdapter::GMPSetNodeId(const char* aNodeId, uint32_t aLength)
{

}

/* static */
bool
WidevineAdapter::Supports(int32_t aModuleVersion,
                          int32_t aInterfaceVersion,
                          int32_t aHostVersion)
{
  return aModuleVersion == CDM_MODULE_VERSION &&
         aInterfaceVersion == cdm::ContentDecryptionModule::kVersion &&
         aHostVersion == cdm::Host_8::kVersion;
}

} // namespace mozilla
