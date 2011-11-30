/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Matthew Gregan <kinetik@flim.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsError.h"
#include "nsBuiltinDecoderStateMachine.h"
#include "nsBuiltinDecoder.h"
#include "nsMediaStream.h"
#include "nsWaveReader.h"
#include "nsTimeRanges.h"
#include "VideoUtils.h"

using namespace mozilla;

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#ifdef PR_LOGGING
extern PRLogModuleInfo* gBuiltinDecoderLog;
#define LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#ifdef SEEK_LOGGING
#define SEEK_LOG(type, msg) PR_LOG(gBuiltinDecoderLog, type, msg)
#else
#define SEEK_LOG(type, msg)
#endif
#else
#define LOG(type, msg)
#define SEEK_LOG(type, msg)
#endif

// Magic values that identify RIFF chunks we're interested in.
static const PRUint32 RIFF_CHUNK_MAGIC = 0x52494646;
static const PRUint32 WAVE_CHUNK_MAGIC = 0x57415645;
static const PRUint32 FRMT_CHUNK_MAGIC = 0x666d7420;
static const PRUint32 DATA_CHUNK_MAGIC = 0x64617461;

// Size of RIFF chunk header.  4 byte chunk header type and 4 byte size field.
static const PRUint16 RIFF_CHUNK_HEADER_SIZE = 8;

// Size of RIFF header.  RIFF chunk and 4 byte RIFF type.
static const PRUint16 RIFF_INITIAL_SIZE = RIFF_CHUNK_HEADER_SIZE + 4;

// Size of required part of format chunk.  Actual format chunks may be
// extended (for non-PCM encodings), but we skip any extended data.
static const PRUint16 WAVE_FORMAT_CHUNK_SIZE = 16;

// PCM encoding type from format chunk.  Linear PCM is the only encoding
// supported by nsAudioStream.
static const PRUint16 WAVE_FORMAT_ENCODING_PCM = 1;

// Maximum number of channels supported
static const PRUint8 MAX_CHANNELS = 2;

namespace {
  PRUint32
  ReadUint32BE(const char** aBuffer)
  {
    PRUint32 result =
      PRUint8((*aBuffer)[0]) << 24 |
      PRUint8((*aBuffer)[1]) << 16 |
      PRUint8((*aBuffer)[2]) << 8 |
      PRUint8((*aBuffer)[3]);
    *aBuffer += sizeof(PRUint32);
    return result;
  }

  PRUint32
  ReadUint32LE(const char** aBuffer)
  {
    PRUint32 result =
      PRUint8((*aBuffer)[3]) << 24 |
      PRUint8((*aBuffer)[2]) << 16 |
      PRUint8((*aBuffer)[1]) << 8 |
      PRUint8((*aBuffer)[0]);
    *aBuffer += sizeof(PRUint32);
    return result;
  }

  PRUint16
  ReadUint16LE(const char** aBuffer)
  {
    PRUint16 result =
      PRUint8((*aBuffer)[1]) << 8 |
      PRUint8((*aBuffer)[0]) << 0;
    *aBuffer += sizeof(PRUint16);
    return result;
  }

  PRInt16
  ReadInt16LE(const char** aBuffer)
  {
    return static_cast<PRInt16>(ReadUint16LE(aBuffer));
  }

  PRUint8
  ReadUint8(const char** aBuffer)
  {
    PRUint8 result = PRUint8((*aBuffer)[0]);
    *aBuffer += sizeof(PRUint8);
    return result;
  }
}

nsWaveReader::nsWaveReader(nsBuiltinDecoder* aDecoder)
  : nsBuiltinDecoderReader(aDecoder)
{
  MOZ_COUNT_CTOR(nsWaveReader);
}

nsWaveReader::~nsWaveReader()
{
  MOZ_COUNT_DTOR(nsWaveReader);
}

nsresult nsWaveReader::Init(nsBuiltinDecoderReader* aCloneDonor)
{
  return NS_OK;
}

nsresult nsWaveReader::ReadMetadata(nsVideoInfo* aInfo)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  bool loaded = LoadRIFFChunk() && LoadFormatChunk() && FindDataOffset();
  if (!loaded) {
    return NS_ERROR_FAILURE;
  }

  mInfo.mHasAudio = true;
  mInfo.mHasVideo = false;
  mInfo.mAudioRate = mSampleRate;
  mInfo.mAudioChannels = mChannels;

  *aInfo = mInfo;

  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  mDecoder->GetStateMachine()->SetDuration(
    static_cast<PRInt64>(BytesToTime(GetDataLength()) * USECS_PER_S));

  return NS_OK;
}

