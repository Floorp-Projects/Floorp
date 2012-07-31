/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsWaveReader_h_)
#define nsWaveReader_h_

#include "nsBuiltinDecoderReader.h"

class nsBuiltinDecoder;
class nsTimeRanges;

class nsWaveReader : public nsBuiltinDecoderReader
{
public:
  nsWaveReader(nsBuiltinDecoder* aDecoder);
  ~nsWaveReader();

  virtual nsresult Init(nsBuiltinDecoderReader* aCloneDonor);
  virtual bool DecodeAudioData();
  virtual bool DecodeVideoFrame(bool &aKeyframeSkip,
                                  PRInt64 aTimeThreshold);

  virtual bool HasAudio()
  {
    return true;
  }

  virtual bool HasVideo()
  {
    return false;
  }

  virtual nsresult ReadMetadata(nsVideoInfo* aInfo,
                                nsHTMLMediaElement::MetadataTags** aTags);
  virtual nsresult Seek(PRInt64 aTime, PRInt64 aStartTime, PRInt64 aEndTime, PRInt64 aCurrentTime);
  virtual nsresult GetBuffered(nsTimeRanges* aBuffered, PRInt64 aStartTime);

  // To seek in a buffered range, we just have to seek the stream.
  virtual bool IsSeekableInBufferedRanges() {
    return true;
  }

private:
  bool ReadAll(char* aBuf, PRInt64 aSize, PRInt64* aBytesRead = nullptr);
  bool LoadRIFFChunk();
  bool ScanForwardUntil(PRUint32 aWantedChunk, PRUint32* aChunkSize);
  bool LoadFormatChunk();
  bool FindDataOffset();

  // Returns the number of seconds that aBytes represents based on the
  // current audio parameters.  e.g.  176400 bytes is 1 second at 16-bit
  // stereo 44.1kHz. The time is rounded to the nearest microsecond.
  double BytesToTime(PRInt64 aBytes) const;

  // Returns the number of bytes that aTime represents based on the current
  // audio parameters.  e.g.  1 second is 176400 bytes at 16-bit stereo
  // 44.1kHz.
  PRInt64 TimeToBytes(double aTime) const;

  // Rounds aBytes down to the nearest complete audio frame.  Assumes
  // beginning of byte range is already frame aligned by caller.
  PRInt64 RoundDownToFrame(PRInt64 aBytes) const;
  PRInt64 GetDataLength();
  PRInt64 GetPosition();

  /*
    Metadata extracted from the WAVE header.  Used to initialize the audio
    stream, and for byte<->time domain conversions.
  */

  // Number of samples per second.  Limited to range [100, 96000] in LoadFormatChunk.
  PRUint32 mSampleRate;

  // Number of channels.  Limited to range [1, 2] in LoadFormatChunk.
  PRUint32 mChannels;

  // Size of a single audio frame, which includes a sample for each channel
  // (interleaved).
  PRUint32 mFrameSize;

  // The sample format of the PCM data.
  nsAudioStream::SampleFormat mSampleFormat;

  // Size of PCM data stored in the WAVE as reported by the data chunk in
  // the media.
  PRInt64 mWaveLength;

  // Start offset of the PCM data in the media stream.  Extends mWaveLength
  // bytes.
  PRInt64 mWavePCMOffset;
};

#endif
