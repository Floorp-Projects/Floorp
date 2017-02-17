/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EMEVideoDecoder.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "MP4Decoder.h"
#include "MediaData.h"
#include "PlatformDecoderModule.h"
#include "VPXDecoder.h"
#include "mozilla/CDMProxy.h"

namespace mozilla {

EMEVideoDecoder::EMEVideoDecoder(CDMProxy* aProxy,
                                 const GMPVideoDecoderParams& aParams)
  : GMPVideoDecoder(GMPVideoDecoderParams(aParams))
  , mProxy(aProxy)
  , mDecryptorId(aProxy->GetDecryptorId())
{
}

void
EMEVideoDecoder::InitTags(nsTArray<nsCString>& aTags)
{
  VideoInfo config = GetConfig();
  if (MP4Decoder::IsH264(config.mMimeType)) {
    aTags.AppendElement(NS_LITERAL_CSTRING("h264"));
  } else if (VPXDecoder::IsVP8(config.mMimeType)) {
    aTags.AppendElement(NS_LITERAL_CSTRING("vp8"));
  } else if (VPXDecoder::IsVP9(config.mMimeType)) {
    aTags.AppendElement(NS_LITERAL_CSTRING("vp9"));
  }
  aTags.AppendElement(NS_ConvertUTF16toUTF8(mProxy->KeySystem()));
}

nsCString
EMEVideoDecoder::GetNodeId()
{
  return mProxy->GetNodeId();
}

GMPUniquePtr<GMPVideoEncodedFrame>
EMEVideoDecoder::CreateFrame(MediaRawData* aSample)
{
  GMPUniquePtr<GMPVideoEncodedFrame> frame =
    GMPVideoDecoder::CreateFrame(aSample);
  if (frame && aSample->mCrypto.mValid) {
    static_cast<gmp::GMPVideoEncodedFrameImpl*>(frame.get())
      ->InitCrypto(aSample->mCrypto);
  }
  return frame;
}

} // namespace mozilla
