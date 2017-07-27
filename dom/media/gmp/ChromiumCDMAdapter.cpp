/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMAdapter.h"
#include "content_decryption_module.h"
#include "content_decryption_module_ext.h"
#include "VideoUtils.h"
#include "gmp-api/gmp-entrypoints.h"
#include "gmp-api/gmp-decryption.h"
#include "gmp-api/gmp-video-codec.h"
#include "gmp-api/gmp-platform.h"
#include "WidevineUtils.h"
#include "GMPLog.h"
#include "mozilla/Move.h"

#if defined(XP_MACOSX) || defined(XP_LINUX)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif

// Declared in WidevineAdapter.cpp.
extern const GMPPlatformAPI* sPlatform;

namespace mozilla {

ChromiumCDMAdapter::ChromiumCDMAdapter(nsTArray<nsCString>&& aHostFilePaths)
{
  PopulateHostFiles(Move(aHostFilePaths));
}

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

static cdm::HostFile
TakeToCDMHostFile(HostFileData& aHostFileData)
{
  return cdm::HostFile(aHostFileData.mBinary.Path().get(),
                       aHostFileData.mBinary.TakePlatformFile(),
                       aHostFileData.mSig.TakePlatformFile());
}

GMPErr
ChromiumCDMAdapter::GMPInit(const GMPPlatformAPI* aPlatformAPI)
{
  CDM_LOG("ChromiumCDMAdapter::GMPInit");
  sPlatform = aPlatformAPI;
  if (!mLib) {
    return GMPGenericErr;
  }

  // Note: we must call the VerifyCdmHost_0 function if it's present before
  // we call the initialize function.
  auto verify = reinterpret_cast<decltype(::VerifyCdmHost_0)*>(
    PR_FindFunctionSymbol(mLib, STRINGIFY(VerifyCdmHost_0)));
  if (verify) {
    nsTArray<cdm::HostFile> files;
    for (HostFileData& hostFile : mHostFiles) {
      files.AppendElement(TakeToCDMHostFile(hostFile));
    }
    bool result = verify(files.Elements(), files.Length());
    GMP_LOG("%s VerifyCdmHost_0 returned %d", __func__, result);
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

HostFile::HostFile(HostFile&& aOther)
  : mPath(aOther.mPath)
  , mFile(aOther.TakePlatformFile())
{
}

HostFile::~HostFile()
{
  if (mFile != cdm::kInvalidPlatformFile) {
#ifdef XP_WIN
    CloseHandle(mFile);
#else
    close(mFile);
#endif
    mFile = cdm::kInvalidPlatformFile;
  }
}

#ifdef XP_WIN
HostFile::HostFile(const nsCString& aPath)
  : mPath(NS_ConvertUTF8toUTF16(aPath))
{
  HANDLE handle = CreateFileW(mPath.get(),
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);
  mFile = (handle == INVALID_HANDLE_VALUE) ? cdm::kInvalidPlatformFile : handle;
}
#endif

#if defined(XP_MACOSX) || defined(XP_LINUX)
HostFile::HostFile(const nsCString& aPath)
  : mPath(aPath)
{
  // Note: open() returns -1 on failure; i.e. kInvalidPlatformFile.
  mFile = open(aPath.get(), O_RDONLY);
}
#endif

cdm::PlatformFile
HostFile::TakePlatformFile()
{
  cdm::PlatformFile f = mFile;
  mFile = cdm::kInvalidPlatformFile;
  return f;
}

void
ChromiumCDMAdapter::PopulateHostFiles(nsTArray<nsCString>&& aHostFilePaths)
{
  for (const nsCString& path : aHostFilePaths) {
    mHostFiles.AppendElement(
      HostFileData(mozilla::HostFile(path),
                   mozilla::HostFile(path + NS_LITERAL_CSTRING(".sig"))));
  }
}

} // namespace mozilla
