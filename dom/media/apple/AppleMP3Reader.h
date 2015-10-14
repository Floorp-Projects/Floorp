/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AppleMP3Reader_h__
#define __AppleMP3Reader_h__

#include "MediaDecoderReader.h"
#include "MediaResource.h"
#include "MP3FrameParser.h"
#include "VideoUtils.h"

#include <AudioToolbox/AudioToolbox.h>

namespace mozilla {

class AppleMP3Reader : public MediaDecoderReader
{
public:
  explicit AppleMP3Reader(AbstractMediaDecoder *aDecoder);
  virtual ~AppleMP3Reader() override;

  virtual nsresult Init() override;

  nsresult PushDataToDemuxer();

  virtual bool DecodeAudioData() override;
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                int64_t aTimeThreshold) override;

  virtual bool HasAudio() override;
  virtual bool HasVideo() override;

  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags) override;

  virtual nsRefPtr<SeekPromise>
  Seek(int64_t aTime, int64_t aEndTime) override;

  void AudioSampleCallback(UInt32 aNumBytes,
                           UInt32 aNumPackets,
                           const void *aData,
                           AudioStreamPacketDescription *aPackets);

  void AudioMetadataCallback(AudioFileStreamID aFileStream,
                             AudioFileStreamPropertyID aPropertyID,
                             UInt32 *aFlags);

protected:
  virtual void NotifyDataArrivedInternal(uint32_t aLength,
                                         int64_t aOffset) override;
public:

  virtual bool IsMediaSeekable() override;

private:
  void SetupDecoder();
  nsresult Read(uint32_t *aNumBytes, char *aData);

  static OSStatus PassthroughInputDataCallback(AudioConverterRef aAudioConverter,
                                               UInt32 *aNumDataPackets,
                                               AudioBufferList *aData,
                                               AudioStreamPacketDescription **aPacketDesc,
                                               void *aUserData);

  // Initialisation has to be done in a callback, so we store the result here.
  bool mStreamReady;

  // Number of audio samples in an audio packet. Constant over all packets in a
  // stream.
  UInt32 mAudioFramesPerCompressedPacket;
  // Store the next audio frame to be played; so we can keep time when seeking.
  UInt64 mCurrentAudioFrame;
  UInt32 mAudioChannels;
  UInt32 mAudioSampleRate;

  uint64_t mDuration;

  AudioFileStreamID mAudioFileStream;
  AudioConverterRef mAudioConverter;

  MP3FrameParser mMP3FrameParser;

  MediaResourceIndex mResource;
  NotifyDataArrivedFilter mFilter;
};

} // namespace mozilla

#endif // __AppleMP3Reader_h__