bool nsWaveReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  PRInt64 pos = GetPosition() - mWavePCMOffset;
  PRInt64 len = GetDataLength();
  PRInt64 remaining = len - pos;
  NS_ASSERTION(remaining >= 0, "Current wave position is greater than wave file length");

  static const PRInt64 BLOCK_SIZE = 4096;
  PRInt64 readSize = NS_MIN(BLOCK_SIZE, remaining);
  PRInt64 frames = readSize / mFrameSize;

  PR_STATIC_ASSERT(PRUint64(BLOCK_SIZE) < UINT_MAX / sizeof(AudioDataValue) / MAX_CHANNELS);
  const size_t bufferSize = static_cast<size_t>(frames * mChannels);
  nsAutoArrayPtr<AudioDataValue> sampleBuffer(new AudioDataValue[bufferSize]);

  PR_STATIC_ASSERT(PRUint64(BLOCK_SIZE) < UINT_MAX / sizeof(char));
  nsAutoArrayPtr<char> dataBuffer(new char[static_cast<size_t>(readSize)]);

  if (!ReadAll(dataBuffer, readSize)) {
    mAudioQueue.Finish();
    return false;
  }

  // convert data to samples
  const char* d = dataBuffer.get();
  AudioDataValue* s = sampleBuffer.get();
  for (int i = 0; i < frames; ++i) {
    for (unsigned int j = 0; j < mChannels; ++j) {
      if (mSampleFormat == nsAudioStream::FORMAT_U8) {
        PRUint8 v =  ReadUint8(&d);
#if defined(MOZ_SAMPLE_TYPE_S16LE)
        *s++ = (v * (1.F/PR_UINT8_MAX)) * PR_UINT16_MAX + PR_INT16_MIN;
#elif defined(MOZ_SAMPLE_TYPE_FLOAT32)
        *s++ = (v * (1.F/PR_UINT8_MAX)) * 2.F - 1.F;
#endif
      }
      else if (mSampleFormat == nsAudioStream::FORMAT_S16_LE) {
        PRInt16 v =  ReadInt16LE(&d);
#if defined(MOZ_SAMPLE_TYPE_S16LE)
        *s++ = v;
#elif defined(MOZ_SAMPLE_TYPE_FLOAT32)
        *s++ = (PRInt32(v) - PR_INT16_MIN) / float(PR_UINT16_MAX) * 2.F - 1.F;
#endif
      }
    }
  }

  double posTime = BytesToTime(pos);
  double readSizeTime = BytesToTime(readSize);
  NS_ASSERTION(posTime <= PR_INT64_MAX / USECS_PER_S, "posTime overflow");
  NS_ASSERTION(readSizeTime <= PR_INT64_MAX / USECS_PER_S, "readSizeTime overflow");
  NS_ASSERTION(frames < PR_INT32_MAX, "frames overflow");

  mAudioQueue.Push(new AudioData(pos,
                                 static_cast<PRInt64>(posTime * USECS_PER_S),
                                 static_cast<PRInt64>(readSizeTime * USECS_PER_S),
                                 static_cast<PRInt32>(frames),
                                 sampleBuffer.forget(),
                                 mChannels));

  return true;
}

bool nsWaveReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                      PRInt64 aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  return false;
}

nsresult nsWaveReader::Seek(PRInt64 aTarget, PRInt64 aStartTime, PRInt64 aEndTime, PRInt64 aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  LOG(PR_LOG_DEBUG, ("%p About to seek to %lld", mDecoder, aTarget));
  if (NS_FAILED(ResetDecode())) {
    return NS_ERROR_FAILURE;
  }
  double d = BytesToTime(GetDataLength());
  NS_ASSERTION(d < PR_INT64_MAX / USECS_PER_S, "Duration overflow"); 
  PRInt64 duration = static_cast<PRInt64>(d * USECS_PER_S);
  double seekTime = NS_MIN(aTarget, duration) / static_cast<double>(USECS_PER_S);
  PRInt64 position = RoundDownToFrame(static_cast<PRInt64>(TimeToBytes(seekTime)));
  NS_ASSERTION(PR_INT64_MAX - mWavePCMOffset > position, "Integer overflow during wave seek");
  position += mWavePCMOffset;
  return mDecoder->GetStream()->Seek(nsISeekableStream::NS_SEEK_SET, position);
}

static double RoundToUsecs(double aSeconds) {
  return floor(aSeconds * USECS_PER_S) / USECS_PER_S;
}

