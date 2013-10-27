/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include "mozilla/Mutex.h"

namespace mozilla {

// Simple parser to tell whether we've found an ID3 header and how long it is,
// so that we can skip it.
// XXX maybe actually parse this stuff?
class ID3Parser
{
public:
  ID3Parser();

  void Reset();
  bool ParseChar(char ch);
  bool IsParsed() const;
  uint32_t GetHeaderLength() const;

private:
  uint32_t mCurrentChar;
  uint32_t mHeaderLength;
};

struct MP3Frame {
  uint16_t mSync1 : 8;      // Always all set
  uint16_t mProtected : 1;  // Ignored
  uint16_t mLayer : 2;
  uint16_t mVersion : 2;
  uint16_t mSync2 : 3;      // Always all set
  uint16_t mPrivate : 1;    // Ignored
  uint16_t mPad : 1;
  uint16_t mSampleRate : 2; // Index into mpeg_srates above
  uint16_t mBitrate : 4;    // Index into mpeg_bitrates above

  uint16_t CalculateLength();
};

// Buffering parser for MP3 frames.
class MP3Parser
{
public:
  MP3Parser();

  // Forget all data the parser has seen so far.
  void Reset();

  // Parse the given byte. If we have found a frame header, return the length of
  // the frame.
  uint16_t ParseFrameLength(uint8_t ch);

  // Get the sample rate from the current header.
  uint32_t GetSampleRate();

private:
  uint32_t mCurrentChar;
  union {
    uint8_t mRaw[3];
    MP3Frame mFrame;
  } mData;
};


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

  // A low-contention lock for protecting the parser results
  Mutex mLock;

  // ID3 header parser. Keeps state between reads in case the header falls
  // in between.
  ID3Parser mID3Parser;

  // MP3 frame header parser.
  MP3Parser mMP3Parser;

  // All fields below are protected by mLock
  uint64_t mTotalFrameSize;
  uint64_t mNumFrames;

  // Offset of the last data parsed. This is the end offset of the last data
  // block parsed, so it's the start offset we expect to get on the next
  // call to Parse().
  int64_t  mOffset;

  // Total length of the stream in bytes.
  int64_t  mLength;

  // Offset of first MP3 frame in the bitstream. Has value -1 until the
  // first MP3 frame is found.
  int64_t mMP3Offset;

  // Number of audio samples per second. Fixed through the whole file.
  uint16_t mSampleRate;

  enum eIsMP3 {
    MAYBE_MP3, // We're giving the stream the benefit of the doubt...
    DEFINITELY_MP3, // We've hit at least one ID3 tag or MP3 frame.
    NOT_MP3 // Not found any evidence of the stream being MP3.
  };

  eIsMP3 mIsMP3;

};

}
