/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineAudioStream.h"

namespace mozilla {

RefPtr<MediaDataDecoder::InitPromise> MFMediaEngineAudioStream::Init() {
  // TODO : implement this by using MediaEngine API.
  return InitPromise::CreateAndResolve(TrackType::kAudioTrack, __func__);
}

nsCString MFMediaEngineAudioStream::GetDescriptionName() const {
  return "media engine audio stream"_ns;
}

}  // namespace mozilla