nsresult nsWaveReader::GetBuffered(nsTimeRanges* aBuffered, PRInt64 aStartTime)
{
  PRInt64 startOffset = mDecoder->GetStream()->GetNextCachedData(mWavePCMOffset);
  while (startOffset >= 0) {
    PRInt64 endOffset = mDecoder->GetStream()->GetCachedDataEnd(startOffset);
    // Bytes [startOffset..endOffset] are cached.
    NS_ASSERTION(startOffset >= mWavePCMOffset, "Integer underflow in GetBuffered");
    NS_ASSERTION(endOffset >= mWavePCMOffset, "Integer underflow in GetBuffered");

    // We need to round the buffered ranges' times to microseconds so that they
    // have the same precision as the currentTime and duration attribute on 
    // the media element.
    aBuffered->Add(RoundToUsecs(BytesToTime(startOffset - mWavePCMOffset)),
                   RoundToUsecs(BytesToTime(endOffset - mWavePCMOffset)));
    startOffset = mDecoder->GetStream()->GetNextCachedData(endOffset);
  }
  return NS_OK;
}

bool
nsWaveReader::ReadAll(char* aBuf, PRInt64 aSize, PRInt64* aBytesRead)
{
  PRUint32 got = 0;
  if (aBytesRead) {
    *aBytesRead = 0;
  }
  do {
    PRUint32 read = 0;
    if (NS_FAILED(mDecoder->GetStream()->Read(aBuf + got, PRUint32(aSize - got), &read))) {
      NS_WARNING("Stream read failed");
      return false;
    }
    if (read == 0) {
      return false;
    }
    mDecoder->NotifyBytesConsumed(read);
    got += read;
    if (aBytesRead) {
      *aBytesRead = got;
    }
  } while (got != aSize);
  return true;
}

bool
nsWaveReader::LoadRIFFChunk()
{
  char riffHeader[RIFF_INITIAL_SIZE];
  const char* p = riffHeader;

  NS_ABORT_IF_FALSE(mDecoder->GetStream()->Tell() == 0,
                    "LoadRIFFChunk called when stream in invalid state");

  if (!ReadAll(riffHeader, sizeof(riffHeader))) {
    return false;
  }

  PR_STATIC_ASSERT(sizeof(PRUint32) * 2 <= RIFF_INITIAL_SIZE);
  if (ReadUint32BE(&p) != RIFF_CHUNK_MAGIC) {
    NS_WARNING("Stream data not in RIFF format");
    return false;
  }

  // Skip over RIFF size field.
  p += 4;

  if (ReadUint32BE(&p) != WAVE_CHUNK_MAGIC) {
    NS_WARNING("Expected WAVE chunk");
    return false;
  }

  return true;
}

bool
nsWaveReader::ScanForwardUntil(PRUint32 aWantedChunk, PRUint32* aChunkSize)
{
  NS_ABORT_IF_FALSE(aChunkSize, "Require aChunkSize argument");
  *aChunkSize = 0;

  for (;;) {
    static const unsigned int CHUNK_HEADER_SIZE = 8;
    char chunkHeader[CHUNK_HEADER_SIZE];
    const char* p = chunkHeader;

    if (!ReadAll(chunkHeader, sizeof(chunkHeader))) {
      return false;
    }

    PR_STATIC_ASSERT(sizeof(PRUint32) * 2 <= CHUNK_HEADER_SIZE);
    PRUint32 magic = ReadUint32BE(&p);
    PRUint32 chunkSize = ReadUint32LE(&p);

    if (magic == aWantedChunk) {
      *aChunkSize = chunkSize;
      return true;
    }

    // RIFF chunks are two-byte aligned, so round up if necessary.
    chunkSize += chunkSize % 2;

    static const unsigned int MAX_CHUNK_SIZE = 1 << 16;
    PR_STATIC_ASSERT(MAX_CHUNK_SIZE < UINT_MAX / sizeof(char));
    nsAutoArrayPtr<char> chunk(new char[MAX_CHUNK_SIZE]);
    while (chunkSize > 0) {
      PRUint32 size = NS_MIN(chunkSize, MAX_CHUNK_SIZE);
      if (!ReadAll(chunk.get(), size)) {
        return false;
      }
      chunkSize -= size;
    }
  }
}

