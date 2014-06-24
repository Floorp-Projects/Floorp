/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WaveReader_h_)
#define WaveReader_h_

#include "MediaDecoderReader.h"
#include "mozilla/dom/HTMLMediaElement.h"

namespace mozilla {
namespace dom {
class TimeRanges;
}
}

namespace mozilla {

class WaveReader : public MediaDecoderReader
{
public:
  WaveReader(AbstractMediaDecoder* aDecoder);
  ~WaveReader();

  virtual nsresult Init(MediaDecoderReader* aCloneDonor);
  virtual bool DecodeAudioData();
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                  int64_t aTimeThreshold);

  virtual bool HasAudio()
  {
    return true;
  }

  virtual bool HasVideo()
  {
    return false;
  }

  virtual nsresult ReadMetadata(MediaInfo* aInfo,
                                MetadataTags** aTags);
  virtual nsresult Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime, int64_t aCurrentTime);
  virtual nsresult GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime);

  // To seek in a buffered range, we just have to seek the stream.
  virtual bool IsSeekableInBufferedRanges() {
    return true;
  }

  virtual bool IsMediaSeekable() MOZ_OVERRIDE;

private:
  bool ReadAll(char* aBuf, int64_t aSize, int64_t* aBytesRead = nullptr);
  bool LoadRIFFChunk();
  bool GetNextChunk(uint32_t* aChunk, uint32_t* aChunkSize);
  bool LoadFormatChunk(uint32_t aChunkSize);
  bool FindDataOffset(uint32_t aChunkSize);
  bool LoadListChunk(uint32_t aChunkSize, nsAutoPtr<dom::HTMLMediaElement::MetadataTags> &aTags);
  bool LoadAllChunks(nsAutoPtr<dom::HTMLMediaElement::MetadataTags> &aTags);

  // Returns the number of seconds that aBytes represents based on the
  // current audio parameters.  e.g.  176400 bytes is 1 second at 16-bit
  // stereo 44.1kHz. The time is rounded to the nearest microsecond.
  double BytesToTime(int64_t aBytes) const;

  // Returns the number of bytes that aTime represents based on the current
  // audio parameters.  e.g.  1 second is 176400 bytes at 16-bit stereo
  // 44.1kHz.
  int64_t TimeToBytes(double aTime) const;

  // Rounds aBytes down to the nearest complete audio frame.  Assumes
  // beginning of byte range is already frame aligned by caller.
  int64_t RoundDownToFrame(int64_t aBytes) const;
  int64_t GetDataLength();
  int64_t GetPosition();

  /*
    Metadata extracted from the WAVE header.  Used to initialize the audio
    stream, and for byte<->time domain conversions.
  */

  // Number of samples per second.  Limited to range [100, 96000] in LoadFormatChunk.
  uint32_t mSampleRate;

  // Number of channels.  Limited to range [1, 2] in LoadFormatChunk.
  uint32_t mChannels;

  // Size of a single audio frame, which includes a sample for each channel
  // (interleaved).
  uint32_t mFrameSize;

  // The sample format of the PCM data. AudioStream::SampleFormat doesn't
  // support U8.
  enum {
    FORMAT_U8,
    FORMAT_S16
  } mSampleFormat;

  // Size of PCM data stored in the WAVE as reported by the data chunk in
  // the media.
  int64_t mWaveLength;

  // Start offset of the PCM data in the media stream.  Extends mWaveLength
  // bytes.
  int64_t mWavePCMOffset;
};

} // namespace mozilla

#endif
