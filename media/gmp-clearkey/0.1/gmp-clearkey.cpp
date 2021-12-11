/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
// This include is required in order for content_decryption_module to work
// on Unix systems.
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "content_decryption_module.h"
#include "content_decryption_module_ext.h"
#include "nss.h"

#include "ClearKeyCDM.h"
#include "ClearKeySessionManager.h"
#include "mozilla/dom/KeySystemNames.h"

#ifndef XP_WIN
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#ifdef ENABLE_WMF
#  include "WMFUtils.h"
#endif  // ENABLE_WMF

extern "C" {

CDM_API
void INITIALIZE_CDM_MODULE() {}

static bool sCanReadHostVerificationFiles = false;

CDM_API
void* CreateCdmInstance(int cdm_interface_version, const char* key_system,
                        uint32_t key_system_size,
                        GetCdmHostFunc get_cdm_host_func, void* user_data) {
  CK_LOGE("ClearKey CreateCDMInstance");

  if (cdm_interface_version != cdm::ContentDecryptionModule_10::kVersion) {
    CK_LOGE(
        "ClearKey CreateCDMInstance failed due to requesting unsupported "
        "version %d.",
        cdm_interface_version);
    return nullptr;
  }
#ifdef ENABLE_WMF
  if (!wmf::EnsureLibs()) {
    CK_LOGE("Required libraries were not found");
    return nullptr;
  }
#endif

  if (NSS_NoDB_Init(nullptr) == SECFailure) {
    CK_LOGE("Unable to initialize NSS");
    return nullptr;
  }

#ifdef MOZILLA_OFFICIAL
  // Test that we're able to read the host files.
  if (!sCanReadHostVerificationFiles) {
    return nullptr;
  }
#endif

  cdm::Host_10* host = static_cast<cdm::Host_10*>(
      get_cdm_host_func(cdm_interface_version, user_data));
  ClearKeyCDM* clearKey = new ClearKeyCDM(host);

  CK_LOGE("Created ClearKeyCDM instance!");

  if (strncmp(key_system, mozilla::kClearKeyWithProtectionQueryKeySystemName,
              key_system_size) == 0) {
    CK_LOGE("Enabling protection query on ClearKeyCDM instance!");
    clearKey->EnableProtectionQuery();
  }

  return clearKey;
}

const size_t TEST_READ_SIZE = 16 * 1024;

bool CanReadSome(cdm::PlatformFile aFile) {
  std::vector<uint8_t> data;
  data.resize(TEST_READ_SIZE);
#ifdef XP_WIN
  DWORD bytesRead = 0;
  return ReadFile(aFile, &data.front(), TEST_READ_SIZE, &bytesRead, nullptr) &&
         bytesRead > 0;
#else
  return read(aFile, &data.front(), TEST_READ_SIZE) > 0;
#endif
}

void ClosePlatformFile(cdm::PlatformFile aFile) {
#ifdef XP_WIN
  CloseHandle(aFile);
#else
  close(aFile);
#endif
}

static uint32_t NumExpectedHostFiles(const cdm::HostFile* aHostFiles,
                                     uint32_t aNumFiles) {
#if !defined(XP_WIN)
  // We expect 4 binaries: clearkey, libxul, plugin-container, and Firefox.
  return 4;
#else
  // Windows running x64 or x86 natively should also have 4 as above.
  // For Windows on ARM64, we run an x86 plugin-contianer process under
  // emulation, and so we expect one additional binary; the x86
  // xul.dll used by plugin-container.exe.
  bool i686underAArch64 = false;
  // Assume that we're running under x86 emulation on an aarch64 host if
  // one of the paths ends with the x86 plugin-container path we'd expect.
  const std::wstring plugincontainer = L"i686\\plugin-container.exe";
  for (uint32_t i = 0; i < aNumFiles; i++) {
    const cdm::HostFile& hostFile = aHostFiles[i];
    if (hostFile.file != cdm::kInvalidPlatformFile) {
      std::wstring path = hostFile.file_path;
      auto offset = path.find(plugincontainer);
      if (offset != std::string::npos &&
          offset == path.size() - plugincontainer.size()) {
        i686underAArch64 = true;
        break;
      }
    }
  }
  return i686underAArch64 ? 5 : 4;
#endif
}

CDM_API
bool VerifyCdmHost_0(const cdm::HostFile* aHostFiles, uint32_t aNumFiles) {
  // Check that we've received the expected number of host files.
  bool rv = (aNumFiles == NumExpectedHostFiles(aHostFiles, aNumFiles));
  // Verify that each binary is readable inside the sandbox,
  // and close the handle.
  for (uint32_t i = 0; i < aNumFiles; i++) {
    const cdm::HostFile& hostFile = aHostFiles[i];
    if (hostFile.file != cdm::kInvalidPlatformFile) {
      if (!CanReadSome(hostFile.file)) {
        rv = false;
      }
      ClosePlatformFile(hostFile.file);
    }
    if (hostFile.sig_file != cdm::kInvalidPlatformFile) {
      ClosePlatformFile(hostFile.sig_file);
    }
  }
  sCanReadHostVerificationFiles = rv;
  return rv;
}

}  // extern "C".
