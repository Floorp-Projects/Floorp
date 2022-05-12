/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINESTREAM_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINESTREAM_H

#include "PlatformDecoderModule.h"

namespace mozilla {

/**
 * MFMediaEngineStream represents a track which would be responsible to provide
 * encoded data into the media engine. The media engine can access this stream
 * by the presentation descriptor which was acquired from the custom media
 * source.
 */
// TODO : Inherit IMFMediaStream in following patches
class MFMediaEngineStream : public MediaDataDecoder {
 public:
  MFMediaEngineStream() = default;

  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINESTREAM_H
