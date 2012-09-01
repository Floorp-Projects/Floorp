/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsError.h"
#include "nsBuiltinDecoder.h"
#include "MediaResource.h"
#include "nsWaveReader.h"
#include "nsTimeRanges.h"
#include "VideoUtils.h"

#include "mozilla/StandardInteger.h"

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
static const uint32_t RIFF_CHUNK_MAGIC = 0x52494646;
static const uint32_t WAVE_CHUNK_MAGIC = 0x57415645;
static const uint32_t FRMT_CHUNK_MAGIC = 0x666d7420;
static const uint32_t DATA_CHUNK_MAGIC = 0x64617461;

// Size of RIFF chunk header.  4 byte chunk header type and 4 byte size field.
static const uint16_t RIFF_CHUNK_HEADER_SIZE = 8;

// Size of RIFF header.  RIFF chunk and 4 byte RIFF type.
static const uint16_t RIFF_INITIAL_SIZE = RIFF_CHUNK_HEADER_SIZE + 4;

// Size of required part of format chunk.  Actual format chunks may be
// extended (for non-PCM encodings), but we skip any extended data.
static const uint16_t WAVE_FORMAT_CHUNK_SIZE = 16;

// PCM encoding type from format chunk.  Linear PCM is the only encoding
// supported by nsAudioStream.
static const uint16_t WAVE_FORMAT_ENCODING_PCM = 1;

// Maximum number of channels supported
static const uint8_t MAX_CHANNELS = 2;

namespace {
  uint32_t
  ReadUint32BE(const char** aBuffer)
  {
    uint32_t result =
      uint8_t((*aBuffer)[0]) << 24 |
      uint8_t((*aBuffer)[1]) << 16 |
      uint8_t((*aBuffer)[2]) << 8 |
      uint8_t((*aBuffer)[3]);
    *aBuffer += sizeof(uint32_t);
    return result;
  }

  uint32_t
  ReadUint32LE(const char** aBuffer)
  {
    uint32_t result =
      uint8_t((*aBuffer)[3]) << 24 |
      uint8_t((*aBuffer)[2]) << 16 |
      uint8_t((*aBuffer)[1]) << 8 |
      uint8_t((*aBuffer)[0]);
    *aBuffer += sizeof(uint32_t);
    return result;
  }

  uint16_t
  ReadUint16LE(const char** aBuffer)
  {
    uint16_t result =
      uint8_t((*aBuffer)[1]) << 8 |
      uint8_t((*aBuffer)[0]) << 0;
    *aBuffer += sizeof(uint16_t);
    return result;
  }

  int16_t
  ReadInt16LE(const char** aBuffer)
  {
    return static_cast<int16_t>(ReadUint16LE(aBuffer));
  }

  uint8_t
  ReadUint8(const char** aBuffer)
  {
    uint8_t result = uint8_t((*aBuffer)[0]);
    *aBuffer += sizeof(uint8_t);
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

nsresult nsWaveReader::ReadMetadata(nsVideoInfo* aInfo,
                                    nsHTMLMediaElement::MetadataTags** aTags)
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

  *aTags = nullptr;

  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  mDecoder->GetStateMachine()->SetDuration(
    static_cast<int64_t>(BytesToTime(GetDataLength()) * USECS_PER_S));

  return NS_OK;
}

bool nsWaveReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  int64_t pos = GetPosition() - mWavePCMOffset;
  int64_t len = GetDataLength();
  int64_t remaining = len - pos;
  NS_ASSERTION(remaining >= 0, "Current wave position is greater than wave file length");

  static const int64_t BLOCK_SIZE = 4096;
  int64_t readSize = NS_MIN(BLOCK_SIZE, remaining);
  int64_t frames = readSize / mFrameSize;

  PR_STATIC_ASSERT(uint64_t(BLOCK_SIZE) < UINT_MAX / sizeof(AudioDataValue) / MAX_CHANNELS);
  const size_t bufferSize = static_cast<size_t>(frames * mChannels);
  nsAutoArrayPtr<AudioDataValue> sampleBuffer(new AudioDataValue[bufferSize]);

