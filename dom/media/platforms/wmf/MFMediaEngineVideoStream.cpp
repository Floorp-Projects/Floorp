/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineVideoStream.h"

namespace mozilla {

RefPtr<MediaDataDecoder::InitPromise> MFMediaEngineVideoStream::Init() {
  // TODO : implement this by using MediaEngine API.
  return InitPromise::CreateAndResolve(TrackType::kVideoTrack, __func__);
}

nsCString MFMediaEngineVideoStream::GetDescriptionName() const {
  return "media engine video stream"_ns;
}

MediaDataDecoder::ConversionRequired MFMediaEngineVideoStream::NeedsConversion()
    const {
  // TODO : check video type
  return ConversionRequired::kNeedAnnexB;
}

}  // namespace mozilla
