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

/*
 * Following code taken from http://www.hydrogenaudio.org/forums/index.php?showtopic=85125
 * with permission from the author, Nick Wallette <sirnickity@gmail.com>.
 */

/* BEGIN shameless copy and paste */

// Bitrates - use [version][layer][bitrate]
const uint16_t mpeg_bitrates[4][4][16] = {
  { // Version 2.5
    { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Reserved
    { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 3
    { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 2
    { 0,  32,  48,  56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }  // Layer 1
  },
  { // Reserved
    { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Invalid
    { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Invalid
    { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Invalid
    { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }  // Invalid
  },
  { // Version 2
    { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Reserved
    { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 3
    { 0,   8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 }, // Layer 2
    { 0,  32,  48,  56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }  // Layer 1
  },
  { // Version 1
    { 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 0 }, // Reserved
    { 0,  32,  40,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 }, // Layer 3
    { 0,  32,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 }, // Layer 2
    { 0,  32,  64,  96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }, // Layer 1
  }
};

// Sample rates - use [version][srate]
const uint16_t mpeg_srates[4][4] = {
    { 11025, 12000,  8000, 0 }, // MPEG 2.5
    {     0,     0,     0, 0 }, // Reserved
    { 22050, 24000, 16000, 0 }, // MPEG 2
    { 44100, 48000, 32000, 0 }  // MPEG 1
};

// Samples per frame - use [version][layer]
const uint16_t mpeg_frame_samples[4][4] = {
//    Rsvd     3     2     1  < Layer  v Version
    {    0,  576, 1152,  384 }, //       2.5
    {    0,    0,    0,    0 }, //       Reserved
    {    0,  576, 1152,  384 }, //       2
    {    0, 1152, 1152,  384 }  //       1
};

// Slot size (MPEG unit of measurement) - use [layer]
const uint8_t mpeg_slot_size[4] = { 0, 1, 1, 4 }; // Rsvd, 3, 2, 1

uint16_t
MP3Frame::CalculateLength()
{
  // Lookup real values of these fields
  uint32_t  bitrate   = mpeg_bitrates[mVersion][mLayer][mBitrate] * 1000;
  uint32_t  samprate  = mpeg_srates[mVersion][mSampleRate];
  uint16_t  samples   = mpeg_frame_samples[mVersion][mLayer];
  uint8_t   slot_size = mpeg_slot_size[mLayer];

  // In-between calculations
  float     bps       = (float)samples / 8.0;
  float     fsize     = ( (bps * (float)bitrate) / (float)samprate )
    + ( (mPad) ? slot_size : 0 );

  // Frame sizes are truncated integers
  return (uint16_t)fsize;
}

/* END shameless copy and paste */


/** MP3Parser methods **/

MP3Parser::MP3Parser()
  : mCurrentChar(0)
{ }

void
MP3Parser::Reset()
{
  mCurrentChar = 0;
}

uint16_t
MP3Parser::ParseFrameLength(uint8_t ch)
{
  mData.mRaw[mCurrentChar] = ch;

  MP3Frame &frame = mData.mFrame;

  // Validate MP3 header as we read. We can't mistake the start of an MP3 frame
  // for the middle of another frame due to the sync byte at the beginning
  // of the frame.

  // The only valid position for an all-high byte is the sync byte at the
  // beginning of the frame.
  if (ch == 0xff) {
    mCurrentChar = 0;
  }

  // Make sure the current byte is valid in context. If not, reset the parser.
  if (mCurrentChar == 2) {
    if (frame.mBitrate == 0x0f) {
      goto fail;
    }
  } else if (mCurrentChar == 1) {
    if (frame.mSync2 != 0x07
        || frame.mVersion == 0x01
        || frame.mLayer == 0x00) {
      goto fail;
    }
  }

  // The only valid character at the beginning of the header is 0xff. Fail if
  // it's different.
  if (mCurrentChar == 0 && frame.mSync1 != 0xff) {
    // Couldn't find the sync byte. Fail.
    return 0;
  }

  mCurrentChar++;
  MOZ_ASSERT(mCurrentChar <= sizeof(MP3Frame));

  // Don't have a full header yet.
  if (mCurrentChar < sizeof(MP3Frame)) {
    return 0;
  }

  // Woo, valid header. Return the length.
  mCurrentChar = 0;
  return frame.CalculateLength();

fail:
  Reset();
  return 0;
}

uint32_t
MP3Parser::GetSampleRate()
{
  MP3Frame &frame = mData.mFrame;
  return mpeg_srates[frame.mVersion][frame.mSampleRate];
}


/** ID3Parser methods **/

const char sID3Head[3] = { 'I', 'D', '3' };
const uint32_t ID3_HEADER_LENGTH = 10;

ID3Parser::ID3Parser()
  : mCurrentChar(0)
  , mHeaderLength(0)
{ }

void
ID3Parser::Reset()
{
  mCurrentChar = mHeaderLength = 0;
}

bool
ID3Parser::ParseChar(char ch)
{
  // First three bytes of an ID3v2 header must match the string "ID3".
  if (mCurrentChar < sizeof(sID3Head) / sizeof(*sID3Head)
      && ch != sID3Head[mCurrentChar]) {
    goto fail;
  }

  // The last four bytes of the header is a 28-bit unsigned integer with the
  // high bit of each byte unset.
  if (mCurrentChar >= 6 && mCurrentChar < ID3_HEADER_LENGTH) {
    if (ch & 0x80) {
      goto fail;
    } else {
      mHeaderLength <<= 7;
      mHeaderLength |= ch;
    }
  }

  mCurrentChar++;

  return IsParsed();

fail:
  Reset();
  return false;
}

bool
ID3Parser::IsParsed() const
{
  return mCurrentChar >= ID3_HEADER_LENGTH;
}

uint32_t
ID3Parser::GetHeaderLength() const
{
  MOZ_ASSERT(IsParsed(),
             "Queried length of ID3 header before parsing finished.");
  return mHeaderLength;
}


/** MP3FrameParser methods **/

// Some MP3's have large ID3v2 tags, up to 150KB, so we allow lots of
// skipped bytes to be read, just in case, before we give up and assume
// we're not parsing an MP3 stream.
static const uint32_t MAX_SKIPPED_BYTES = 200 * 1024;

// The number of audio samples per MP3 frame. This is constant over all MP3
// streams. With this constant, the stream's sample rate, and an estimated
// number of frames in the stream, we can estimate the stream's duration
// fairly accurately.
static const uint32_t SAMPLES_PER_FRAME = 1152;

enum {
  MP3_HEADER_LENGTH   = 4,
};

MP3FrameParser::MP3FrameParser(int64_t aLength)
: mLock("MP3FrameParser.mLock"),
  mTotalFrameSize(0),
  mNumFrames(0),
  mOffset(0),
  mLength(aLength),
  mMP3Offset(-1),
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

  const uint8_t *buffer = aBuffer;
  const uint8_t *bufferEnd = aBuffer + aLength;

  // If we haven't found any MP3 frame data yet, there might be ID3 headers
  // we can skip over.
  if (mMP3Offset < 0) {
    for (const uint8_t *ch = buffer; ch < bufferEnd; ch++) {
      if (mID3Parser.ParseChar(*ch)) {
        // Found an ID3 header. We don't care about the body of the header, so
        // just skip past.
        buffer = ch + mID3Parser.GetHeaderLength() - (ID3_HEADER_LENGTH - 1);
        ch = buffer;

        // Yes, this is an MP3!
        mIsMP3 = DEFINITELY_MP3;

        mID3Parser.Reset();
      }
    }
  }

  while (buffer < bufferEnd) {
    uint16_t frameLen = mMP3Parser.ParseFrameLength(*buffer);

    if (frameLen) {

      if (mMP3Offset < 0) {
        // Found our first frame: mark this stream as MP3 and let the decoder
        // know where in the stream the MP3 data starts.
        mIsMP3 = DEFINITELY_MP3;
        // We're at the last byte of an MP3Frame, so MP3 data started
        // sizeof - 1 bytes ago.
        mMP3Offset = aStreamOffset
          + (buffer - aBuffer)
          - (sizeof(MP3Frame) - 1);
      }

      mSampleRate = mMP3Parser.GetSampleRate();
      mTotalFrameSize += frameLen;
      mNumFrames++;

      buffer += frameLen - sizeof(MP3Frame);
    } else {
      buffer++;
    }
  }

  *aOutBytesRead = buffer - aBuffer;
  return NS_OK;
}

void MP3FrameParser::Parse(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  MutexAutoLock mon(mLock);

  const uint8_t* buffer = reinterpret_cast<const uint8_t*>(aBuffer);
  int32_t length = aLength;
  int64_t offset = aOffset;

  // Got some data we have seen already. Skip forward to what we need.
  if (aOffset < mOffset) {
    buffer += mOffset - aOffset;
    length -= mOffset - aOffset;
    offset = mOffset;

    if (length <= 0) {
      return;
    }
  }

  // If there is a discontinuity in the input stream, reset the state of the
  // parsers so we don't get any partial headers.
  if (mOffset < aOffset) {
    if (!mID3Parser.IsParsed()) {
      // Only reset this if it hasn't finished yet.
      mID3Parser.Reset();
    }
    mMP3Parser.Reset();
  }

  uint32_t bytesRead = 0;
  if (NS_FAILED(ParseBuffer(buffer,
                            length,
                            offset,
                            &bytesRead))) {
    return;
  }

  MOZ_ASSERT(length <= (int)bytesRead, "All bytes should have been consumed");

  // Update next data offset
  mOffset = offset + bytesRead;

  // If we've parsed lots of data and we still have nothing, just give up.
  if (!mID3Parser.IsParsed() && !mNumFrames && mOffset > MAX_SKIPPED_BYTES) {
    mIsMP3 = NOT_MP3;
  }
}

int64_t MP3FrameParser::GetDuration()
{
  MutexAutoLock mon(mLock);

  if (!mNumFrames || !mSampleRate) {
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
