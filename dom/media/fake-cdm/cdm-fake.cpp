/*!
 * \copy
 *     Copyright (c)  2009-2014, Cisco Systems
 *     Copyright (c)  2014, Mozilla
 *     All rights reserved.
 *
 *     Redistribution and use in source and binary forms, with or without
 *     modification, are permitted provided that the following conditions
 *     are met:
 *
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *
 *        * Redistributions in binary form must reproduce the above copyright
 *          notice, this list of conditions and the following disclaimer in
 *          the documentation and/or other materials provided with the
 *          distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *     "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *     LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *     FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *     COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *     BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *     CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *     ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *     POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *************************************************************************************
 */

#include "stddef.h"
#include "cdm-test-decryptor.h"
#include "content_decryption_module.h"
#include "content_decryption_module_ext.h"

extern "C" {

CDM_API
void INITIALIZE_CDM_MODULE() {

}

CDM_API
void* CreateCdmInstance(int cdm_interface_version,
                        const char* key_system,
                        uint32_t key_system_size,
                        GetCdmHostFunc get_cdm_host_func,
                        void* user_data)
{
  if (cdm_interface_version != cdm::ContentDecryptionModule_8::kVersion) {
    // Only support CDM version 8 currently.
    return nullptr;
  }
  cdm::Host_8* host = static_cast<cdm::Host_8*>(
    get_cdm_host_func(cdm_interface_version, user_data));
  return new FakeDecryptor(host);
}


CDM_API
bool
VerifyCdmHost_0(const cdm::HostFile* aHostFiles, uint32_t aNumFiles)
{
  return true;
}

} // extern "C"
