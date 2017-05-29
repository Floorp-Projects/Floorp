/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineAdapter.h"
#include "content_decryption_module.h"
#include "VideoUtils.h"
#include "WidevineDecryptor.h"
#include "WidevineDummyDecoder.h"
#include "WidevineUtils.h"
#include "WidevineVideoDecoder.h"
#include "gmp-api/gmp-entrypoints.h"
#include "gmp-api/gmp-decryption.h"
#include "gmp-api/gmp-video-codec.h"
#include "gmp-api/gmp-platform.h"

const GMPPlatformAPI* sPlatform = nullptr;

namespace mozilla {

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
  CDM_LOG("GetCdmHostFunc(%d, %p)", aHostInterfaceVersion, aUserData);
  WidevineDecryptor* decryptor = reinterpret_cast<WidevineDecryptor*>(aUserData);
  MOZ_ASSERT(decryptor);
  return static_cast<cdm::Host_8*>(decryptor);
}

#define STRINGIFY(s) _STRINGIFY(s)
#define _STRINGIFY(s) #s

GMPErr
WidevineAdapter::GMPInit(const GMPPlatformAPI* aPlatformAPI)
{
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
WidevineAdapter::GMPGetAPI(const char* aAPIName,
                           void* aHostAPI,
                           void** aPluginAPI,
                           uint32_t aDecryptorId)
{
  CDM_LOG("WidevineAdapter::GMPGetAPI(%s, 0x%p, 0x%p, %u) this=0x%p",
          aAPIName, aHostAPI, aPluginAPI, aDecryptorId, this);
  if (!strcmp(aAPIName, GMP_API_DECRYPTOR)) {
    if (WidevineDecryptor::GetInstance(aDecryptorId)) {
      // We only support one CDM instance per PGMPDecryptor. Fail!
      CDM_LOG("WidevineAdapter::GMPGetAPI() Tried to create more than once CDM per IPDL actor! FAIL!");
      return GMPQuotaExceededErr;
    }
    auto create = reinterpret_cast<decltype(::CreateCdmInstance)*>(
      PR_FindFunctionSymbol(mLib, "CreateCdmInstance"));
    if (!create) {
      CDM_LOG("WidevineAdapter::GMPGetAPI(%s, 0x%p, 0x%p, %u) this=0x%p FAILED to find CreateCdmInstance",
              aAPIName, aHostAPI, aPluginAPI, aDecryptorId, this);
      return GMPGenericErr;
    }

    auto* decryptor = new WidevineDecryptor();

    auto cdm = reinterpret_cast<cdm::ContentDecryptionModule_8*>(
      create(cdm::ContentDecryptionModule_8::kVersion,
             kEMEKeySystemWidevine.get(),
             kEMEKeySystemWidevine.Length(),
             &GetCdmHost,
             decryptor));
    if (!cdm) {
      CDM_LOG("WidevineAdapter::GMPGetAPI(%s, 0x%p, 0x%p, %u) this=0x%p FAILED to create cdm",
              aAPIName, aHostAPI, aPluginAPI, aDecryptorId, this);
      return GMPGenericErr;
    }
    CDM_LOG("cdm: 0x%p", cdm);
    RefPtr<CDMWrapper> wrapper(new CDMWrapper(cdm, decryptor));
    decryptor->SetCDM(wrapper, aDecryptorId);
    *aPluginAPI = decryptor;

  } else if (!strcmp(aAPIName, GMP_API_VIDEO_DECODER)) {
    RefPtr<CDMWrapper> wrapper = WidevineDecryptor::GetInstance(aDecryptorId);

    // There is a possible race condition, where the decryptor will be destroyed
    // before we are able to create the video decoder, so we create a dummy
    // decoder to avoid crashing.
    if (!wrapper) {
      CDM_LOG("WidevineAdapter::GMPGetAPI(%s, 0x%p, 0x%p, %u) this=0x%p No cdm for video decoder. Using a DummyDecoder",
              aAPIName, aHostAPI, aPluginAPI, aDecryptorId, this);

      *aPluginAPI = new WidevineDummyDecoder();
    } else {
      *aPluginAPI = new WidevineVideoDecoder(static_cast<GMPVideoHost*>(aHostAPI),
                                             wrapper);
    }
  }
  return *aPluginAPI ? GMPNoErr : GMPNotImplementedErr;
}

void
WidevineAdapter::GMPShutdown()
{
  CDM_LOG("WidevineAdapter::GMPShutdown()");

  decltype(::DeinitializeCdmModule)* deinit;
  deinit = (decltype(deinit))(PR_FindFunctionSymbol(mLib, "DeinitializeCdmModule"));
  if (deinit) {
    CDM_LOG("DeinitializeCdmModule()");
    deinit();
  }
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
