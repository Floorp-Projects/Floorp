/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WMFDATAENCODERUTILS_H_
#define WMFDATAENCODERUTILS_H_
#include <mfapi.h>
#include "EncoderConfig.h"
#include "AnnexB.h"
#include "H264.h"
#include "libyuv.h"
#include "mozilla/Logging.h"
#include "mozilla/mscom/EnsureMTA.h"
#include "WMF.h"

#define WMF_ENC_LOGD(arg, ...)                    \
  MOZ_LOG(                                        \
      mozilla::sPEMLog, mozilla::LogLevel::Debug, \
      ("WMFMediaDataEncoder(0x%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define WMF_ENC_LOGE(arg, ...)                    \
  MOZ_LOG(                                        \
      mozilla::sPEMLog, mozilla::LogLevel::Error, \
      ("WMFMediaDataEncoder(0x%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

class MFTEncoder;

extern LazyLogModule sPEMLog;

GUID CodecToSubtype(CodecType aCodec);

bool CanCreateWMFEncoder(CodecType aCodec);

already_AddRefed<MediaByteBuffer> ParseH264Parameters(
    nsTArray<uint8_t>& aHeader, const bool aAsAnnexB);
uint32_t GetProfile(H264_PROFILE aProfileLevel);

already_AddRefed<IMFMediaType> CreateInputType(EncoderConfig& aConfig);

already_AddRefed<IMFMediaType> CreateOutputType(EncoderConfig& aConfig);

HRESULT SetMediaTypes(RefPtr<MFTEncoder>& aEncoder, EncoderConfig& aConfig);

}  // namespace mozilla

#endif  // WMFDATAENCODERUTILS_H_
