/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include "mozilla/Mutex.h"

namespace mozilla {

// A description of the MP3 format and its extensions is available at
//
//  http://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header
//
// The data in MP3 streams is split into small frames, with each frame
// containing a fixed number of samples. The duration of a frame depends
// on the frame's bit rate and sample rate. Both values can vary among
// frames, so it is necessary to examine each individual frame of an MP3
// stream to calculate the stream's overall duration.
//
// The MP3 frame parser extracts information from an MP3 data stream. It
// accepts a range of frames of an MP3 stream as input, and parses all
// frames for their duration. Callers can query the stream's overall
// duration from the parser.
//
// Call the methods NotifyDataArrived or Parse to add new data. If you added
// information for a certain stream position, you cannot go back to previous
// positions. The parser will simply ignore the input. If you skip stream
// positions, the duration of the related MP3 frames will be estimated from
// the stream's average.
//
// The method GetDuration returns calculated duration of the stream, including
// estimates for skipped ranges.
//
// All public methods are thread-safe.

class MP3FrameParser
{
public:
  MP3FrameParser(int64_t aLength=-1);

  bool IsMP3() {
    MutexAutoLock mon(mLock);
    return mIsMP3 != NOT_MP3;
  }

  void Parse(const char* aBuffer, uint32_t aLength, int64_t aStreamOffset);

  // Returns the duration, in microseconds. If the entire stream has not
  // been parsed yet, this is an estimate based on the bitrate of the
  // frames parsed so far.
  int64_t GetDuration();

  // Returns the offset of the first MP3 frame in the stream, or -1 of
  // no MP3 frame has been detected yet.
  int64_t GetMP3Offset();

private:

  // Parses aBuffer, starting at offset 0. Returns the number of bytes
  // parsed, relative to the start of the buffer. Note this may be
  // greater than aLength if the headers in the buffer indicate that
  // the frame or ID3 tag extends outside of aBuffer. Returns failure
  // if too many non-MP3 bytes are parsed.
  nsresult ParseBuffer(const uint8_t* aBuffer,
                       uint32_t aLength,
                       int64_t aStreamOffset,
                       uint32_t* aOutBytesRead);

  // mBuffer must be at least 19 bytes long, in case the last byte in the
  // buffer is the first byte in a 10 byte long ID3 tag header.
  uint8_t  mBuffer[32];
  uint32_t mBufferLength;

  // A low-contention lock for protecting the parser results
  Mutex mLock;

  // All fields below are protected by mLock
  uint64_t mDurationUs;
  uint64_t mBitRateSum;
  uint64_t mNumFrames;

  // Offset of the last data parsed. This is the end offset of the last data
  // block parsed, so it's the start offset we expect to get on the next
  // call to Parse().
  int64_t  mOffset;

  // Count of the number of bytes that the parser hasn't seen so far. This
  // happens when the stream seeks.
  int64_t  mUnhandled;
  int64_t  mLength;
  // Offset of first MP3 frame in the bitstream. Has value -1 until the
  // first MP3 frame is found.
  int64_t mMP3Offset;

  // Count of bytes that have been parsed but skipped over because we couldn't
  // find a sync pattern or an ID3 header. If this gets too high, we assume
  // the stream either isn't MP3, or is corrupt.
  uint32_t mSkippedBytes;

  enum eIsMP3 {
    MAYBE_MP3, // We're giving the stream the benefit of the doubt...
    DEFINITELY_MP3, // We've hit at least one ID3 tag or MP3 frame.
    NOT_MP3 // Not found any evidence of the stream being MP3.
  };

  eIsMP3 mIsMP3;

};

}
