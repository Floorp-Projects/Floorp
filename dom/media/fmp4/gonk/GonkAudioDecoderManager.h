/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkAudioDecoderManager_h_)
#define GonkAudioDecoderManager_h_

#include "mozilla/RefPtr.h"
#include "MP4Reader.h"
#include "GonkMediaDataDecoder.h"

using namespace android;

namespace android {
struct MOZ_EXPORT ALooper;
class MOZ_EXPORT MediaBuffer;
} // namespace android

namespace mozilla {

class GonkAudioDecoderManager : public GonkDecoderManager {
typedef android::MediaCodecProxy MediaCodecProxy;
public:
  GonkAudioDecoderManager(MediaTaskQueue* aTaskQueue,
                          const AudioInfo& aConfig);
  ~GonkAudioDecoderManager();

  virtual android::sp<MediaCodecProxy> Init(MediaDataDecoderCallback* aCallback) override;

  virtual nsresult Output(int64_t aStreamOffset,
                          nsRefPtr<MediaData>& aOutput) override;

  virtual nsresult Flush() override;

protected:
  virtual bool PerformFormatSpecificProcess(MediaRawData* aSample) override;

  virtual status_t SendSampleToOMX(MediaRawData* aSample) override;

private:

  nsresult CreateAudioData(int64_t aStreamOffset,
                              AudioData** aOutData);

  void ReleaseAudioBuffer();
  // MediaCodedc's wrapper that performs the decoding.
  android::sp<MediaCodecProxy> mDecoder;

  const uint32_t mAudioChannels;
  const uint32_t mAudioRate;
  const uint32_t mAudioProfile;
  nsTArray<uint8_t> mUserData;
  bool mUseAdts;

  MediaDataDecoderCallback*  mReaderCallback;
  android::MediaBuffer* mAudioBuffer;
  android::sp<ALooper> mLooper;
};

} // namespace mozilla

#endif // GonkAudioDecoderManager_h_
