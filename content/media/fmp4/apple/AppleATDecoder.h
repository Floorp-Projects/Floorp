/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppleATDecoder_h
#define mozilla_AppleATDecoder_h

#include <AudioToolbox/AudioToolbox.h>
#include "PlatformDecoderModule.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIThread.h"

namespace mozilla {

class MediaTaskQueue;
class MediaDataDecoderCallback;

class AppleATDecoder : public MediaDataDecoder {
public:
  AppleATDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                 MediaTaskQueue* aVideoTaskQueue,
                 MediaDataDecoderCallback* aCallback);
  ~AppleATDecoder();

  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Shutdown() MOZ_OVERRIDE;


  // Internal callbacks for the platform C api. Don't call externally.
  void MetadataCallback(AudioFileStreamID aFileStream,
                        AudioFileStreamPropertyID aPropertyID,
                        UInt32* aFlags);
  void SampleCallback(uint32_t aNumBytes,
                      uint32_t aNumPackets,
                      const void* aData,
                      AudioStreamPacketDescription* aPackets);

  // Callbacks also need access to the config.
  const mp4_demuxer::AudioDecoderConfig& mConfig;

private:
  RefPtr<MediaTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  AudioConverterRef mConverter;
  AudioFileStreamID mStream;
  uint64_t mCurrentAudioFrame;
  int64_t mSamplePosition;
  bool mHaveOutput;

  void SetupDecoder();
  void SubmitSample(nsAutoPtr<mp4_demuxer::MP4Sample> aSample);
};

} // namespace mozilla

#endif // mozilla_AppleATDecoder_h
