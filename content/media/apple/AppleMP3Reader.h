/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AppleMP3Reader_h__
#define __AppleMP3Reader_h__

#include "MediaDecoderReader.h"
#include "MP3FrameParser.h"
#include "VideoUtils.h"

#include <AudioToolbox/AudioToolbox.h>

namespace mozilla {

class AppleMP3Reader : public MediaDecoderReader
{
public:
  explicit AppleMP3Reader(AbstractMediaDecoder *aDecoder);
  virtual ~AppleMP3Reader() MOZ_OVERRIDE;

  virtual nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE;

  nsresult PushDataToDemuxer();

  virtual bool DecodeAudioData() MOZ_OVERRIDE;
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                int64_t aTimeThreshold) MOZ_OVERRIDE;

  virtual bool HasAudio() MOZ_OVERRIDE;
  virtual bool HasVideo() MOZ_OVERRIDE;

  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags) MOZ_OVERRIDE;

  virtual nsresult Seek(int64_t aTime,
                        int64_t aStartTime,
                        int64_t aEndTime,
                        int64_t aCurrentTime) MOZ_OVERRIDE;

  void AudioSampleCallback(UInt32 aNumBytes,
                           UInt32 aNumPackets,
                           const void *aData,
                           AudioStreamPacketDescription *aPackets);

  void AudioMetadataCallback(AudioFileStreamID aFileStream,
                             AudioFileStreamPropertyID aPropertyID,
                             UInt32 *aFlags);

  virtual void NotifyDataArrived(const char* aBuffer,
                                 uint32_t aLength,
                                 int64_t aOffset) MOZ_OVERRIDE;

  virtual bool IsMediaSeekable() MOZ_OVERRIDE;

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
};

} // namespace mozilla

#endif // __AppleMP3Reader_h__
