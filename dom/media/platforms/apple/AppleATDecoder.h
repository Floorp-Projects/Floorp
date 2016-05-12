/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppleATDecoder_h
#define mozilla_AppleATDecoder_h

#include <AudioToolbox/AudioToolbox.h>
#include "PlatformDecoderModule.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Vector.h"
#include "nsIThread.h"
#include "AudioConverter.h"

namespace mozilla {

class FlushableTaskQueue;
class MediaDataDecoderCallback;

class AppleATDecoder : public MediaDataDecoder {
public:
  AppleATDecoder(const AudioInfo& aConfig,
                 FlushableTaskQueue* aVideoTaskQueue,
                 MediaDataDecoderCallback* aCallback);
  virtual ~AppleATDecoder();

  RefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;

  const char* GetDescriptionName() const override
  {
    return "apple CoreMedia decoder";
  }

  // Callbacks also need access to the config.
  const AudioInfo& mConfig;

  // Use to extract magic cookie for HE-AAC detection.
  nsTArray<uint8_t> mMagicCookie;
  // Will be set to true should an error occurred while attempting to retrieve
  // the magic cookie property.
  bool mFileStreamError;

private:
  RefPtr<FlushableTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  AudioConverterRef mConverter;
  AudioStreamBasicDescription mOutputFormat;
  UInt32 mFormatID;
  AudioFileStreamID mStream;
  nsTArray<RefPtr<MediaRawData>> mQueuedSamples;
  UniquePtr<AudioConfig::ChannelLayout> mChannelLayout;
  UniquePtr<AudioConverter> mAudioConverter;
  Atomic<bool> mIsFlushing;

  void ProcessFlush();
  void SubmitSample(MediaRawData* aSample);
  nsresult DecodeSample(MediaRawData* aSample);
  nsresult GetInputAudioDescription(AudioStreamBasicDescription& aDesc,
                                    const nsTArray<uint8_t>& aExtraData);
  // Setup AudioConverter once all information required has been gathered.
  // Will return NS_ERROR_NOT_INITIALIZED if more data is required.
  nsresult SetupDecoder(MediaRawData* aSample);
  nsresult GetImplicitAACMagicCookie(const MediaRawData* aSample);
  nsresult SetupChannelLayout();
};

} // namespace mozilla

#endif // mozilla_AppleATDecoder_h
