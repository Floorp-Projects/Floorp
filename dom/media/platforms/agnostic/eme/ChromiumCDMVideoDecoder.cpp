/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMVideoDecoder.h"
#include "GMPService.h"
#include "GMPVideoDecoder.h"

namespace mozilla {

ChromiumCDMVideoDecoder::ChromiumCDMVideoDecoder(
  const GMPVideoDecoderParams& aParams,
  CDMProxy* aCDMProxy)
  : mConfig(aParams.mConfig)
  , mCrashHelper(aParams.mCrashHelper)
  , mGMPThread(GetGMPAbstractThread())
  , mImageContainer(aParams.mImageContainer)
{
}

ChromiumCDMVideoDecoder::~ChromiumCDMVideoDecoder()
{
}

RefPtr<MediaDataDecoder::InitPromise>
ChromiumCDMVideoDecoder::Init()
{
  return InitPromise::CreateAndResolve(TrackInfo::kUndefinedTrack, __func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
ChromiumCDMVideoDecoder::Decode(MediaRawData* aSample)
{
  return DecodePromise::CreateAndReject(
    MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, RESULT_DETAIL("Unimplemented")),
    __func__);
}

RefPtr<MediaDataDecoder::FlushPromise>
ChromiumCDMVideoDecoder::Flush()
{
  return FlushPromise::CreateAndReject(
    MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, RESULT_DETAIL("Unimplemented")),
    __func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
ChromiumCDMVideoDecoder::Drain()
{
  return DecodePromise::CreateAndReject(
    MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, RESULT_DETAIL("Unimplemented")),
    __func__);
}

RefPtr<ShutdownPromise>
ChromiumCDMVideoDecoder::Shutdown()
{
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

} // namespace mozilla
