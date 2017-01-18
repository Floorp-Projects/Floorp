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

#include "ClearKeyCDM.h"
#include "ClearKeySessionManager.h"
// This include is required in order for content_decryption_module to work
// on Unix systems.
#include "stddef.h"
#include "content_decryption_module.h"

#ifdef ENABLE_WMF
#include "WMFUtils.h"
#endif // ENABLE_WMF

extern "C" {

CDM_EXPORT
void INITIALIZE_CDM_MODULE() {

}

CDM_EXPORT
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

  cdm::Host_8* host = static_cast<cdm::Host_8*>(
    get_cdm_host_func(cdm_interface_version, user_data));
  ClearKeyCDM* clearKey = new ClearKeyCDM(host);

  CK_LOGE("Created ClearKeyCDM instance!");

  return clearKey;
}
}
