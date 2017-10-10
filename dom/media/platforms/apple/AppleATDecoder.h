/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppleATDecoder_h
#define mozilla_AppleATDecoder_h

#include <AudioToolbox/AudioToolbox.h>
#include "PlatformDecoderModule.h"
#include "mozilla/Vector.h"
#include "nsIThread.h"
#include "AudioConverter.h"

namespace mozilla {

class TaskQueue;

DDLoggedTypeDeclNameAndBase(AppleATDecoder, MediaDataDecoder);

class AppleATDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<AppleATDecoder>
{
public:
  AppleATDecoder(const AudioInfo& aConfig,
                 TaskQueue* aTaskQueue);
  ~AppleATDecoder();

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;

  nsCString GetDescriptionName() const override
  {
    return NS_LITERAL_CSTRING("apple coremedia decoder");
  }

  // Callbacks also need access to the config.
  const AudioInfo& mConfig;

  // Use to extract magic cookie for HE-AAC detection.
  nsTArray<uint8_t> mMagicCookie;
  // Will be set to true should an error occurred while attempting to retrieve
  // the magic cookie property.
  bool mFileStreamError;

  const RefPtr<TaskQueue> mTaskQueue;

private:
  AudioConverterRef mConverter;
  AudioStreamBasicDescription mOutputFormat;
  UInt32 mFormatID;
  AudioFileStreamID mStream;
  nsTArray<RefPtr<MediaRawData>> mQueuedSamples;
  UniquePtr<AudioConfig::ChannelLayout> mChannelLayout;
  UniquePtr<AudioConverter> mAudioConverter;
  DecodedData mDecodedSamples;

  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);
  RefPtr<FlushPromise> ProcessFlush();
  void ProcessShutdown();
  MediaResult DecodeSample(MediaRawData* aSample);
  MediaResult GetInputAudioDescription(AudioStreamBasicDescription& aDesc,
                                       const nsTArray<uint8_t>& aExtraData);
  // Setup AudioConverter once all information required has been gathered.
  // Will return NS_ERROR_NOT_INITIALIZED if more data is required.
  MediaResult SetupDecoder(MediaRawData* aSample);
  nsresult GetImplicitAACMagicCookie(const MediaRawData* aSample);
  nsresult SetupChannelLayout();
  uint32_t mParsedFramesForAACMagicCookie;
  bool mErrored;
};

} // namespace mozilla

#endif // mozilla_AppleATDecoder_h
