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
 * The Initial Developer of the Original Code is the Mozilla Corporation.
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
#if !defined(nsWebMBufferedParser_h_)
#define nsWebMBufferedParser_h_

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "mozilla/ReentrantMonitor.h"

class nsTimeRanges;
using mozilla::ReentrantMonitor;

// Stores a stream byte offset and the scaled timecode of the block at
// that offset.  The timecode must be scaled by the stream's timecode
// scale before use.
struct nsWebMTimeDataOffset
{
  nsWebMTimeDataOffset(PRInt64 aOffset, PRUint64 aTimecode)
    : mOffset(aOffset), mTimecode(aTimecode)
  {}

  bool operator==(PRInt64 aOffset) const {
    return mOffset == aOffset;
  }

  bool operator<(PRInt64 aOffset) const {
    return mOffset < aOffset;
  }

  PRInt64 mOffset;
  PRUint64 mTimecode;
};

// A simple WebM parser that produces data offset to timecode pairs as it
// consumes blocks.  A new parser is created for each distinct range of data
// received and begins parsing from the first WebM cluster within that
// range.  Old parsers are destroyed when their range merges with a later
// parser or an already parsed range.  The parser may start at any position
// within the stream.
struct nsWebMBufferedParser
{
  nsWebMBufferedParser(PRInt64 aOffset)
    : mStartOffset(aOffset), mCurrentOffset(aOffset), mState(CLUSTER_SYNC), mClusterIDPos(0)
  {}

  // Steps the parser through aLength bytes of data.  Always consumes
  // aLength bytes.  Updates mCurrentOffset before returning.  Acquires
  // aReentrantMonitor before using aMapping.
  void Append(const unsigned char* aBuffer, PRUint32 aLength,
              nsTArray<nsWebMTimeDataOffset>& aMapping,
              ReentrantMonitor& aReentrantMonitor);

  bool operator==(PRInt64 aOffset) const {
    return mCurrentOffset == aOffset;
  }

  bool operator<(PRInt64 aOffset) const {
    return mCurrentOffset < aOffset;
  }

  // The offset at which this parser started parsing.  Used to merge
  // adjacent parsers, in which case the later parser adopts the earlier
  // parser's mStartOffset.
  PRInt64 mStartOffset;

  // Current offset with the stream.  Updated in chunks as Append() consumes
  // data.
  PRInt64 mCurrentOffset;

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
  PRUint32 mClusterIDPos;

  // Variable length integer read from data.
  PRUint64 mVInt;

  // Encoding length of mVInt.  This is the total number of bytes used to
  // encoding mVInt's value.
  PRUint32 mVIntLength;

  // Number of bytes of mVInt left to read.  mVInt is complete once this
  // reaches 0.
  PRUint32 mVIntLeft;

  // Size of the block currently being parsed.  Any unused data within the
  // block is skipped once the block timecode has been parsed.
  PRUint64 mBlockSize;

  // Cluster-level timecode.
  PRUint64 mClusterTimecode;

  // Start offset of the block currently being parsed.  Used as the byte
  // offset for the offset-to-time mapping once the block timecode has been
  // parsed.
  PRInt64 mBlockOffset;

  // Block-level timecode.  This is summed with mClusterTimecode to produce
  // an absolute timecode for the offset-to-time mapping.
  PRInt16 mBlockTimecode;

  // Number of bytes of mBlockTimecode left to read.
  PRUint32 mBlockTimecodeLength;

  // Count of bytes left to skip before resuming parse at mNextState.
  // Mostly used to skip block payload data after reading a block timecode.
  PRUint32 mSkipBytes;
};

class nsWebMBufferedState
{
  NS_INLINE_DECL_REFCOUNTING(nsWebMBufferedState)

public:
  nsWebMBufferedState() : mReentrantMonitor("nsWebMBufferedState") {
    MOZ_COUNT_CTOR(nsWebMBufferedState);
  }

  ~nsWebMBufferedState() {
    MOZ_COUNT_DTOR(nsWebMBufferedState);
  }

  void NotifyDataArrived(const char* aBuffer, PRUint32 aLength, PRInt64 aOffset);
  bool CalculateBufferedForRange(PRInt64 aStartOffset, PRInt64 aEndOffset,
                                 PRUint64* aStartTime, PRUint64* aEndTime);

private:
  // Synchronizes access to the mTimeMapping array.
  ReentrantMonitor mReentrantMonitor;

  // Sorted (by offset) map of data offsets to timecodes.  Populated
  // on the main thread as data is received and parsed by nsWebMBufferedParsers.
  nsTArray<nsWebMTimeDataOffset> mTimeMapping;

  // Sorted (by offset) live parser instances.  Main thread only.
  nsTArray<nsWebMBufferedParser> mRangeParsers;
};

#endif
