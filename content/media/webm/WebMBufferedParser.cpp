/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAlgorithm.h"
#include "WebMBufferedParser.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsThreadUtils.h"
#include <algorithm>

namespace mozilla {

static uint32_t
VIntLength(unsigned char aFirstByte, uint32_t* aMask)
{
  uint32_t count = 1;
  uint32_t mask = 1 << 7;
  while (count < 8) {
    if ((aFirstByte & mask) != 0) {
      break;
    }
    mask >>= 1;
    count += 1;
  }
  if (aMask) {
    *aMask = mask;
  }
  NS_ASSERTION(count >= 1 && count <= 8, "Insane VInt length.");
  return count;
}

void WebMBufferedParser::Append(const unsigned char* aBuffer, uint32_t aLength,
                                nsTArray<WebMTimeDataOffset>& aMapping,
                                ReentrantMonitor& aReentrantMonitor)
{
  static const uint32_t SEGMENT_ID = 0x18538067;
  static const uint32_t SEGINFO_ID = 0x1549a966;
  static const uint32_t CLUSTER_ID = 0x1f43b675;
  static const uint32_t TIMECODESCALE_ID = 0x2ad7b1;
  static const unsigned char TIMECODE_ID = 0xe7;
  static const unsigned char BLOCK_ID = 0xa1;
  static const unsigned char SIMPLEBLOCK_ID = 0xa3;
  static const uint32_t BLOCK_TIMECODE_LENGTH = 2;

  const unsigned char* p = aBuffer;

  // Parse each byte in aBuffer one-by-one, producing timecodes and updating
  // aMapping as we go.  Parser pauses at end of stream (which may be at any
  // point within the parse) and resumes parsing the next time Append is
  // called with new data.
  while (p < aBuffer + aLength) {
    switch (mState) {
    case READ_ELEMENT_ID:
      mVIntRaw = true;
      mState = READ_VINT;
      mNextState = READ_ELEMENT_SIZE;
      break;
    case READ_ELEMENT_SIZE:
      mVIntRaw = false;
      mElement.mID = mVInt;
      mState = READ_VINT;
      mNextState = PARSE_ELEMENT;
      break;
    case PARSE_ELEMENT:
      mElement.mSize = mVInt;
      switch (mElement.mID.mValue) {
      case SEGMENT_ID:
        mState = READ_ELEMENT_ID;
        break;
      case SEGINFO_ID:
        mGotTimecodeScale = true;
        mState = READ_ELEMENT_ID;
        break;
      case TIMECODE_ID:
        mVInt = VInt();
        mVIntLeft = mElement.mSize.mValue;
        mState = READ_VINT_REST;
        mNextState = READ_CLUSTER_TIMECODE;
        break;
      case TIMECODESCALE_ID:
        mVInt = VInt();
        mVIntLeft = mElement.mSize.mValue;
        mState = READ_VINT_REST;
        mNextState = READ_TIMECODESCALE;
        break;
      case CLUSTER_ID:
        mClusterOffset = mCurrentOffset + (p - aBuffer) -
                        (mElement.mID.mLength + mElement.mSize.mLength);
        mState = READ_ELEMENT_ID;
        break;
      case SIMPLEBLOCK_ID:
        /* FALLTHROUGH */
      case BLOCK_ID:
        mBlockSize = mElement.mSize.mValue;
        mBlockTimecode = 0;
        mBlockTimecodeLength = BLOCK_TIMECODE_LENGTH;
        mBlockOffset = mCurrentOffset + (p - aBuffer) -
                       (mElement.mID.mLength + mElement.mSize.mLength);
        mState = READ_VINT;
        mNextState = READ_BLOCK_TIMECODE;
        break;
      default:
        mSkipBytes = mElement.mSize.mValue;
        mState = SKIP_DATA;
        mNextState = READ_ELEMENT_ID;
        break;
      }
      break;
    case READ_VINT: {
      unsigned char c = *p++;
      uint32_t mask;
      mVInt.mLength = VIntLength(c, &mask);
      mVIntLeft = mVInt.mLength - 1;
      mVInt.mValue = mVIntRaw ? c : c & ~mask;
      mState = READ_VINT_REST;
      break;
    }
    case READ_VINT_REST:
      if (mVIntLeft) {
        mVInt.mValue <<= 8;
        mVInt.mValue |= *p++;
        mVIntLeft -= 1;
      } else {
        mState = mNextState;
      }
      break;
    case READ_TIMECODESCALE:
      mTimecodeScale = mVInt.mValue;
      mState = READ_ELEMENT_ID;
      break;
    case READ_CLUSTER_TIMECODE:
      mClusterTimecode = mVInt.mValue;
      mState = READ_ELEMENT_ID;
      break;
    case READ_BLOCK_TIMECODE:
      if (mBlockTimecodeLength) {
        mBlockTimecode <<= 8;
        mBlockTimecode |= *p++;
        mBlockTimecodeLength -= 1;
      } else {
        // It's possible we've parsed this data before, so avoid inserting
        // duplicate WebMTimeDataOffset entries.
        {
          ReentrantMonitorAutoEnter mon(aReentrantMonitor);
          int64_t endOffset = mBlockOffset + mBlockSize +
                              mElement.mID.mLength + mElement.mSize.mLength;
          uint32_t idx = aMapping.IndexOfFirstElementGt(endOffset);
          if (idx == 0 || aMapping[idx - 1] != endOffset) {
            // Don't insert invalid negative timecodes.
            if (mBlockTimecode >= 0 || mClusterTimecode >= uint16_t(abs(mBlockTimecode))) {
              MOZ_ASSERT(mGotTimecodeScale);
              uint64_t absTimecode = mClusterTimecode + mBlockTimecode;
              absTimecode *= mTimecodeScale;
              WebMTimeDataOffset entry(endOffset, absTimecode, mClusterOffset);
              aMapping.InsertElementAt(idx, entry);
            }
          }
        }

        // Skip rest of block header and the block's payload.
        mBlockSize -= mVInt.mLength;
        mBlockSize -= BLOCK_TIMECODE_LENGTH;
        mSkipBytes = uint32_t(mBlockSize);
        mState = SKIP_DATA;
        mNextState = READ_ELEMENT_ID;
      }
      break;
    case SKIP_DATA:
      if (mSkipBytes) {
        uint32_t left = aLength - (p - aBuffer);
        left = std::min(left, mSkipBytes);
        p += left;
        mSkipBytes -= left;
      } else {
        mState = mNextState;
      }
      break;
    }
  }

  NS_ASSERTION(p == aBuffer + aLength, "Must have parsed to end of data.");
  mCurrentOffset += aLength;
}

struct SyncOffsetComparator {
  bool Equals(const WebMTimeDataOffset& a, const int64_t& b) const {
    return a.mSyncOffset == b;
  }

