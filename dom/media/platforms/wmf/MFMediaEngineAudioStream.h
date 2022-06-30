/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEAUDIOSTREAM_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEAUDIOSTREAM_H

#include "MFMediaEngineStream.h"

namespace mozilla {

class MFMediaEngineAudioStream final : public MFMediaEngineStream {
 public:
  explicit MFMediaEngineAudioStream(const CreateDecoderParams& aParam) {}

  RefPtr<InitPromise> Init() override;
  nsCString GetDescriptionName() const override;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEAUDIOSTREAM_H
