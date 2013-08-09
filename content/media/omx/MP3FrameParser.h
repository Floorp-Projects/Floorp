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
    return mIsMP3;
  }

  void Parse(const uint8_t* aBuffer, uint32_t aLength, int64_t aOffset);

  void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset);

  int64_t GetDuration();

private:
  size_t ParseInternalBuffer(const uint8_t* aBuffer, uint32_t aLength, int64_t aOffset);

  uint8_t  mBuffer[10];
  uint32_t mBufferLength;

  // A low-contention lock for protecting the parser results
  Mutex mLock;

  // All fields below are protected by mLock
  uint64_t mDurationUs;
  uint64_t mBitRateSum;
  uint64_t mNumFrames;
  int64_t  mOffset;
  int64_t  mUnhandled;
  int64_t  mLength;
  uint32_t mTrailing;

  // Contains the state of the MP3 detection
  bool mIsMP3;
};

}