  bool LessThan(const WebMTimeDataOffset& a, const int64_t& b) const {
    return a.mSyncOffset < b;
  }
};

bool WebMBufferedState::CalculateBufferedForRange(int64_t aStartOffset, int64_t aEndOffset,
                                                  uint64_t* aStartTime, uint64_t* aEndTime)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  // Find the first WebMTimeDataOffset at or after aStartOffset.
  uint32_t start = mTimeMapping.IndexOfFirstElementGt(aStartOffset - 1, SyncOffsetComparator());
  if (start == mTimeMapping.Length()) {
    return false;
  }

  // Find the first WebMTimeDataOffset at or before aEndOffset.
  uint32_t end = mTimeMapping.IndexOfFirstElementGt(aEndOffset);
  if (end > 0) {
    end -= 1;
  }

  // Range is empty.
  if (end <= start) {
    return false;
  }

  NS_ASSERTION(mTimeMapping[start].mSyncOffset >= aStartOffset &&
               mTimeMapping[end].mEndOffset <= aEndOffset,
               "Computed time range must lie within data range.");
  if (start > 0) {
    NS_ASSERTION(mTimeMapping[start - 1].mSyncOffset < aStartOffset,
                 "Must have found least WebMTimeDataOffset for start");
  }
  if (end < mTimeMapping.Length() - 1) {
    NS_ASSERTION(mTimeMapping[end + 1].mEndOffset > aEndOffset,
                 "Must have found greatest WebMTimeDataOffset for end");
  }

  uint64_t frameDuration = mTimeMapping[end].mTimecode - mTimeMapping[end - 1].mTimecode;
  *aStartTime = mTimeMapping[start].mTimecode;
  *aEndTime = mTimeMapping[end].mTimecode + frameDuration;
  return true;
}

bool WebMBufferedState::GetOffsetForTime(uint64_t aTime, int64_t* aOffset)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  WebMTimeDataOffset result(0, 0, 0);

  for (uint32_t i = 0; i < mTimeMapping.Length(); ++i) {
    WebMTimeDataOffset o = mTimeMapping[i];
    if (o.mTimecode < aTime && o.mTimecode > result.mTimecode) {
      result = o;
    }
  }

  *aOffset = result.mSyncOffset;
  return true;
}

void WebMBufferedState::NotifyDataArrived(const char* aBuffer, uint32_t aLength, int64_t aOffset)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  uint32_t idx = mRangeParsers.IndexOfFirstElementGt(aOffset - 1);
  if (idx == 0 || !(mRangeParsers[idx-1] == aOffset)) {
    // If the incoming data overlaps an already parsed range, adjust the
    // buffer so that we only reparse the new data.  It's also possible to
    // have an overlap where the end of the incoming data is within an
    // already parsed range, but we don't bother handling that other than by
    // avoiding storing duplicate timecodes when the parser runs.
    if (idx != mRangeParsers.Length() && mRangeParsers[idx].mStartOffset <= aOffset) {
      // Complete overlap, skip parsing.
      if (aOffset + aLength <= mRangeParsers[idx].mCurrentOffset) {
        return;
      }

      // Partial overlap, adjust the buffer to parse only the new data.
      int64_t adjust = mRangeParsers[idx].mCurrentOffset - aOffset;
      NS_ASSERTION(adjust >= 0, "Overlap detection bug.");
      aBuffer += adjust;
      aLength -= uint32_t(adjust);
    } else {
      mRangeParsers.InsertElementAt(idx, WebMBufferedParser(aOffset));
      if (idx != 0) {
        mRangeParsers[idx].SetTimecodeScale(mRangeParsers[0].GetTimecodeScale());
      }
    }
  }

  mRangeParsers[idx].Append(reinterpret_cast<const unsigned char*>(aBuffer),
                            aLength,
                            mTimeMapping,
                            mReentrantMonitor);

  // Merge parsers with overlapping regions and clean up the remnants.
  uint32_t i = 0;
  while (i + 1 < mRangeParsers.Length()) {
    if (mRangeParsers[i].mCurrentOffset >= mRangeParsers[i + 1].mStartOffset) {
      mRangeParsers[i + 1].mStartOffset = mRangeParsers[i].mStartOffset;
      mRangeParsers.RemoveElementAt(i);
    } else {
      i += 1;
    }
  }
}

} // namespace mozilla