bool
nsWaveReader::LoadFormatChunk()
{
  PRUint32 fmtSize, rate, channels, frameSize, sampleFormat;
  char waveFormat[WAVE_FORMAT_CHUNK_SIZE];
  const char* p = waveFormat;

  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mDecoder->GetStream()->Tell() % 2 == 0,
                    "LoadFormatChunk called with unaligned stream");

  // The "format" chunk may not directly follow the "riff" chunk, so skip
  // over any intermediate chunks.
  if (!ScanForwardUntil(FRMT_CHUNK_MAGIC, &fmtSize)) {
    return false;
  }

  if (!ReadAll(waveFormat, sizeof(waveFormat))) {
    return false;
  }

  PR_STATIC_ASSERT(sizeof(PRUint16) +
                   sizeof(PRUint16) +
                   sizeof(PRUint32) +
                   4 +
                   sizeof(PRUint16) +
                   sizeof(PRUint16) <= sizeof(waveFormat));
  if (ReadUint16LE(&p) != WAVE_FORMAT_ENCODING_PCM) {
    NS_WARNING("WAVE is not uncompressed PCM, compressed encodings are not supported");
    return false;
  }

  channels = ReadUint16LE(&p);
  rate = ReadUint32LE(&p);

  // Skip over average bytes per second field.
  p += 4;

  frameSize = ReadUint16LE(&p);

  sampleFormat = ReadUint16LE(&p);

  // PCM encoded WAVEs are not expected to have an extended "format" chunk,
  // but I have found WAVEs that have a extended "format" chunk with an
  // extension size of 0 bytes.  Be polite and handle this rather than
  // considering the file invalid.  This code skips any extension of the
  // "format" chunk.
  if (fmtSize > WAVE_FORMAT_CHUNK_SIZE) {
    char extLength[2];
    const char* p = extLength;

    if (!ReadAll(extLength, sizeof(extLength))) {
      return false;
    }

    PR_STATIC_ASSERT(sizeof(PRUint16) <= sizeof(extLength));
    PRUint16 extra = ReadUint16LE(&p);
    if (fmtSize - (WAVE_FORMAT_CHUNK_SIZE + 2) != extra) {
      NS_WARNING("Invalid extended format chunk size");
      return false;
    }
    extra += extra % 2;

    if (extra > 0) {
      PR_STATIC_ASSERT(PR_UINT16_MAX + (PR_UINT16_MAX % 2) < UINT_MAX / sizeof(char));
      nsAutoArrayPtr<char> chunkExtension(new char[extra]);
      if (!ReadAll(chunkExtension.get(), extra)) {
        return false;
      }
    }
  }

  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mDecoder->GetStream()->Tell() % 2 == 0,
                    "LoadFormatChunk left stream unaligned");

  // Make sure metadata is fairly sane.  The rate check is fairly arbitrary,
  // but the channels check is intentionally limited to mono or stereo
  // because that's what the audio backend currently supports.
  if (rate < 100 || rate > 96000 ||
      channels < 1 || channels > MAX_CHANNELS ||
      (frameSize != 1 && frameSize != 2 && frameSize != 4) ||
      (sampleFormat != 8 && sampleFormat != 16)) {
    NS_WARNING("Invalid WAVE metadata");
    return false;
  }

  ReentrantMonitorAutoEnter monitor(mDecoder->GetReentrantMonitor());
  mSampleRate = rate;
  mChannels = channels;
  mFrameSize = frameSize;
  if (sampleFormat == 8) {
    mSampleFormat = nsAudioStream::FORMAT_U8;
  } else {
    mSampleFormat = nsAudioStream::FORMAT_S16_LE;
  }
  return true;
}

bool
nsWaveReader::FindDataOffset()
{
  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mDecoder->GetStream()->Tell() % 2 == 0,
                    "FindDataOffset called with unaligned stream");

  // The "data" chunk may not directly follow the "format" chunk, so skip
  // over any intermediate chunks.
  PRUint32 length;
  if (!ScanForwardUntil(DATA_CHUNK_MAGIC, &length)) {
    return false;
  }

  PRInt64 offset = mDecoder->GetStream()->Tell();
  if (offset <= 0 || offset > PR_UINT32_MAX) {
    NS_WARNING("PCM data offset out of range");
    return false;
  }

  ReentrantMonitorAutoEnter monitor(mDecoder->GetReentrantMonitor());
  mWaveLength = length;
  mWavePCMOffset = PRUint32(offset);
  return true;
}

double
nsWaveReader::BytesToTime(PRInt64 aBytes) const
{
  NS_ABORT_IF_FALSE(aBytes >= 0, "Must be >= 0");
  return float(aBytes) / mSampleRate / mFrameSize;
}

PRInt64
nsWaveReader::TimeToBytes(double aTime) const
{
  NS_ABORT_IF_FALSE(aTime >= 0.0f, "Must be >= 0");
  return RoundDownToFrame(PRInt64(aTime * mSampleRate * mFrameSize));
}

PRInt64
nsWaveReader::RoundDownToFrame(PRInt64 aBytes) const
{
  NS_ABORT_IF_FALSE(aBytes >= 0, "Must be >= 0");
  return aBytes - (aBytes % mFrameSize);
}

PRInt64
nsWaveReader::GetDataLength()
{
  PRInt64 length = mWaveLength;
  // If the decoder has a valid content length, and it's shorter than the
  // expected length of the PCM data, calculate the playback duration from
  // the content length rather than the expected PCM data length.
  PRInt64 streamLength = mDecoder->GetStream()->GetLength();
  if (streamLength >= 0) {
    PRInt64 dataLength = NS_MAX<PRInt64>(0, streamLength - mWavePCMOffset);
    length = NS_MIN(dataLength, length);
  }
  return length;
}

PRInt64
nsWaveReader::GetPosition()
{
  return mDecoder->GetStream()->Tell();
}
