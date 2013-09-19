/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "nsMemory.h"
#include "MP3FrameParser.h"
#include "VideoUtils.h"

namespace mozilla {

// An ID3Buffer contains data of an ID3v2 header. The supplied buffer must
// point to an ID3 header and at least the size of ID_HEADER_LENGTH. Run the
// Parse method to read in the header's values.

class ID3Buffer
{
public:

  enum {
    ID3_HEADER_LENGTH = 10
  };

  ID3Buffer(const uint8_t* aBuffer, uint32_t aLength)
  : mBuffer(aBuffer),
    mLength(aLength),
    mSize(0)
  {
    MOZ_ASSERT(mBuffer || !mLength);
  }

  nsresult Parse();

  int64_t Length() const {
    return ID3_HEADER_LENGTH + mSize;
  }

private:
  const uint8_t* mBuffer;
  uint32_t       mLength;
  uint32_t       mSize;
};

nsresult ID3Buffer::Parse()
{
  NS_ENSURE_TRUE(mBuffer && mLength >= ID3_HEADER_LENGTH, NS_ERROR_INVALID_ARG);

  if ((mBuffer[0] != 'I') ||
      (mBuffer[1] != 'D') ||
      (mBuffer[2] != '3') ||
      (mBuffer[6] & 0x80) ||
      (mBuffer[7] & 0x80) ||
      (mBuffer[8] & 0x80) ||
      (mBuffer[9] & 0x80)) {
    return NS_ERROR_INVALID_ARG;
  }

  mSize = ((static_cast<uint32_t>(mBuffer[6])<<21) |
           (static_cast<uint32_t>(mBuffer[7])<<14) |
           (static_cast<uint32_t>(mBuffer[8])<<7)  |
            static_cast<uint32_t>(mBuffer[9]));

  return NS_OK;
}

// The MP3Buffer contains MP3 frame data. The supplied buffer must point
// to a frame header. Call the method Parse to extract information from
// the MP3 frame headers in the supplied buffer.

class MP3Buffer
{
public:

  enum {
    MP3_HEADER_LENGTH   = 4,
    MP3_FRAMESIZE_CONST = 144000,
    MP3_DURATION_CONST  = 8000
  };

  MP3Buffer(const uint8_t* aBuffer, uint32_t aLength)
  : mBuffer(aBuffer),
    mLength(aLength),
    mDurationUs(0),
    mNumFrames(0),
    mBitRateSum(0),
    mSampleRate(0),
    mFrameSizeSum(0)
  {
    MOZ_ASSERT(mBuffer || !mLength);
  }

  nsresult Parse();

  int64_t GetDuration() const {
    return mDurationUs;
  }

  int64_t GetNumberOfFrames() const {
    return mNumFrames;
  }

  int64_t GetBitRateSum() const {
    return mBitRateSum;
  }

  int16_t GetSampleRate() const {
    return mSampleRate;
  }

  int64_t GetFrameSizeSum() const {
    return mFrameSizeSum;
  }

private:

  enum MP3FrameHeaderField {
    MP3_HDR_FIELD_SYNC,
    MP3_HDR_FIELD_VERSION,
    MP3_HDR_FIELD_LAYER,
    MP3_HDR_FIELD_BITRATE,
    MP3_HDR_FIELD_SAMPLERATE,
    MP3_HDR_FIELD_PADDING,
    MP3_HDR_FIELDS // Must be last enumerator value
  };

  enum {
    MP3_HDR_CONST_FRAMESYNC = 0x7ff,
    MP3_HDR_CONST_VERSION   = 3,
    MP3_HDR_CONST_LAYER     = 1
  };

  static uint32_t ExtractBits(uint32_t aValue, uint32_t aOffset,
                              uint32_t aBits);
  static uint32_t ExtractFrameHeaderField(uint32_t aHeader,
                                          enum MP3FrameHeaderField aField);
  static uint32_t ExtractFrameHeader(const uint8_t* aBuffer);
  static nsresult DecodeFrameHeader(const uint8_t* aBuffer,
                                    uint32_t* aFrameSize,
                                    uint32_t* aBitRate,
                                    uint16_t* aSampleRate,
                                    uint64_t* aDuration);

