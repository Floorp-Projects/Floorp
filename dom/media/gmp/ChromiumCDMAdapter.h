/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumAdapter_h_
#define ChromiumAdapter_h_

#include "GMPLoader.h"
#include "prlink.h"
#include "GMPUtils.h"
#include "nsTArray.h"
#include "content_decryption_module_ext.h"
#include "nsString.h"

#include <utility>

struct GMPPlatformAPI;

namespace mozilla {

#if defined(XP_WIN)
typedef nsString HostFileString;
#else
typedef nsCString HostFileString;
#endif

class HostFile {
 public:
  explicit HostFile(const nsCString& aPath);
  HostFile(HostFile&& aOther);
  ~HostFile();

  const HostFileString& Path() const { return mPath; }
  cdm::PlatformFile TakePlatformFile();

 private:
  const HostFileString mPath;
  cdm::PlatformFile mFile = cdm::kInvalidPlatformFile;
};

struct HostFileData {
  HostFileData(HostFile&& aBinary, HostFile&& aSig)
      : mBinary(std::move(aBinary)), mSig(std::move(aSig)) {}

  HostFileData(HostFileData&& aOther)
      : mBinary(std::move(aOther.mBinary)), mSig(std::move(aOther.mSig)) {}

  ~HostFileData() = default;

  HostFile mBinary;
  HostFile mSig;
};

class ChromiumCDMAdapter : public gmp::GMPAdapter {
 public:
  explicit ChromiumCDMAdapter(
      nsTArray<std::pair<nsCString, nsCString>>&& aHostPathPairs);

  void SetAdaptee(PRLibrary* aLib) override;

  // These are called in place of the corresponding GMP API functions.
  GMPErr GMPInit(const GMPPlatformAPI* aPlatformAPI) override;
  GMPErr GMPGetAPI(const char* aAPIName, void* aHostAPI, void** aPluginAPI,
                   const nsCString& aKeySystem) override;
  void GMPShutdown() override;

  static bool Supports(int32_t aModuleVersion, int32_t aInterfaceVersion,
                       int32_t aHostVersion);

 private:
  void PopulateHostFiles(
      nsTArray<std::pair<nsCString, nsCString>>&& aHostFilePaths);

  PRLibrary* mLib = nullptr;
  nsTArray<HostFileData> mHostFiles;
};

}  // namespace mozilla

#endif  // ChromiumAdapter_h_
