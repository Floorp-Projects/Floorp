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
#include <stdio.h>
#include <string.h>
#include <vector>

#include "ClearKeyCDM.h"
#include "ClearKeySessionManager.h"
// This include is required in order for content_decryption_module to work
// on Unix systems.
#include "stddef.h"
#include "content_decryption_module.h"
#include "content_decryption_module_ext.h"

#if defined(XP_MACOSX) || defined(XP_LINUX)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef ENABLE_WMF
#include "WMFUtils.h"
#endif // ENABLE_WMF

extern "C" {

CDM_API
void INITIALIZE_CDM_MODULE() {

}

static bool sCanReadHostVerificationFiles = false;

CDM_API
void* CreateCdmInstance(int cdm_interface_version,
                        const char* key_system,
                        uint32_t key_system_size,
                        GetCdmHostFunc get_cdm_host_func,
                        void* user_data)
{

  CK_LOGE("ClearKey CreateCDMInstance");

#ifdef ENABLE_WMF
  if (!wmf::EnsureLibs()) {
    CK_LOGE("Required libraries were not found");
    return nullptr;
  }
#endif

  // Test that we're able to read the host files.
  if (!sCanReadHostVerificationFiles) {
    return nullptr;
  }

  cdm::Host_8* host = static_cast<cdm::Host_8*>(
    get_cdm_host_func(cdm_interface_version, user_data));
  ClearKeyCDM* clearKey = new ClearKeyCDM(host);

  CK_LOGE("Created ClearKeyCDM instance!");

  return clearKey;
}

const size_t TEST_READ_SIZE = 16 * 1024;

bool
CanReadSome(cdm::PlatformFile aFile)
{
  vector<uint8_t> data;
  data.resize(TEST_READ_SIZE);
#ifdef XP_WIN
  DWORD bytesRead = 0;
  return ReadFile(aFile, &data.front(), TEST_READ_SIZE, &bytesRead, nullptr) &&
         bytesRead > 0;
#elif defined(XP_MACOSX) || defined(XP_LINUX)
  return read(aFile, &data.front(), TEST_READ_SIZE) > 0;
#else
  return false;
#endif
}

void
ClosePlatformFile(cdm::PlatformFile aFile)
{
#ifdef XP_WIN
  CloseHandle(aFile);
#elif defined(XP_MACOSX) || defined(XP_LINUX)
  close(aFile);
#endif
}

CDM_API
bool
VerifyCdmHost_0(const cdm::HostFile* aHostFiles, uint32_t aNumFiles)
{
  // We expect 4 binaries: clearkey, libxul, plugin-container, and Firefox.
  bool rv = (aNumFiles == 4);
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

} // extern "C".