  static const uint16_t sBitRate[16];
  static const uint16_t sSampleRate[4];

  const uint8_t* mBuffer;
  uint32_t       mLength;

  // The duration of this parsers data in milliseconds.
  int64_t mDurationUs;

  // The number of frames in the range.
  int64_t mNumFrames;

  // The sum of all frame's bit rates.
  int64_t mBitRateSum;

  // The number of audio samples per second
  int16_t mSampleRate;

  // The sum of all frame's sizes in byte.
  int32_t mFrameSizeSum;
};

const uint16_t MP3Buffer::sBitRate[16] = {
  0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
};

const uint16_t MP3Buffer::sSampleRate[4] = {
  44100, 48000, 32000, 0
};

uint32_t MP3Buffer::ExtractBits(uint32_t aValue, uint32_t aOffset, uint32_t aBits)
{
  return (aValue >> aOffset) & ((0x1ul << aBits) - 1);
}

uint32_t MP3Buffer::ExtractFrameHeaderField(uint32_t aHeader, enum MP3FrameHeaderField aField)
{
  static const uint8_t sField[MP3_HDR_FIELDS][2] = {
    {21, 11}, {19, 2}, {17, 2}, {12, 4}, {10, 2}, {9, 1}
  };

  MOZ_ASSERT(aField < MP3_HDR_FIELDS);
  return ExtractBits(aHeader, sField[aField][0], sField[aField][1]);
}

uint32_t MP3Buffer::ExtractFrameHeader(const uint8_t* aBuffer)
{
  MOZ_ASSERT(aBuffer);

  uint32_t header = (static_cast<uint32_t>(aBuffer[0])<<24) |
                    (static_cast<uint32_t>(aBuffer[1])<<16) |
                    (static_cast<uint32_t>(aBuffer[2])<<8)  |
                     static_cast<uint32_t>(aBuffer[3]);

  uint32_t frameSync = ExtractFrameHeaderField(header, MP3_HDR_FIELD_SYNC);
  uint32_t version = ExtractFrameHeaderField(header, MP3_HDR_FIELD_VERSION);
  uint32_t layer = ExtractFrameHeaderField(header, MP3_HDR_FIELD_LAYER);
  uint32_t bitRate = sBitRate[ExtractFrameHeaderField(header, MP3_HDR_FIELD_BITRATE)];
  uint32_t sampleRate = sSampleRate[ExtractFrameHeaderField(header, MP3_HDR_FIELD_SAMPLERATE)];

  // branch-less implementation of
  //
  //  if (fields-are-valid)
  //    return header;
  //  else
  //    return 0;
  //
  return (frameSync == uint32_t(MP3_HDR_CONST_FRAMESYNC)) *
         (version == uint32_t(MP3_HDR_CONST_VERSION)) *
         (layer == uint32_t(MP3_HDR_CONST_LAYER)) * !!bitRate * !!sampleRate * header;
}

nsresult MP3Buffer::DecodeFrameHeader(const uint8_t* aBuffer,
                                      uint32_t* aFrameSize,
                                      uint32_t* aBitRate,
                                      uint16_t* aSampleRate,
                                      uint64_t* aDuration)
{
  uint32_t header = ExtractFrameHeader(aBuffer);

  if (!header) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t bitRate = sBitRate[ExtractFrameHeaderField(header, MP3_HDR_FIELD_BITRATE)];
  uint32_t sampleRate = sSampleRate[ExtractFrameHeaderField(header, MP3_HDR_FIELD_SAMPLERATE)];

  uint32_t padding = ExtractFrameHeaderField(header, MP3_HDR_FIELD_PADDING);
  uint32_t frameSize = (uint64_t(MP3_FRAMESIZE_CONST) * bitRate) / sampleRate + padding;

  MOZ_ASSERT(aBitRate);
  *aBitRate = bitRate;

  MOZ_ASSERT(aFrameSize);
  *aFrameSize = frameSize;

  MOZ_ASSERT(aDuration);
  *aDuration = (uint64_t(MP3_DURATION_CONST) * frameSize) / bitRate;

  MOZ_ASSERT(aSampleRate);
  *aSampleRate = sampleRate;

  return NS_OK;
}

nsresult MP3Buffer::Parse()
{
  // We walk over the newly arrived data and sum up the
  // bit rates, sizes, durations, etc. of the contained
  // MP3 frames.

  const uint8_t* buffer = mBuffer;
  uint32_t length = mLength;

  while (length >= MP3_HEADER_LENGTH) {

    uint32_t frameSize;
    uint32_t bitRate;
    uint16_t sampleRate;
    uint64_t duration;

    nsresult rv = DecodeFrameHeader(buffer, &frameSize, &bitRate,
                                    &sampleRate, &duration);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mBitRateSum += bitRate;
    mDurationUs += duration;
    ++mNumFrames;

    mFrameSizeSum += frameSize;

    mSampleRate = sampleRate;

    if (frameSize <= length) {
      length -= frameSize;
    } else {
      length = 0;
    }

    buffer += frameSize;
  }

  return NS_OK;
}

// Some MP3's have large ID3v2 tags, up to 150KB, so we allow lots of
// skipped bytes to be read, just in case, before we give up and assume
// we're not parsing an MP3 stream.
static const uint32_t MAX_SKIPPED_BYTES = 200 * 1024;

// The number of audio samples per MP3 frame. This is constant over all MP3
// streams. With this constant, the stream's sample rate, and an estimated
// number of frames in the stream, we can estimate the stream's duration
// fairly accurately.
static const uint32_t SAMPLES_PER_FRAME = 1152;

MP3FrameParser::MP3FrameParser(int64_t aLength)
: mBufferLength(0),
  mLock("MP3FrameParser.mLock"),
  mDurationUs(0),
  mBitRateSum(0),
  mTotalFrameSize(0),
  mNumFrames(0),
  mOffset(0),
  mLength(aLength),
  mMP3Offset(-1),
  mSkippedBytes(0),
  mSampleRate(0),
  mIsMP3(MAYBE_MP3)
{ }

nsresult MP3FrameParser::ParseBuffer(const uint8_t* aBuffer,
                                     uint32_t aLength,
                                     int64_t aStreamOffset,
                                     uint32_t* aOutBytesRead)
{
  // Iterate forwards over the buffer, looking for ID3 tag, or MP3
  // Frame headers.
  uint32_t bufferOffset = 0;
  uint32_t headersParsed = 0;
  while (bufferOffset < aLength) {
    const uint8_t* buffer = aBuffer + bufferOffset;
    const uint32_t length = aLength - bufferOffset;
    if (mMP3Offset == -1) {
      // We've not found any MP3 frames yet, there may still be ID3 tags in
      // the stream, so test for them.
      if (length < ID3Buffer::ID3_HEADER_LENGTH) {
        // We don't have enough data to get a complete ID3 header, bail.
        break;
      }
      ID3Buffer id3Buffer(buffer, length);
      if (NS_SUCCEEDED(id3Buffer.Parse())) {
        bufferOffset += id3Buffer.Length();
        // Try to parse the next chunk.
        headersParsed++;
        continue;
      }
    }
    if (length < MP3Buffer::MP3_HEADER_LENGTH) {
      // We don't have enough data to get a complete MP3 frame header, bail.
      break;
    }
    MP3Buffer mp3Buffer(buffer, length);
    if (NS_SUCCEEDED(mp3Buffer.Parse())) {
      headersParsed++;
      if (mMP3Offset == -1) {
        mMP3Offset = aStreamOffset + bufferOffset;
      }
      mDurationUs += mp3Buffer.GetDuration();
      mBitRateSum += mp3Buffer.GetBitRateSum();
      mTotalFrameSize += mp3Buffer.GetFrameSizeSum();
      mSampleRate = mp3Buffer.GetSampleRate();
      mNumFrames += mp3Buffer.GetNumberOfFrames();
      bufferOffset += mp3Buffer.GetFrameSizeSum();
    } else {
      // No ID3 or MP3 frame header here. Try the next byte.
      ++bufferOffset;
    }
  }
  if (headersParsed == 0) {
    if (mIsMP3 == MAYBE_MP3) {
      mSkippedBytes += aLength;
      if (mSkippedBytes > MAX_SKIPPED_BYTES) {
        mIsMP3 = NOT_MP3;
        return NS_ERROR_FAILURE;
      }
    }
  } else {
    mIsMP3 = DEFINITELY_MP3;
    mSkippedBytes = 0;
  }
  *aOutBytesRead = bufferOffset;
  return NS_OK;
}

void MP3FrameParser::Parse(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  MutexAutoLock mon(mLock);

  const uint8_t* buffer = reinterpret_cast<const uint8_t*>(aBuffer);
  const int64_t lastChunkEnd = mOffset + mBufferLength;
  if (aOffset + aLength <= lastChunkEnd) {
    // We already processed this fragment.
    return;
  } else if (aOffset < lastChunkEnd) {
    // mOffset is within the new fragment, shorten range.
    aLength -= lastChunkEnd - aOffset;
    buffer += lastChunkEnd - aOffset;
    aOffset = lastChunkEnd;
  } else if (aOffset > lastChunkEnd) {
    // Fragment comes after current position, store difference.
    mOffset += aOffset - lastChunkEnd;
    mSkippedBytes = 0;
  }

  if (mBufferLength > 0) {
    // We have some data which was left over from the last buffer we received.
    // Append to it, so that we have enough data to parse a complete header, and
    // try to parse it.
    uint32_t copyLength = std::min<size_t>(NS_ARRAY_LENGTH(mBuffer)-mBufferLength, aLength);
    memcpy(mBuffer+mBufferLength, buffer, copyLength*sizeof(*mBuffer));
    // Caculate the offset of the data in the start of the buffer.
    int64_t streamOffset = mOffset - mBufferLength;
    uint32_t bufferLength = mBufferLength + copyLength;
    uint32_t bytesRead = 0;
    if (NS_FAILED(ParseBuffer(mBuffer,
                              bufferLength,
                              streamOffset,
                              &bytesRead))) {
      return;
    }
    MOZ_ASSERT(bytesRead >= mBufferLength, "Parse should leave original buffer");

    // Adjust the incoming buffer pointer/length so that it reflects that we may have
    // consumed data from buffer.
    uint32_t adjust = bytesRead - mBufferLength;
    mBufferLength = 0;
    if (adjust >= aLength) {
      // The frame or tag found in the buffer finishes outside the range.
      // Just set the offset to the end of that tag/frame, and return.
      mOffset = streamOffset + bytesRead;
      if (mOffset > mLength) {
        mLength = mOffset;
      }
      return;
    }
    aOffset += adjust;
    MOZ_ASSERT(aLength >= adjust);
    aLength -= adjust;
  }

  uint32_t bytesRead = 0;
  if (NS_FAILED(ParseBuffer(buffer,
                            aLength,
                            aOffset,
                            &bytesRead))) {
    return;
  }
  mOffset += bytesRead;

  if (bytesRead < aLength) {
    // We have some data left over. Store trailing bytes in temporary buffer
    // to be parsed next time we receive more data.
    uint32_t trailing = aLength - bytesRead;
    MOZ_ASSERT(trailing < (NS_ARRAY_LENGTH(mBuffer)*sizeof(mBuffer[0])));
    memcpy(mBuffer, buffer+(aLength-trailing), trailing);
    mBufferLength = trailing;
  }

  if (mOffset > mLength) {
    mLength = mOffset;
  }
}

int64_t MP3FrameParser::GetDuration()
{
  MutexAutoLock mon(mLock);

  if (!mNumFrames) {
    return -1; // Not a single frame decoded yet
  }

  // Estimate the total number of frames in the file from the average frame
  // size we've seen so far, and the length of the file.
  double avgFrameSize = (double)mTotalFrameSize / mNumFrames;

  // Need to cut out the header here. Ignore everything up to the first MP3
  // frames.
  double estimatedFrames = (double)(mLength - mMP3Offset) / avgFrameSize;

  // The duration of each frame is constant over a given stream.
  double usPerFrame = USECS_PER_S * SAMPLES_PER_FRAME / mSampleRate;

  return estimatedFrames * usPerFrame;
}

int64_t MP3FrameParser::GetMP3Offset()
{
  MutexAutoLock mon(mLock);
  return mMP3Offset;
}

}