  PR_STATIC_ASSERT(uint64_t(BLOCK_SIZE) < UINT_MAX / sizeof(char));
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
        uint8_t v =  ReadUint8(&d);
#if defined(MOZ_SAMPLE_TYPE_S16)
        *s++ = (v * (1.F/PR_UINT8_MAX)) * PR_UINT16_MAX + PR_INT16_MIN;
#elif defined(MOZ_SAMPLE_TYPE_FLOAT32)
        *s++ = (v * (1.F/PR_UINT8_MAX)) * 2.F - 1.F;
#endif
      }
      else if (mSampleFormat == nsAudioStream::FORMAT_S16) {
        int16_t v =  ReadInt16LE(&d);
#if defined(MOZ_SAMPLE_TYPE_S16)
        *s++ = v;
#elif defined(MOZ_SAMPLE_TYPE_FLOAT32)
        *s++ = (int32_t(v) - PR_INT16_MIN) / float(PR_UINT16_MAX) * 2.F - 1.F;
#endif
      }
    }
  }

  double posTime = BytesToTime(pos);
  double readSizeTime = BytesToTime(readSize);
  NS_ASSERTION(posTime <= INT64_MAX / USECS_PER_S, "posTime overflow");
  NS_ASSERTION(readSizeTime <= INT64_MAX / USECS_PER_S, "readSizeTime overflow");
  NS_ASSERTION(frames < PR_INT32_MAX, "frames overflow");

  mAudioQueue.Push(new AudioData(pos,
                                 static_cast<int64_t>(posTime * USECS_PER_S),
                                 static_cast<int64_t>(readSizeTime * USECS_PER_S),
                                 static_cast<int32_t>(frames),
                                 sampleBuffer.forget(),
                                 mChannels));

  return true;
}

bool nsWaveReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                      int64_t aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  return false;
}

nsresult nsWaveReader::Seek(int64_t aTarget, int64_t aStartTime, int64_t aEndTime, int64_t aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  LOG(PR_LOG_DEBUG, ("%p About to seek to %lld", mDecoder, aTarget));
  if (NS_FAILED(ResetDecode())) {
    return NS_ERROR_FAILURE;
  }
  double d = BytesToTime(GetDataLength());
  NS_ASSERTION(d < INT64_MAX / USECS_PER_S, "Duration overflow"); 
  int64_t duration = static_cast<int64_t>(d * USECS_PER_S);
  double seekTime = NS_MIN(aTarget, duration) / static_cast<double>(USECS_PER_S);
  int64_t position = RoundDownToFrame(static_cast<int64_t>(TimeToBytes(seekTime)));
  NS_ASSERTION(INT64_MAX - mWavePCMOffset > position, "Integer overflow during wave seek");
  position += mWavePCMOffset;
  return mDecoder->GetResource()->Seek(nsISeekableStream::NS_SEEK_SET, position);
}

static double RoundToUsecs(double aSeconds) {
  return floor(aSeconds * USECS_PER_S) / USECS_PER_S;
}

nsresult nsWaveReader::GetBuffered(nsTimeRanges* aBuffered, int64_t aStartTime)
{
  if (!mInfo.mHasAudio) {
    return NS_OK;
  }
  int64_t startOffset = mDecoder->GetResource()->GetNextCachedData(mWavePCMOffset);
  while (startOffset >= 0) {
    int64_t endOffset = mDecoder->GetResource()->GetCachedDataEnd(startOffset);
    // Bytes [startOffset..endOffset] are cached.
    NS_ASSERTION(startOffset >= mWavePCMOffset, "Integer underflow in GetBuffered");
    NS_ASSERTION(endOffset >= mWavePCMOffset, "Integer underflow in GetBuffered");

    // We need to round the buffered ranges' times to microseconds so that they
    // have the same precision as the currentTime and duration attribute on 
    // the media element.
    aBuffered->Add(RoundToUsecs(BytesToTime(startOffset - mWavePCMOffset)),
                   RoundToUsecs(BytesToTime(endOffset - mWavePCMOffset)));
    startOffset = mDecoder->GetResource()->GetNextCachedData(endOffset);
  }
  return NS_OK;
}

