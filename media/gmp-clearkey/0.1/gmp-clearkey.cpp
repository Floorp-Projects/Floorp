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

#include "ClearKeyAsyncShutdown.h"
#include "ClearKeySessionManager.h"
#include "gmp-api/gmp-async-shutdown.h"
#include "gmp-api/gmp-decryption.h"
#include "gmp-api/gmp-platform.h"

#if defined(ENABLE_WMF)
#include "WMFUtils.h"
#include "VideoDecoder.h"
#endif

#if defined(WIN32)
#define GMP_EXPORT __declspec(dllexport)
#else
#define GMP_EXPORT __attribute__((visibility("default")))
#endif

static GMPPlatformAPI* sPlatform = nullptr;
GMPPlatformAPI*
GetPlatform()
{
  return sPlatform;
}

extern "C" {

GMP_EXPORT GMPErr
GMPInit(GMPPlatformAPI* aPlatformAPI)
{
  sPlatform = aPlatformAPI;
  return GMPNoErr;
}

GMP_EXPORT GMPErr
GMPGetAPI(const char* aApiName, void* aHostAPI, void** aPluginAPI)
{
  CK_LOGD("ClearKey GMPGetAPI |%s|", aApiName);
  assert(!*aPluginAPI);

  if (!strcmp(aApiName, GMP_API_DECRYPTOR)) {
    *aPluginAPI = new ClearKeySessionManager();
  }
#if defined(ENABLE_WMF)
 else if (!strcmp(aApiName, GMP_API_VIDEO_DECODER) &&
             wmf::EnsureLibs()) {
    *aPluginAPI = new VideoDecoder(static_cast<GMPVideoHost*>(aHostAPI));
  }
#endif
  else if (!strcmp(aApiName, GMP_API_ASYNC_SHUTDOWN)) {
    *aPluginAPI = new ClearKeyAsyncShutdown(static_cast<GMPAsyncShutdownHost*> (aHostAPI));
  } else {
    CK_LOGE("GMPGetAPI couldn't resolve API name |%s|\n", aApiName);
  }

  return *aPluginAPI ? GMPNoErr : GMPNotImplementedErr;
}

GMP_EXPORT GMPErr
GMPShutdown(void)
{
  CK_LOGD("ClearKey GMPShutdown");
  return GMPNoErr;
}

}
