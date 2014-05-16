/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsError.h"
#include "AbstractMediaDecoder.h"
#include "MediaResource.h"
#include "WaveReader.h"
#include "mozilla/dom/TimeRanges.h"
#include "MediaDecoderStateMachine.h"
#include "VideoUtils.h"
#include "nsISeekableStream.h"

#include <stdint.h>
#include "mozilla/ArrayUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Endian.h"
#include <algorithm>

namespace mozilla {

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#ifdef SEEK_LOGGING
#define SEEK_LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#else
#define SEEK_LOG(type, msg)
#endif
#else
#define LOG(type, msg)
#define SEEK_LOG(type, msg)
#endif

struct waveIdToName {
  uint32_t id;
  nsCString name;
};


// Magic values that identify RIFF chunks we're interested in.
static const uint32_t RIFF_CHUNK_MAGIC = 0x52494646;
static const uint32_t WAVE_CHUNK_MAGIC = 0x57415645;
static const uint32_t FRMT_CHUNK_MAGIC = 0x666d7420;
static const uint32_t DATA_CHUNK_MAGIC = 0x64617461;
static const uint32_t LIST_CHUNK_MAGIC = 0x4c495354;

// Size of chunk header.  4 byte chunk header type and 4 byte size field.
static const uint16_t CHUNK_HEADER_SIZE = 8;

// Size of RIFF header.  RIFF chunk and 4 byte RIFF type.
static const uint16_t RIFF_INITIAL_SIZE = CHUNK_HEADER_SIZE + 4;

// Size of required part of format chunk.  Actual format chunks may be
// extended (for non-PCM encodings), but we skip any extended data.
static const uint16_t WAVE_FORMAT_CHUNK_SIZE = 16;

// PCM encoding type from format chunk.  Linear PCM is the only encoding
// supported by AudioStream.
static const uint16_t WAVE_FORMAT_ENCODING_PCM = 1;

// We reject files with more than this number of channels if we're decoding for
// playback.
static const uint8_t MAX_CHANNELS = 2;

namespace {
  uint32_t
  ReadUint32BE(const char** aBuffer)
  {
    uint32_t result = BigEndian::readUint32(*aBuffer);
    *aBuffer += sizeof(uint32_t);
    return result;
  }

  uint32_t
  ReadUint32LE(const char** aBuffer)
  {
    uint32_t result = LittleEndian::readUint32(*aBuffer);
    *aBuffer += sizeof(uint32_t);
    return result;
  }

  uint16_t
  ReadUint16LE(const char** aBuffer)
  {
    uint16_t result = LittleEndian::readUint16(*aBuffer);
    *aBuffer += sizeof(uint16_t);
    return result;
  }

  int16_t
  ReadInt16LE(const char** aBuffer)
  {
    uint16_t result = LittleEndian::readInt16(*aBuffer);
    *aBuffer += sizeof(int16_t);
    return result;
  }

  uint8_t
  ReadUint8(const char** aBuffer)
  {
    uint8_t result = uint8_t((*aBuffer)[0]);
    *aBuffer += sizeof(uint8_t);
    return result;
  }
}

WaveReader::WaveReader(AbstractMediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder)
{
  MOZ_COUNT_CTOR(WaveReader);
}

WaveReader::~WaveReader()
{
  MOZ_COUNT_DTOR(WaveReader);
}

nsresult WaveReader::Init(MediaDecoderReader* aCloneDonor)
{
  return NS_OK;
}

nsresult WaveReader::ReadMetadata(MediaInfo* aInfo,
                                  MetadataTags** aTags)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  bool loaded = LoadRIFFChunk();
  if (!loaded) {
    return NS_ERROR_FAILURE;
  }

  nsAutoPtr<dom::HTMLMediaElement::MetadataTags> tags;

  bool loadAllChunks = LoadAllChunks(tags);
  if (!loadAllChunks) {
    return NS_ERROR_FAILURE;
  }

