/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WebMBufferedParser_h_)
#define WebMBufferedParser_h_

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {

namespace dom {
class TimeRanges;
}

// Stores a stream byte offset and the scaled timecode of the block at
// that offset.  The timecode must be scaled by the stream's timecode
// scale before use.
struct WebMTimeDataOffset
{
  WebMTimeDataOffset(int64_t aOffset, uint64_t aTimecode)
    : mOffset(aOffset), mTimecode(aTimecode)
  {}

  bool operator==(int64_t aOffset) const {
    return mOffset == aOffset;
  }

  bool operator<(int64_t aOffset) const {
    return mOffset < aOffset;
  }

  int64_t mOffset;
  uint64_t mTimecode;
};

// A simple WebM parser that produces data offset to timecode pairs as it
// consumes blocks.  A new parser is created for each distinct range of data
// received and begins parsing from the first WebM cluster within that
// range.  Old parsers are destroyed when their range merges with a later
// parser or an already parsed range.  The parser may start at any position
// within the stream.
struct WebMBufferedParser
{
  WebMBufferedParser(int64_t aOffset)
    : mStartOffset(aOffset), mCurrentOffset(aOffset), mState(CLUSTER_SYNC), mClusterIDPos(0)
  {}

  // Steps the parser through aLength bytes of data.  Always consumes
  // aLength bytes.  Updates mCurrentOffset before returning.  Acquires
  // aReentrantMonitor before using aMapping.
  void Append(const unsigned char* aBuffer, uint32_t aLength,
              nsTArray<WebMTimeDataOffset>& aMapping,
              ReentrantMonitor& aReentrantMonitor);

  bool operator==(int64_t aOffset) const {
    return mCurrentOffset == aOffset;
  }

  bool operator<(int64_t aOffset) const {
    return mCurrentOffset < aOffset;
  }

  // The offset at which this parser started parsing.  Used to merge
  // adjacent parsers, in which case the later parser adopts the earlier
  // parser's mStartOffset.
  int64_t mStartOffset;

  // Current offset with the stream.  Updated in chunks as Append() consumes
  // data.
  int64_t mCurrentOffset;

private:
  enum State {
    // Parser start state.  Scans forward searching for stream sync by
    // matching CLUSTER_ID with the curernt byte.  The match state is stored
    // in mClusterIDPos.  Once this reaches sizeof(CLUSTER_ID), stream may
    // have sync.  The parser then advances to read the cluster size and
    // timecode.
    CLUSTER_SYNC,

    /*
      The the parser states below assume that CLUSTER_SYNC has found a valid
      sync point within the data.  If parsing fails in these states, the
      parser returns to CLUSTER_SYNC to find a new sync point.
    */

    // Read the first byte of a variable length integer.  The first byte
    // encodes both the variable integer's length and part of the value.
    // The value read so far is stored in mVInt and the length is stored in
    // mVIntLength.  The number of bytes left to read is stored in
    // mVIntLeft.
    READ_VINT,

    // Reads the remaining mVIntLeft bytes into mVInt.
    READ_VINT_REST,

    // Check that the next element is TIMECODE_ID.  The cluster timecode is
    // required to be the first element in a cluster.  Advances to READ_VINT
    // to read the timecode's length into mVInt.
    TIMECODE_SYNC,

    // mVInt holds the length of the variable length unsigned integer
    // containing the cluster timecode.  Read mVInt bytes into
    // mClusterTimecode.
    READ_CLUSTER_TIMECODE,

    // Skips elements with a cluster until BLOCKGROUP_ID or SIMPLEBLOCK_ID
    // is found.  If BLOCKGROUP_ID is found, the parser returns to
    // ANY_BLOCK_ID searching for a BLOCK_ID.  Once a block or simpleblock
    // is found, the current data offset is stored in mBlockOffset.  If the
    // current byte is the beginning of a four byte variant integer, it
    // indicates the parser has reached a top-level element ID and the
    // parser returns to CLUSTER_SYNC.
    ANY_BLOCK_SYNC,

    // Start reading a block.  Blocks and simpleblocks are parsed the same
    // way as the initial layouts are identical.  mBlockSize is initialized
    // from mVInt (holding the element size), and mBlockTimecode(Length) is
    // initialized for parsing.
    READ_BLOCK,

    // Reads mBlockTimecodeLength bytes of data into mBlockTimecode.  When
    // mBlockTimecodeLength reaches 0, the timecode has been read.  The sum
    // of mClusterTimecode and mBlockTimecode is stored as a pair with
    // mBlockOffset into the offset-to-time map.
    READ_BLOCK_TIMECODE,

    // Skip mSkipBytes of data before resuming parse at mNextState.
    SKIP_DATA,

    // Skip the content of an element.  mVInt holds the element length.
    SKIP_ELEMENT
  };

  // Current state machine action.
  State mState;

  // Next state machine action.  SKIP_DATA and READ_VINT_REST advance to
  // mNextState when the current action completes.
  State mNextState;

  // Match position within CLUSTER_ID.  Used to find sync within arbitrary
  // data.
  uint32_t mClusterIDPos;

  // Variable length integer read from data.
  uint64_t mVInt;

  // Encoding length of mVInt.  This is the total number of bytes used to
  // encoding mVInt's value.
  uint32_t mVIntLength;

  // Number of bytes of mVInt left to read.  mVInt is complete once this
  // reaches 0.
  uint32_t mVIntLeft;

  // Size of the block currently being parsed.  Any unused data within the
  // block is skipped once the block timecode has been parsed.
  uint64_t mBlockSize;

  // Cluster-level timecode.
  uint64_t mClusterTimecode;

  // Start offset of the block currently being parsed.  Used as the byte
  // offset for the offset-to-time mapping once the block timecode has been
  // parsed.
  int64_t mBlockOffset;

  // Block-level timecode.  This is summed with mClusterTimecode to produce
  // an absolute timecode for the offset-to-time mapping.
  int16_t mBlockTimecode;

  // Number of bytes of mBlockTimecode left to read.
  uint32_t mBlockTimecodeLength;

  // Count of bytes left to skip before resuming parse at mNextState.
  // Mostly used to skip block payload data after reading a block timecode.
  uint32_t mSkipBytes;
};

class WebMBufferedState
{
  NS_INLINE_DECL_REFCOUNTING(WebMBufferedState)

public:
  WebMBufferedState() : mReentrantMonitor("WebMBufferedState") {
    MOZ_COUNT_CTOR(WebMBufferedState);
  }

  ~WebMBufferedState() {
    MOZ_COUNT_DTOR(WebMBufferedState);
  }

  void NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset);
  bool CalculateBufferedForRange(int64_t aStartOffset, int64_t aEndOffset,
                                 uint64_t* aStartTime, uint64_t* aEndTime);

private:
  // Synchronizes access to the mTimeMapping array.
  ReentrantMonitor mReentrantMonitor;

  // Sorted (by offset) map of data offsets to timecodes.  Populated
  // on the main thread as data is received and parsed by WebMBufferedParsers.
  nsTArray<WebMTimeDataOffset> mTimeMapping;

  // Sorted (by offset) live parser instances.  Main thread only.
  nsTArray<WebMBufferedParser> mRangeParsers;
};

} // namespace mozilla

#endif