bool
nsWaveReader::ReadAll(char* aBuf, int64_t aSize, int64_t* aBytesRead)
{
  uint32_t got = 0;
  if (aBytesRead) {
    *aBytesRead = 0;
  }
  do {
    uint32_t read = 0;
    if (NS_FAILED(mDecoder->GetResource()->Read(aBuf + got, uint32_t(aSize - got), &read))) {
      NS_WARNING("Resource read failed");
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

  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() == 0,
                    "LoadRIFFChunk called when resource in invalid state");

  if (!ReadAll(riffHeader, sizeof(riffHeader))) {
    return false;
  }

  PR_STATIC_ASSERT(sizeof(uint32_t) * 2 <= RIFF_INITIAL_SIZE);
  if (ReadUint32BE(&p) != RIFF_CHUNK_MAGIC) {
    NS_WARNING("resource data not in RIFF format");
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
nsWaveReader::ScanForwardUntil(uint32_t aWantedChunk, uint32_t* aChunkSize)
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

    PR_STATIC_ASSERT(sizeof(uint32_t) * 2 <= CHUNK_HEADER_SIZE);
    uint32_t magic = ReadUint32BE(&p);
    uint32_t chunkSize = ReadUint32LE(&p);

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
      uint32_t size = NS_MIN(chunkSize, MAX_CHUNK_SIZE);
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
  uint32_t fmtSize, rate, channels, frameSize, sampleFormat;
  char waveFormat[WAVE_FORMAT_CHUNK_SIZE];
  const char* p = waveFormat;

  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() % 2 == 0,
                    "LoadFormatChunk called with unaligned resource");

  // The "format" chunk may not directly follow the "riff" chunk, so skip
  // over any intermediate chunks.
  if (!ScanForwardUntil(FRMT_CHUNK_MAGIC, &fmtSize)) {
    return false;
  }

  if (!ReadAll(waveFormat, sizeof(waveFormat))) {
    return false;
  }

  PR_STATIC_ASSERT(sizeof(uint16_t) +
                   sizeof(uint16_t) +
                   sizeof(uint32_t) +
                   4 +
                   sizeof(uint16_t) +
                   sizeof(uint16_t) <= sizeof(waveFormat));
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

    PR_STATIC_ASSERT(sizeof(uint16_t) <= sizeof(extLength));
    uint16_t extra = ReadUint16LE(&p);
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
  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() % 2 == 0,
                    "LoadFormatChunk left resource unaligned");

  // Make sure metadata is fairly sane.  The rate check is fairly arbitrary,
  // but the channels check is intentionally limited to mono or stereo
  // because that's what the audio backend currently supports.
  unsigned int actualFrameSize = sampleFormat == 8 ? 1 : 2 * channels;
  if (rate < 100 || rate > 96000 ||
      channels < 1 || channels > MAX_CHANNELS ||
      (frameSize != 1 && frameSize != 2 && frameSize != 4) ||
      (sampleFormat != 8 && sampleFormat != 16) ||
      frameSize != actualFrameSize) {
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
    mSampleFormat = nsAudioStream::FORMAT_S16;
  }
  return true;
}

bool
nsWaveReader::FindDataOffset()
{
  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() % 2 == 0,
                    "FindDataOffset called with unaligned resource");

  // The "data" chunk may not directly follow the "format" chunk, so skip
  // over any intermediate chunks.
  uint32_t length;
  if (!ScanForwardUntil(DATA_CHUNK_MAGIC, &length)) {
    return false;
  }

  int64_t offset = mDecoder->GetResource()->Tell();
  if (offset <= 0 || offset > PR_UINT32_MAX) {
    NS_WARNING("PCM data offset out of range");
    return false;
  }

  ReentrantMonitorAutoEnter monitor(mDecoder->GetReentrantMonitor());
  mWaveLength = length;
  mWavePCMOffset = uint32_t(offset);
  return true;
}

double
nsWaveReader::BytesToTime(int64_t aBytes) const
{
  NS_ABORT_IF_FALSE(aBytes >= 0, "Must be >= 0");
  return float(aBytes) / mSampleRate / mFrameSize;
}

int64_t
nsWaveReader::TimeToBytes(double aTime) const
{
  NS_ABORT_IF_FALSE(aTime >= 0.0f, "Must be >= 0");
  return RoundDownToFrame(int64_t(aTime * mSampleRate * mFrameSize));
}

int64_t
nsWaveReader::RoundDownToFrame(int64_t aBytes) const
{
  NS_ABORT_IF_FALSE(aBytes >= 0, "Must be >= 0");
  return aBytes - (aBytes % mFrameSize);
}

int64_t
nsWaveReader::GetDataLength()
{
  int64_t length = mWaveLength;
  // If the decoder has a valid content length, and it's shorter than the
  // expected length of the PCM data, calculate the playback duration from
  // the content length rather than the expected PCM data length.
  int64_t streamLength = mDecoder->GetResource()->GetLength();
  if (streamLength >= 0) {
    int64_t dataLength = NS_MAX<int64_t>(0, streamLength - mWavePCMOffset);
    length = NS_MIN(dataLength, length);
  }
  return length;
}

int64_t
nsWaveReader::GetPosition()
{
  return mDecoder->GetResource()->Tell();
}