  mInfo.mAudio.mHasAudio = true;
  mInfo.mAudio.mRate = mSampleRate;
  mInfo.mAudio.mChannels = mChannels;

  *aInfo = mInfo;

  *aTags = tags.forget();

  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  mDecoder->SetMediaDuration(
    static_cast<int64_t>(BytesToTime(GetDataLength()) * USECS_PER_S));

  return NS_OK;
}

template <typename T> T UnsignedByteToAudioSample(uint8_t aValue);
template <typename T> T SignedShortToAudioSample(int16_t aValue);

template <> inline float
UnsignedByteToAudioSample<float>(uint8_t aValue)
{
  return aValue * (2.0f / UINT8_MAX) - 1.0f;
}
template <> inline int16_t
UnsignedByteToAudioSample<int16_t>(uint8_t aValue)
{
  return int16_t(aValue * UINT16_MAX / UINT8_MAX + INT16_MIN);
}

template <> inline float
SignedShortToAudioSample<float>(int16_t aValue)
{
  return AudioSampleToFloat(aValue);
}
template <> inline int16_t
SignedShortToAudioSample<int16_t>(int16_t aValue)
{
  return aValue;
}

bool WaveReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  int64_t pos = GetPosition() - mWavePCMOffset;
  int64_t len = GetDataLength();
  int64_t remaining = len - pos;
  NS_ASSERTION(remaining >= 0, "Current wave position is greater than wave file length");

  static const int64_t BLOCK_SIZE = 4096;
  int64_t readSize = std::min(BLOCK_SIZE, remaining);
  int64_t frames = readSize / mFrameSize;

  static_assert(uint64_t(BLOCK_SIZE) < UINT_MAX /
                sizeof(AudioDataValue) / MAX_CHANNELS,
                "bufferSize calculation could overflow.");
  const size_t bufferSize = static_cast<size_t>(frames * mChannels);
  nsAutoArrayPtr<AudioDataValue> sampleBuffer(new AudioDataValue[bufferSize]);

  static_assert(uint64_t(BLOCK_SIZE) < UINT_MAX / sizeof(char),
                "BLOCK_SIZE too large for enumerator.");
  nsAutoArrayPtr<char> dataBuffer(new char[static_cast<size_t>(readSize)]);

  if (!ReadAll(dataBuffer, readSize)) {
    return false;
  }

  // convert data to samples
  const char* d = dataBuffer.get();
  AudioDataValue* s = sampleBuffer.get();
  for (int i = 0; i < frames; ++i) {
    for (unsigned int j = 0; j < mChannels; ++j) {
      if (mSampleFormat == FORMAT_U8) {
        uint8_t v =  ReadUint8(&d);
        *s++ = UnsignedByteToAudioSample<AudioDataValue>(v);
      } else if (mSampleFormat == FORMAT_S16) {
        int16_t v =  ReadInt16LE(&d);
        *s++ = SignedShortToAudioSample<AudioDataValue>(v);
      }
    }
  }

  double posTime = BytesToTime(pos);
  double readSizeTime = BytesToTime(readSize);
  NS_ASSERTION(posTime <= INT64_MAX / USECS_PER_S, "posTime overflow");
  NS_ASSERTION(readSizeTime <= INT64_MAX / USECS_PER_S, "readSizeTime overflow");
  NS_ASSERTION(frames < INT32_MAX, "frames overflow");

  mAudioQueue.Push(new AudioData(pos,
                                 static_cast<int64_t>(posTime * USECS_PER_S),
                                 static_cast<int64_t>(readSizeTime * USECS_PER_S),
                                 static_cast<int32_t>(frames),
                                 sampleBuffer.forget(),
                                 mChannels));

  return true;
}

bool WaveReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                      int64_t aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  return false;
}

