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

namespace mozilla {

class FlushableMediaTaskQueue;
class MediaDataDecoderCallback;

class AppleATDecoder : public MediaDataDecoder {
public:
  AppleATDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                 FlushableMediaTaskQueue* aVideoTaskQueue,
                 MediaDataDecoderCallback* aCallback);
  virtual ~AppleATDecoder();

  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  // Callbacks also need access to the config.
  const mp4_demuxer::AudioDecoderConfig& mConfig;

  // Use to extract magic cookie for HE-AAC detection.
  nsTArray<uint8_t> mMagicCookie;
  // Will be set to true should an error occurred while attempting to retrieve
  // the magic cookie property.
  bool mFileStreamError;

private:
  nsRefPtr<FlushableMediaTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  AudioConverterRef mConverter;
  AudioStreamBasicDescription mOutputFormat;
  UInt32 mFormatID;
  AudioFileStreamID mStream;
  nsTArray<nsAutoPtr<mp4_demuxer::MP4Sample>> mQueuedSamples;

  void SubmitSample(nsAutoPtr<mp4_demuxer::MP4Sample> aSample);
  nsresult DecodeSample(mp4_demuxer::MP4Sample* aSample);
  nsresult GetInputAudioDescription(AudioStreamBasicDescription& aDesc,
                                    const nsTArray<uint8_t>& aExtraData);
  // Setup AudioConverter once all information required has been gathered.
  // Will return NS_ERROR_NOT_INITIALIZED if more data is required.
  nsresult SetupDecoder(mp4_demuxer::MP4Sample* aSample);
  nsresult GetImplicitAACMagicCookie(const mp4_demuxer::MP4Sample* aSample);
};

} // namespace mozilla

#endif // mozilla_AppleATDecoder_h
