/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H

#include "MFMediaEngineStream.h"

namespace mozilla {

class MFMediaEngineVideoStream final : public MFMediaEngineStream {
 public:
  explicit MFMediaEngineVideoStream(const CreateDecoderParams& aParam) {}

  RefPtr<InitPromise> Init() override;
  nsCString GetDescriptionName() const override;
  ConversionRequired NeedsConversion() const override;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEVIDEOSTREAM_H