nsresult WaveReader::Seek(int64_t aTarget, int64_t aStartTime, int64_t aEndTime, int64_t aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  LOG(PR_LOG_DEBUG, ("%p About to seek to %lld", mDecoder, aTarget));
  if (NS_FAILED(ResetDecode())) {
    return NS_ERROR_FAILURE;
  }
  double d = BytesToTime(GetDataLength());
  NS_ASSERTION(d < INT64_MAX / USECS_PER_S, "Duration overflow"); 
  int64_t duration = static_cast<int64_t>(d * USECS_PER_S);
  double seekTime = std::min(aTarget, duration) / static_cast<double>(USECS_PER_S);
  int64_t position = RoundDownToFrame(static_cast<int64_t>(TimeToBytes(seekTime)));
  NS_ASSERTION(INT64_MAX - mWavePCMOffset > position, "Integer overflow during wave seek");
  position += mWavePCMOffset;
  return mDecoder->GetResource()->Seek(nsISeekableStream::NS_SEEK_SET, position);
}

static double RoundToUsecs(double aSeconds) {
  return floor(aSeconds * USECS_PER_S) / USECS_PER_S;
}

nsresult WaveReader::GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime)
{
  if (!mInfo.HasAudio()) {
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
WaveReader::ReadAll(char* aBuf, int64_t aSize, int64_t* aBytesRead)
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
    got += read;
    if (aBytesRead) {
      *aBytesRead = got;
    }
  } while (got != aSize);
  return true;
}

bool
WaveReader::LoadRIFFChunk()
{
  char riffHeader[RIFF_INITIAL_SIZE];
  const char* p = riffHeader;

  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() == 0,
                    "LoadRIFFChunk called when resource in invalid state");

  if (!ReadAll(riffHeader, sizeof(riffHeader))) {
    return false;
  }

  static_assert(sizeof(uint32_t) * 3 <= RIFF_INITIAL_SIZE,
                "Reads would overflow riffHeader buffer.");
  if (ReadUint32BE(&p) != RIFF_CHUNK_MAGIC) {
    NS_WARNING("resource data not in RIFF format");
    return false;
  }

  // Skip over RIFF size field.
  p += sizeof(uint32_t);

  if (ReadUint32BE(&p) != WAVE_CHUNK_MAGIC) {
    NS_WARNING("Expected WAVE chunk");
    return false;
  }

  return true;
}

bool
WaveReader::LoadFormatChunk(uint32_t aChunkSize)
{
  uint32_t rate, channels, frameSize, sampleFormat;
  char waveFormat[WAVE_FORMAT_CHUNK_SIZE];
  const char* p = waveFormat;

  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() % 2 == 0,
                    "LoadFormatChunk called with unaligned resource");

  if (!ReadAll(waveFormat, sizeof(waveFormat))) {
    return false;
  }

  static_assert(sizeof(uint16_t) +
                sizeof(uint16_t) +
                sizeof(uint32_t) +
                4 +
                sizeof(uint16_t) +
                sizeof(uint16_t) <= sizeof(waveFormat),
                "Reads would overflow waveFormat buffer.");
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
  if (aChunkSize > WAVE_FORMAT_CHUNK_SIZE) {
    char extLength[2];
    const char* p = extLength;

    if (!ReadAll(extLength, sizeof(extLength))) {
      return false;
    }

    static_assert(sizeof(uint16_t) <= sizeof(extLength),
                  "Reads would overflow extLength buffer.");
    uint16_t extra = ReadUint16LE(&p);
    if (aChunkSize - (WAVE_FORMAT_CHUNK_SIZE + 2) != extra) {
      NS_WARNING("Invalid extended format chunk size");
      return false;
    }
    extra += extra % 2;

    if (extra > 0) {
      static_assert(UINT16_MAX + (UINT16_MAX % 2) < UINT_MAX / sizeof(char),
                    "chunkExtension array too large for iterator.");
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
  // when the media is intended for direct playback because that's what the
  // audio backend currently supports.
  unsigned int actualFrameSize = (sampleFormat == 8 ? 1 : 2) * channels;
  if (rate < 100 || rate > 96000 ||
      (((channels < 1 || channels > MAX_CHANNELS) ||
       (frameSize != 1 && frameSize != 2 && frameSize != 4)) &&
       !mIgnoreAudioOutputFormat) ||
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
    mSampleFormat = FORMAT_U8;
  } else {
    mSampleFormat = FORMAT_S16;
  }
  return true;
}

bool
WaveReader::FindDataOffset(uint32_t aChunkSize)
{
  // RIFF chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() % 2 == 0,
                    "FindDataOffset called with unaligned resource");

  int64_t offset = mDecoder->GetResource()->Tell();
  if (offset <= 0 || offset > UINT32_MAX) {
    NS_WARNING("PCM data offset out of range");
    return false;
  }

  ReentrantMonitorAutoEnter monitor(mDecoder->GetReentrantMonitor());
  mWaveLength = aChunkSize;
  mWavePCMOffset = uint32_t(offset);
  return true;
}

