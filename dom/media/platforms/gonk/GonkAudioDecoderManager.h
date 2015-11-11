/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkAudioDecoderManager_h_)
#define GonkAudioDecoderManager_h_

#include "AudioCompactor.h"
#include "mozilla/RefPtr.h"
#include "GonkMediaDataDecoder.h"

using namespace android;

namespace android {
class MOZ_EXPORT MediaBuffer;
} // namespace android

namespace mozilla {

class GonkAudioDecoderManager : public GonkDecoderManager {
typedef android::MediaCodecProxy MediaCodecProxy;
public:
  GonkAudioDecoderManager(const AudioInfo& aConfig);

  virtual ~GonkAudioDecoderManager();

  RefPtr<InitPromise> Init() override;

  nsresult Output(int64_t aStreamOffset,
                          RefPtr<MediaData>& aOutput) override;

  void ProcessFlush() override;

private:
  bool InitMediaCodecProxy();

  nsresult CreateAudioData(MediaBuffer* aBuffer, int64_t aStreamOffset);

  uint32_t mAudioChannels;
  uint32_t mAudioRate;
  const uint32_t mAudioProfile;

  MediaQueue<AudioData> mAudioQueue;

  AudioCompactor mAudioCompactor;

};

} // namespace mozilla

#endif // GonkAudioDecoderManager_h_
