/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "nsMemory.h"
#include "MP3FrameParser.h"

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

  int64_t GetMP3Offset() const {
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
    mFrameSizeSum(0),
    mTrailing(0)
  {
    MOZ_ASSERT(mBuffer || !mLength);
  }

  static const uint8_t* FindNextHeader(const uint8_t* aBuffer, uint32_t aLength);

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

  int64_t GetFrameSizeSum() const {
    return mFrameSizeSum;
  }

  int64_t GetTrailing() const {
    return mTrailing;
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
                                          size_t* aFrameSize,
                                          uint32_t* aBitRate,
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

  // The sum of all frame's sizes in byte.
  int32_t mFrameSizeSum;

  // The number of trailing bytes.
  int32_t mTrailing;
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

const uint8_t* MP3Buffer::FindNextHeader(const uint8_t* aBuffer, uint32_t aLength)
{
  MOZ_ASSERT(aBuffer || !aLength);

  // Find MP3's frame-sync marker while there are at least 4 bytes
  // left to contain the MP3 frame header

  while (aLength >= MP3_HEADER_LENGTH) {
    if (ExtractFrameHeader(aBuffer)) {
      break;
    }
    ++aBuffer;
    --aLength;
  }

  return aBuffer;
}

nsresult MP3Buffer::DecodeFrameHeader(const uint8_t* aBuffer,
                                      uint32_t* aFrameSize,
                                      uint32_t* aBitRate,
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

  return NS_OK;
}

nsresult MP3Buffer::Parse()
{
  // We walk over the newly arrived data and sum up the
  // bit rates, sizes, durations, etc. of the contained
  // MP3 frames.

  const uint8_t* buffer = mBuffer;
  uint32_t       length = mLength;

  while (length >= MP3_HEADER_LENGTH) {

    uint32_t frameSize;
    uint32_t bitRate;
    uint64_t duration;

    nsresult rv = DecodeFrameHeader(buffer, &frameSize, &bitRate, &duration);
    NS_ENSURE_SUCCESS(rv, rv);

    mBitRateSum += bitRate;
    mDurationUs += duration;
    ++mNumFrames;

    mFrameSizeSum += frameSize;

    if (frameSize <= length) {
      length -= frameSize;
    } else {
      length = 0;
    }

    buffer += frameSize;
  }

  mTrailing = length;

  return NS_OK;
}

MP3FrameParser::MP3FrameParser(int64_t aLength)
: mBufferLength(0),
  mLock("MP3FrameParser.mLock"),
  mDurationUs(0),
  mBitRateSum(0),
  mNumFrames(0),
  mOffset(0),
  mUnhandled(0),
  mLength(aLength),
  mTrailing(0),
  mIsMP3(true)
{ }

size_t MP3FrameParser::ParseInternalBuffer(const uint8_t* aBuffer, uint32_t aLength, int64_t aOffset)
{
  if (mOffset != aOffset) {
    // If we don't append, we throw away our temporary buffer.
    mBufferLength = 0;
    return 0;
  }

  size_t copyLength = 0;

  if (mBufferLength || !mOffset) {

    // We have some data in our temporary buffer and append to it, or
    // we are at the beginning of the stream. We both cases, we append
    // some data to our temporary buffer and try to parse it.
    copyLength = std::min<size_t>(NS_ARRAY_LENGTH(mBuffer)-mBufferLength, aLength);
    memcpy(mBuffer+mBufferLength, aBuffer, copyLength*sizeof(*mBuffer));
    mBufferLength += copyLength;
  }

  if ((mBufferLength >= ID3Buffer::ID3_HEADER_LENGTH) && (mOffset < ID3Buffer::ID3_HEADER_LENGTH)) {

    // There might be an ID3 header at the very beginning of the stream.
    ID3Buffer id3Buffer(mBuffer, mBufferLength);
    nsresult rv = id3Buffer.Parse();

    if (rv == NS_OK) {
      mOffset += id3Buffer.GetMP3Offset()-(mBufferLength-copyLength);
      mBufferLength = 0;
    }
  }

  if (mBufferLength >= MP3Buffer::MP3_HEADER_LENGTH) {

    // Or there could be a regular frame header somewhere
    // in the stream.
    MP3Buffer mp3Buffer(mBuffer, mBufferLength);
    nsresult rv = mp3Buffer.Parse();

    if (rv == NS_OK) {
      mDurationUs += mp3Buffer.GetDuration();
      mBitRateSum += mp3Buffer.GetBitRateSum();
      mNumFrames  += mp3Buffer.GetNumberOfFrames();
      mOffset     += mp3Buffer.GetFrameSizeSum()-(mBufferLength-copyLength);
      mBufferLength = 0;
    }
  }

  if (mBufferLength) {
    // We have not been able to successfully parse the
    // content of the temporary buffer. If the buffer is
    // full already, the stream does not contain MP3.
    mOffset += copyLength;
    mIsMP3   = (mBufferLength < NS_ARRAY_LENGTH(mBuffer));
  } else {
    // We parsed the temporary buffer. The parser code
    // will update the input data.
    copyLength = 0;
  }

  if (mOffset > mLength) {
    mLength = mOffset;
  }

  return copyLength;
}

void MP3FrameParser::Parse(const uint8_t* aBuffer, uint32_t aLength, int64_t aOffset)
{
  MutexAutoLock mon(mLock);

  // We first try to parse the remaining data from the last call that
  // is stored in an internal buffer.
  size_t bufferIncr = ParseInternalBuffer(aBuffer, aLength, aOffset);

  aBuffer += bufferIncr;
  aLength -= bufferIncr;
  aOffset += bufferIncr;

  // The number of attempts to parse the data. This should be 1 of we
  // append to the end of the existing data.
  int retries = 1;

  if (aOffset+aLength <= mOffset) {
    // We already processed this fragment.
    return;
  } else if (aOffset < mOffset) {
    // mOffset is within the new fragment, shorten range.
    aLength -= mOffset-aOffset;
    aBuffer += mOffset-aOffset;
    aOffset  = mOffset;
  } else if (aOffset > mOffset) {
    // Fragment comes after current position, store difference.
    mUnhandled += aOffset-mOffset;

    // We might start in the middle of a frame and have find the next
    // frame header. As our detection heuristics might return false
    // positives, we simply try multiple times. The current value comes
    // from experimentation with MP3 files. If you encounter false positives
    // and incorrectly parsed MP3 files, try incrementing this value.
    retries = 5;
  }

  uint32_t trailing = 0;

  while (retries) {

    MP3Buffer mp3Buffer(aBuffer, aLength);
    nsresult rv = mp3Buffer.Parse();

    if (rv != NS_OK) {
      --retries;

      if (!retries) {
        mIsMP3 = false;
        return;
      }

      // We might be in the middle of a frame, find next frame header
      const uint8_t *buffer = MP3Buffer::FindNextHeader(aBuffer+1, aLength-1);

      mUnhandled += buffer-aBuffer;
      mOffset     = aOffset + buffer-aBuffer;
      aLength    -= buffer-aBuffer;
      aBuffer     = buffer;
    } else {
      mDurationUs += mp3Buffer.GetDuration();
      mBitRateSum += mp3Buffer.GetBitRateSum();
      mNumFrames  += mp3Buffer.GetNumberOfFrames();
      mOffset     += mp3Buffer.GetFrameSizeSum();

      trailing = mp3Buffer.GetTrailing();
      retries = 0;
    }
  }

  if (trailing) {
    // Store trailing bytes in temporary buffer.
    MOZ_ASSERT(trailing < (NS_ARRAY_LENGTH(mBuffer)*sizeof(*mBuffer)));
    memcpy(mBuffer, aBuffer+(aLength-trailing), trailing);
    mBufferLength = trailing;
  }

  if (mOffset > mLength) {
    mLength = mOffset;
  }
}

void MP3FrameParser::NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  Parse(reinterpret_cast<const uint8_t*>(aBuffer), aLength, aOffset);
}

int64_t MP3FrameParser::GetDuration()
{
  MutexAutoLock mon(mLock);

  if (!mNumFrames) {
    return -1; // Not a single frame decoded yet
  }

  // Compute the duration of the unhandled fragments from
  // the average bitrate.
  int64_t avgBitRate = mBitRateSum / mNumFrames;
  NS_ENSURE_TRUE(avgBitRate > 0, mDurationUs);

  MOZ_ASSERT(mLength >= mOffset);
  int64_t unhandled = mUnhandled + (mLength-mOffset);

  return mDurationUs + (uint64_t(MP3Buffer::MP3_DURATION_CONST) * unhandled) / avgBitRate;
}

}