double
WaveReader::BytesToTime(int64_t aBytes) const
{
  NS_ABORT_IF_FALSE(aBytes >= 0, "Must be >= 0");
  return float(aBytes) / mSampleRate / mFrameSize;
}

int64_t
WaveReader::TimeToBytes(double aTime) const
{
  NS_ABORT_IF_FALSE(aTime >= 0.0f, "Must be >= 0");
  return RoundDownToFrame(int64_t(aTime * mSampleRate * mFrameSize));
}

int64_t
WaveReader::RoundDownToFrame(int64_t aBytes) const
{
  NS_ABORT_IF_FALSE(aBytes >= 0, "Must be >= 0");
  return aBytes - (aBytes % mFrameSize);
}

int64_t
WaveReader::GetDataLength()
{
  int64_t length = mWaveLength;
  // If the decoder has a valid content length, and it's shorter than the
  // expected length of the PCM data, calculate the playback duration from
  // the content length rather than the expected PCM data length.
  int64_t streamLength = mDecoder->GetResource()->GetLength();
  if (streamLength >= 0) {
    int64_t dataLength = std::max<int64_t>(0, streamLength - mWavePCMOffset);
    length = std::min(dataLength, length);
  }
  return length;
}

int64_t
WaveReader::GetPosition()
{
  return mDecoder->GetResource()->Tell();
}

bool
WaveReader::GetNextChunk(uint32_t* aChunk, uint32_t* aChunkSize)
{
  NS_ABORT_IF_FALSE(aChunk, "Must have aChunk");
  NS_ABORT_IF_FALSE(aChunkSize, "Must have aChunkSize");
  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() % 2 == 0,
                    "GetNextChunk called with unaligned resource");

  char chunkHeader[CHUNK_HEADER_SIZE];
  const char* p = chunkHeader;

  if (!ReadAll(chunkHeader, sizeof(chunkHeader))) {
    return false;
  }

  static_assert(sizeof(uint32_t) * 2 <= CHUNK_HEADER_SIZE,
                "Reads would overflow chunkHeader buffer.");
  *aChunk = ReadUint32BE(&p);
  *aChunkSize = ReadUint32LE(&p);

  return true;
}

