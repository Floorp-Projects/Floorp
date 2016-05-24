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

#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>

#include "gmp-platform.h"
#include "gmp-video-decode.h"

#if defined(GMP_FAKE_SUPPORT_DECRYPT)
#include "gmp-decryption.h"
#include "gmp-test-decryptor.h"
#include "gmp-test-storage.h"
#endif

#if defined(_MSC_VER)
#define PUBLIC_FUNC __declspec(dllexport)
#else
#define PUBLIC_FUNC
#endif

GMPPlatformAPI* g_platform_api = NULL;

extern "C" {

  PUBLIC_FUNC GMPErr
  GMPInit (GMPPlatformAPI* aPlatformAPI) {
    g_platform_api = aPlatformAPI;
    return GMPNoErr;
  }

  PUBLIC_FUNC GMPErr
  GMPGetAPI (const char* aApiName, void* aHostAPI, void** aPluginApi) {
    if (!strcmp (aApiName, GMP_API_VIDEO_DECODER)) {
      // Note: Deliberately advertise in our .info file that we support
      // video-decode, but we fail the "get" call here to simulate what
      // happens when decoder init fails.
      return GMPGenericErr;
#if defined(GMP_FAKE_SUPPORT_DECRYPT)
    } else if (!strcmp (aApiName, GMP_API_DECRYPTOR_BACKWARDS_COMPAT)) {
      *aPluginApi = new FakeDecryptor(static_cast<GMPDecryptorHost*> (aHostAPI));
      return GMPNoErr;
    } else if (!strcmp (aApiName, GMP_API_ASYNC_SHUTDOWN)) {
      *aPluginApi = new TestAsyncShutdown(static_cast<GMPAsyncShutdownHost*> (aHostAPI));
      return GMPNoErr;
#endif
    }
    return GMPGenericErr;
  }

  PUBLIC_FUNC void
  GMPShutdown (void) {
    g_platform_api = NULL;
  }

#if defined(GMP_FAKE_SUPPORT_DECRYPT)
  PUBLIC_FUNC void
  GMPSetNodeId(const char* aNodeId, uint32_t aLength) {
    FakeDecryptor::SetNodeId(aNodeId, aLength);
  }
#endif

} // extern "C"
