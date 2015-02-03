/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ClearKeySessionManager.h"
#if defined(ENABLE_WMF)
#include "WMFUtils.h"
#endif

#include "gmp-api/gmp-decryption.h"
#include "gmp-api/gmp-platform.h"
#include "mozilla/Attributes.h"

static GMPPlatformAPI* sPlatform = nullptr;
GMPPlatformAPI*
GetPlatform()
{
  return sPlatform;
}

extern "C" {

MOZ_EXPORT GMPErr
GMPInit(GMPPlatformAPI* aPlatformAPI)
{
  sPlatform = aPlatformAPI;
  return GMPNoErr;
}

MOZ_EXPORT GMPErr
GMPGetAPI(const char* aApiName, void* aHostAPI, void** aPluginAPI)
{
  CK_LOGD("ClearKey GMPGetAPI |%s|", aApiName);
  assert(!*aPluginAPI);

  if (!strcmp(aApiName, GMP_API_DECRYPTOR)) {
    *aPluginAPI = new ClearKeySessionManager();
  }
#if defined(ENABLE_WMF)
  else if (wmf::EnsureLibs()) {
    if (!strcmp(aApiName, GMP_API_AUDIO_DECODER)) {
      *aPluginAPI = new AudioDecoder(static_cast<GMPAudioHost*>(aHostAPI));
    } else if (!strcmp(aApiName, GMP_API_VIDEO_DECODER)) {
      *aPluginAPI = new VideoDecoder(static_cast<GMPVideoHost*>(aHostAPI));
    }
  }
#endif
  else {
    CK_LOGE("GMPGetAPI couldn't resolve API name |%s|\n", aApiName);
  }

  return *aPluginAPI ? GMPNoErr : GMPNotImplementedErr;
}

MOZ_EXPORT GMPErr
GMPShutdown(void)
{
  CK_LOGD("ClearKey GMPShutdown");
  return GMPNoErr;
}

}
