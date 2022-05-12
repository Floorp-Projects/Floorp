/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineStream.h"

namespace mozilla {

RefPtr<MediaDataDecoder::DecodePromise> MFMediaEngineStream::Decode(
    MediaRawData* aSample) {
  // TODO : implement this by using MediaEngine API.
  return DecodePromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                                        __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> MFMediaEngineStream::Drain() {
  // TODO : implement this by using MediaEngine API.
  return DecodePromise::CreateAndReject(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                                        __func__);
}

RefPtr<MediaDataDecoder::FlushPromise> MFMediaEngineStream::Flush() {
  // TODO : implement this by using MediaEngine API.
  return MediaDataDecoder::FlushPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, RESULT_DETAIL("NotImpl")),
      __func__);
}

RefPtr<ShutdownPromise> MFMediaEngineStream::Shutdown() {
  // TODO : implement this by using MediaEngine API.
  return ShutdownPromise::CreateAndReject(false, __func__);
}

}  // namespace mozilla