bool
WaveReader::LoadListChunk(uint32_t aChunkSize,
                          nsAutoPtr<dom::HTMLMediaElement::MetadataTags> &aTags)
{
  // List chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() % 2 == 0,
                    "LoadListChunk called with unaligned resource");

  static const unsigned int MAX_CHUNK_SIZE = 1 << 16;
  static_assert(uint64_t(MAX_CHUNK_SIZE) < UINT_MAX / sizeof(char),
                "MAX_CHUNK_SIZE too large for enumerator.");

  if (aChunkSize > MAX_CHUNK_SIZE) {
    return false;
  }

  nsAutoArrayPtr<char> chunk(new char[aChunkSize]);
  if (!ReadAll(chunk.get(), aChunkSize)) {
    return false;
  }

  static const uint32_t INFO_LIST_MAGIC = 0x494e464f;
  const char *p = chunk.get();
  if (ReadUint32BE(&p) != INFO_LIST_MAGIC) {
    return false;
  }

  const waveIdToName ID_TO_NAME[] = {
    { 0x49415254, NS_LITERAL_CSTRING("artist") },   // IART
    { 0x49434d54, NS_LITERAL_CSTRING("comments") }, // ICMT
    { 0x49474e52, NS_LITERAL_CSTRING("genre") },    // IGNR
    { 0x494e414d, NS_LITERAL_CSTRING("name") },     // INAM
  };

  const char* const end = chunk.get() + aChunkSize;

  aTags = new dom::HTMLMediaElement::MetadataTags;

  while (p + 8 < end) {
    uint32_t id = ReadUint32BE(&p);
    // Uppercase tag id, inspired by GStreamer's Wave parser.
    id &= 0xDFDFDFDF;

    uint32_t length = ReadUint32LE(&p);

    // Subchunk shall not exceed parent chunk.
    if (end - p < length) {
      break;
    }

    // Wrap the string, adjusting length to account for optional
    // null termination in the chunk.
    nsCString val(p, length);
    if (length > 0 && val[length - 1] == '\0') {
      val.SetLength(length - 1);
    }

    // Chunks in List::INFO are always word (two byte) aligned. So round up if
    // necessary.
    length += length % 2;
    p += length;

    if (!IsUTF8(val)) {
      continue;
    }

    for (size_t i = 0; i < mozilla::ArrayLength(ID_TO_NAME); ++i) {
      if (id == ID_TO_NAME[i].id) {
        aTags->Put(ID_TO_NAME[i].name, val);
        break;
      }
    }
  }

  return true;
}

bool
WaveReader::LoadAllChunks(nsAutoPtr<dom::HTMLMediaElement::MetadataTags> &aTags)
{
  // Chunks are always word (two byte) aligned.
  NS_ABORT_IF_FALSE(mDecoder->GetResource()->Tell() % 2 == 0,
                    "LoadAllChunks called with unaligned resource");

  bool loadFormatChunk = false;
  bool findDataOffset = false;

  for (;;) {
    static const unsigned int CHUNK_HEADER_SIZE = 8;
    char chunkHeader[CHUNK_HEADER_SIZE];
    const char* p = chunkHeader;

    if (!ReadAll(chunkHeader, sizeof(chunkHeader))) {
      return false;
    }

    static_assert(sizeof(uint32_t) * 2 <= CHUNK_HEADER_SIZE,
                  "Reads would overflow chunkHeader buffer.");

    uint32_t magic = ReadUint32BE(&p);
    uint32_t chunkSize = ReadUint32LE(&p);
    int64_t chunkStart = GetPosition();

    switch (magic) {
      case FRMT_CHUNK_MAGIC:
        loadFormatChunk = LoadFormatChunk(chunkSize);
        if (!loadFormatChunk) {
          return false;
        }
        break;

      case LIST_CHUNK_MAGIC:
        if (!aTags) {
          LoadListChunk(chunkSize, aTags);
        }
        break;

      case DATA_CHUNK_MAGIC:
        findDataOffset = FindDataOffset(chunkSize);
        return loadFormatChunk && findDataOffset;

      default:
        break;
    }

    // RIFF chunks are two-byte aligned, so round up if necessary.
    chunkSize += chunkSize % 2;

    // Move forward to next chunk
    CheckedInt64 forward = CheckedInt64(chunkStart) + chunkSize - GetPosition();

    if (!forward.isValid() || forward.value() < 0) {
      return false;
    }

    static const int64_t MAX_CHUNK_SIZE = 1 << 16;
    static_assert(uint64_t(MAX_CHUNK_SIZE) < UINT_MAX / sizeof(char),
                  "MAX_CHUNK_SIZE too large for enumerator.");
    nsAutoArrayPtr<char> chunk(new char[MAX_CHUNK_SIZE]);
    while (forward.value() > 0) {
      int64_t size = std::min(forward.value(), MAX_CHUNK_SIZE);
      if (!ReadAll(chunk.get(), size)) {
        return false;
      }
      forward -= size;
    }
  }

  return false;
}

} // namespace mozilla
