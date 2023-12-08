/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoEncoder.h"

namespace mozilla {

RefPtr<MediaDataEncoder::InitPromise> FFmpegVideoEncoder<LIBAV_VER>::Init() {
  return InitPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegVideoEncoder<LIBAV_VER>::Encode(
    const MediaData* aSample) {
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegVideoEncoder<LIBAV_VER>::Drain() {
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<ShutdownPromise> FFmpegVideoEncoder<LIBAV_VER>::Shutdown() {
  return ShutdownPromise::CreateAndReject(false, __func__);
}

RefPtr<GenericPromise> FFmpegVideoEncoder<LIBAV_VER>::SetBitrate(
    Rate aBitsPerSec) {
  return GenericPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

nsCString FFmpegVideoEncoder<LIBAV_VER>::GetDescriptionName() const {
  return ""_ns;
}

}  // namespace mozilla
