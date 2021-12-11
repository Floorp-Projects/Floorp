/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASEGMENT_H_
#define MOZILLA_MEDIASEGMENT_H_

#include "PrincipalHandle.h"
#include "nsTArray.h"
#ifdef MOZILLA_INTERNAL_API
#  include "mozilla/TimeStamp.h"
#endif
#include <algorithm>

namespace mozilla {

/**
 * Track or graph rate in Hz. Maximum 1 << TRACK_RATE_MAX_BITS Hz. This
 * maximum avoids overflow in conversions between track rates and conversions
 * from seconds.
 */
typedef int32_t TrackRate;
const int64_t TRACK_RATE_MAX_BITS = 20;
const TrackRate TRACK_RATE_MAX = 1 << TRACK_RATE_MAX_BITS;

/**
 * A number of ticks at a rate determined by some underlying track (e.g., audio
 * sample rate). We want to make sure that multiplying TrackTicks by a TrackRate
 * doesn't overflow, so we set its max accordingly.
 * TrackTime should be used instead when we're working with MediaTrackGraph's
 * rate, but TrackTicks can be used outside MediaTracks when we have data at a
 * different rate.
 */
typedef int64_t TrackTicks;
const int64_t TRACK_TICKS_MAX = INT64_MAX >> TRACK_RATE_MAX_BITS;

/**
 * We represent media times in 64-bit audio frame counts or ticks.
 * All tracks in a MediaTrackGraph have the same rate.
 */
typedef int64_t MediaTime;
const int64_t MEDIA_TIME_MAX = TRACK_TICKS_MAX;

/**
 * Media time relative to the start of a MediaTrack.
 */
typedef MediaTime TrackTime;
const TrackTime TRACK_TIME_MAX = MEDIA_TIME_MAX;

/**
 * Media time relative to the start of the graph timeline.
 */
typedef MediaTime GraphTime;
const GraphTime GRAPH_TIME_MAX = MEDIA_TIME_MAX;

/* Time conversion helper functions */
inline TrackTicks RateConvertTicksRoundDown(TrackRate aOutRate,
                                            TrackRate aInRate,
                                            TrackTicks aTicks) {
  MOZ_ASSERT(0 < aOutRate && aOutRate <= TRACK_RATE_MAX, "Bad out rate");
  MOZ_ASSERT(0 < aInRate && aInRate <= TRACK_RATE_MAX, "Bad in rate");
  MOZ_ASSERT(0 <= aTicks && aTicks <= TRACK_TICKS_MAX, "Bad ticks");
  return (aTicks * aOutRate) / aInRate;
}

inline TrackTicks RateConvertTicksRoundUp(TrackRate aOutRate, TrackRate aInRate,
                                          TrackTicks aTicks) {
  MOZ_ASSERT(0 < aOutRate && aOutRate <= TRACK_RATE_MAX, "Bad out rate");
  MOZ_ASSERT(0 < aInRate && aInRate <= TRACK_RATE_MAX, "Bad in rate");
  MOZ_ASSERT(0 <= aTicks && aTicks <= TRACK_TICKS_MAX, "Bad ticks");
  return (aTicks * aOutRate + aInRate - 1) / aInRate;
}

/**
 * The number of chunks allocated by default for a MediaSegment.
 * Appending more chunks than this will cause further allocations.
 *
 * 16 is an arbitrary number intended to cover the most common cases in the
 * MediaTrackGraph (1 with silence and 1-2 with data for a realtime track)
 * with some margin.
 */
const size_t DEFAULT_SEGMENT_CAPACITY = 16;

/**
 * A MediaSegment is a chunk of media data sequential in time. Different
 * types of data have different subclasses of MediaSegment, all inheriting
 * from MediaSegmentBase.
 * All MediaSegment data is timed using TrackTime. The actual tick rate
 * is defined on a per-track basis. For some track types, this can be
 * a fixed constant for all tracks of that type (e.g. 1MHz for video).
 *
 * Each media segment defines a concept of "null media data" (e.g. silence
 * for audio or "no video frame" for video), which can be efficiently
 * represented. This is used for padding.
 */
class MediaSegment {
 public:
  MediaSegment(const MediaSegment&) = delete;
  MediaSegment& operator=(const MediaSegment&) = delete;

  MOZ_COUNTED_DTOR_VIRTUAL(MediaSegment)

  enum Type { AUDIO, VIDEO, TYPE_COUNT };

  /**
   * Gets the total duration of the segment.
   */
  TrackTime GetDuration() const { return mDuration; }
  Type GetType() const { return mType; }

  /**
   * Gets the last principal id that was appended to this segment.
   */
  const PrincipalHandle& GetLastPrincipalHandle() const {
    return mLastPrincipalHandle;
  }
  /**
   * Called by the MediaTrackGraph as it appends a chunk with a different
   * principal id than the current one.
   */
  void SetLastPrincipalHandle(PrincipalHandle aLastPrincipalHandle) {
    mLastPrincipalHandle = std::forward<PrincipalHandle>(aLastPrincipalHandle);
  }

  /**
   * Returns true if all chunks in this segment are null.
   */
  virtual bool IsNull() const = 0;

  /**
   * Returns true if this segment contains no chunks.
   */
  virtual bool IsEmpty() const = 0;

  /**
   * Create a MediaSegment of the same type.
   */
  virtual MediaSegment* CreateEmptyClone() const = 0;
  /**
   * Moves contents of aSource to the end of this segment.
   */
  virtual void AppendFrom(MediaSegment* aSource) = 0;
  /**
   * Append a slice of aSource to this segment.
   */
  virtual void AppendSlice(const MediaSegment& aSource, TrackTime aStart,
                           TrackTime aEnd) = 0;
  /**
   * Replace all contents up to aDuration with null data.
   */
  virtual void ForgetUpTo(TrackTime aDuration) = 0;
  /**
   * Forget all data buffered after a given point
   */
  virtual void FlushAfter(TrackTime aNewEnd) = 0;
  /**
   * Insert aDuration of null data at the start of the segment.
   */
  virtual void InsertNullDataAtStart(TrackTime aDuration) = 0;
  /**
   * Insert aDuration of null data at the end of the segment.
   */
  virtual void AppendNullData(TrackTime aDuration) = 0;
  /**
   * Replace contents with disabled (silence/black) data of the same duration
   */
  virtual void ReplaceWithDisabled() = 0;
  /**
   * Replace contents with null data of the same duration
   */
  virtual void ReplaceWithNull() = 0;
  /**
   * Remove all contents, setting duration to 0.
   */
  virtual void Clear() = 0;

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    return 0;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 protected:
  explicit MediaSegment(Type aType)
      : mDuration(0),
        mType(aType),
        mLastPrincipalHandle(PRINCIPAL_HANDLE_NONE) {
    MOZ_COUNT_CTOR(MediaSegment);
  }

  MediaSegment(MediaSegment&& aSegment)
      : mDuration(std::move(aSegment.mDuration)),
        mType(std::move(aSegment.mType)),
        mLastPrincipalHandle(std::move(aSegment.mLastPrincipalHandle)) {
    MOZ_COUNT_CTOR(MediaSegment);
  }

  TrackTime mDuration;  // total of mDurations of all chunks
  Type mType;

  // The latest principal handle that the MediaTrackGraph has processed for
  // this segment.
  PrincipalHandle mLastPrincipalHandle;
};

/**
 * C is the implementation class subclassed from MediaSegmentBase.
 * C must contain a Chunk class.
 */
template <class C, class Chunk>
class MediaSegmentBase : public MediaSegment {
 public:
  bool IsNull() const override {
    for (typename C::ConstChunkIterator iter(*this); !iter.IsEnded();
         iter.Next()) {
      if (!iter->IsNull()) {
        return false;
      }
    }
    return true;
  }
  bool IsEmpty() const override { return mChunks.IsEmpty(); }
  MediaSegment* CreateEmptyClone() const override { return new C(); }
  void AppendFrom(MediaSegment* aSource) override {
    NS_ASSERTION(aSource->GetType() == C::StaticType(), "Wrong type");
    AppendFromInternal(static_cast<C*>(aSource));
  }
  void AppendFrom(C* aSource) { AppendFromInternal(aSource); }
  void AppendSlice(const MediaSegment& aSource, TrackTime aStart,
                   TrackTime aEnd) override {
    NS_ASSERTION(aSource.GetType() == C::StaticType(), "Wrong type");
    AppendSliceInternal(static_cast<const C&>(aSource), aStart, aEnd);
  }
  void AppendSlice(const C& aOther, TrackTime aStart, TrackTime aEnd) {
    AppendSliceInternal(aOther, aStart, aEnd);
  }
  /**
   * Replace the first aDuration ticks with null media data, because the data
   * will not be required again.
   */
  void ForgetUpTo(TrackTime aDuration) override {
    if (mChunks.IsEmpty() || aDuration <= 0) {
      return;
    }
    if (mChunks[0].IsNull()) {
      TrackTime extraToForget =
          std::min(aDuration, mDuration) - mChunks[0].GetDuration();
      if (extraToForget > 0) {
        RemoveLeading(extraToForget, 1);
        mChunks[0].mDuration += extraToForget;
        mDuration += extraToForget;
      }
      return;
    }
    RemoveLeading(aDuration, 0);
    mChunks.InsertElementAt(0)->SetNull(aDuration);
    mDuration += aDuration;
  }
  void FlushAfter(TrackTime aNewEnd) override {
    if (mChunks.IsEmpty()) {
      return;
    }

    if (!aNewEnd) {
      Clear();
    } else if (mChunks[0].IsNull()) {
      TrackTime extraToKeep = aNewEnd - mChunks[0].GetDuration();
      if (extraToKeep < 0) {
        // reduce the size of the Null, get rid of everthing else
        mChunks[0].SetNull(aNewEnd);
        extraToKeep = 0;
      }
      RemoveTrailing(extraToKeep, 1);
    } else {
      if (aNewEnd > mDuration) {
        NS_ASSERTION(aNewEnd <= mDuration, "can't add data in FlushAfter");
        return;
      }
      RemoveTrailing(aNewEnd, 0);
    }
    mDuration = aNewEnd;
  }
  void InsertNullDataAtStart(TrackTime aDuration) override {
    if (aDuration <= 0) {
      return;
    }
    if (!mChunks.IsEmpty() && mChunks[0].IsNull()) {
      mChunks[0].mDuration += aDuration;
    } else {
      mChunks.InsertElementAt(0)->SetNull(aDuration);
    }
    mDuration += aDuration;
  }
  void AppendNullData(TrackTime aDuration) override {
    if (aDuration <= 0) {
      return;
    }
    if (!mChunks.IsEmpty() && mChunks[mChunks.Length() - 1].IsNull()) {
      mChunks[mChunks.Length() - 1].mDuration += aDuration;
    } else {
      mChunks.AppendElement()->SetNull(aDuration);
    }
    mDuration += aDuration;
  }
  void ReplaceWithDisabled() override {
    if (GetType() != AUDIO) {
      MOZ_CRASH("Disabling unknown segment type");
    }
    ReplaceWithNull();
  }
  void ReplaceWithNull() override {
    TrackTime duration = GetDuration();
    Clear();
    AppendNullData(duration);
  }
  void Clear() override {
    mDuration = 0;
    mChunks.ClearAndRetainStorage();
    mChunks.SetCapacity(DEFAULT_SEGMENT_CAPACITY);
  }

  class ChunkIterator {
   public:
    explicit ChunkIterator(MediaSegmentBase<C, Chunk>& aSegment)
        : mSegment(aSegment), mIndex(0) {}
    bool IsEnded() { return mIndex >= mSegment.mChunks.Length(); }
    void Next() { ++mIndex; }
    Chunk& operator*() { return mSegment.mChunks[mIndex]; }
    Chunk* operator->() { return &mSegment.mChunks[mIndex]; }

   private:
    MediaSegmentBase<C, Chunk>& mSegment;
    uint32_t mIndex;
  };
  class ConstChunkIterator {
   public:
    explicit ConstChunkIterator(const MediaSegmentBase<C, Chunk>& aSegment)
        : mSegment(aSegment), mIndex(0) {}
    bool IsEnded() { return mIndex >= mSegment.mChunks.Length(); }
    void Next() { ++mIndex; }
    const Chunk& operator*() { return mSegment.mChunks[mIndex]; }
    const Chunk* operator->() { return &mSegment.mChunks[mIndex]; }

   private:
    const MediaSegmentBase<C, Chunk>& mSegment;
    uint32_t mIndex;
  };

  void RemoveLeading(TrackTime aDuration) { RemoveLeading(aDuration, 0); }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t amount = mChunks.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < mChunks.Length(); i++) {
      amount += mChunks[i].SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  Chunk* GetLastChunk() {
    if (mChunks.IsEmpty()) {
      return nullptr;
    }
    return &mChunks[mChunks.Length() - 1];
  }

  const Chunk* GetLastChunk() const {
    if (mChunks.IsEmpty()) {
      return nullptr;
    }
    return &mChunks[mChunks.Length() - 1];
  }

 protected:
  explicit MediaSegmentBase(Type aType) : MediaSegment(aType), mChunks() {}

  MediaSegmentBase(MediaSegmentBase&& aSegment)
      : MediaSegment(std::move(aSegment)),
        mChunks(std::move(aSegment.mChunks)) {
    MOZ_ASSERT(mChunks.Capacity() >= DEFAULT_SEGMENT_CAPACITY,
               "Capacity must be retained in self after swap");
    MOZ_ASSERT(aSegment.mChunks.Capacity() >= DEFAULT_SEGMENT_CAPACITY,
               "Capacity must be retained in other after swap");
  }

  /**
   * Appends the contents of aSource to this segment, clearing aSource.
   */
  void AppendFromInternal(MediaSegmentBase<C, Chunk>* aSource) {
    MOZ_ASSERT(aSource->mDuration >= 0);
    mDuration += aSource->mDuration;
    aSource->mDuration = 0;
    size_t offset = 0;
    if (!mChunks.IsEmpty() && !aSource->mChunks.IsEmpty() &&
        mChunks[mChunks.Length() - 1].CanCombineWithFollowing(
            aSource->mChunks[0])) {
      mChunks[mChunks.Length() - 1].mDuration += aSource->mChunks[0].mDuration;
      offset = 1;
    }

    for (; offset < aSource->mChunks.Length(); ++offset) {
      mChunks.AppendElement(std::move(aSource->mChunks[offset]));
    }

    aSource->mChunks.ClearAndRetainStorage();
    MOZ_ASSERT(aSource->mChunks.Capacity() >= DEFAULT_SEGMENT_CAPACITY,
               "Capacity must be retained after appending from aSource");
  }

  void AppendSliceInternal(const MediaSegmentBase<C, Chunk>& aSource,
                           TrackTime aStart, TrackTime aEnd) {
    MOZ_ASSERT(aStart <= aEnd, "Endpoints inverted");
    NS_ASSERTION(aStart >= 0 && aEnd <= aSource.mDuration,
                 "Slice out of range");
    mDuration += aEnd - aStart;
    TrackTime offset = 0;
    for (uint32_t i = 0; i < aSource.mChunks.Length() && offset < aEnd; ++i) {
      const Chunk& c = aSource.mChunks[i];
      TrackTime start = std::max(aStart, offset);
      TrackTime nextOffset = offset + c.GetDuration();
      TrackTime end = std::min(aEnd, nextOffset);
      if (start < end) {
        if (!mChunks.IsEmpty() &&
            mChunks[mChunks.Length() - 1].CanCombineWithFollowing(c)) {
          MOZ_ASSERT(start - offset >= 0 && end - offset <= aSource.mDuration,
                     "Slice out of bounds");
          mChunks[mChunks.Length() - 1].mDuration += end - start;
        } else {
          mChunks.AppendElement(c)->SliceTo(start - offset, end - offset);
        }
      }
      offset = nextOffset;
    }
  }

  Chunk* AppendChunk(TrackTime aDuration) {
    MOZ_ASSERT(aDuration >= 0);
    Chunk* c = mChunks.AppendElement();
    c->mDuration = aDuration;
    mDuration += aDuration;
    return c;
  }

  void RemoveLeading(TrackTime aDuration, uint32_t aStartIndex) {
    NS_ASSERTION(aDuration >= 0, "Can't remove negative duration");
    TrackTime t = aDuration;
    uint32_t chunksToRemove = 0;
    for (uint32_t i = aStartIndex; i < mChunks.Length() && t > 0; ++i) {
      Chunk* c = &mChunks[i];
      if (c->GetDuration() > t) {
        c->SliceTo(t, c->GetDuration());
        t = 0;
        break;
      }
      t -= c->GetDuration();
      chunksToRemove = i + 1 - aStartIndex;
    }
    if (aStartIndex == 0 && chunksToRemove == mChunks.Length()) {
      mChunks.ClearAndRetainStorage();
    } else {
      mChunks.RemoveElementsAt(aStartIndex, chunksToRemove);
    }
    mDuration -= aDuration - t;

    MOZ_ASSERT(mChunks.Capacity() >= DEFAULT_SEGMENT_CAPACITY,
               "Capacity must be retained after removing chunks");
  }

  void RemoveTrailing(TrackTime aKeep, uint32_t aStartIndex) {
    NS_ASSERTION(aKeep >= 0, "Can't keep negative duration");
    TrackTime t = aKeep;
    uint32_t i;
    for (i = aStartIndex; i < mChunks.Length() && t; ++i) {
      Chunk* c = &mChunks[i];
      if (c->GetDuration() > t) {
        c->SliceTo(0, t);
        break;
      }
      t -= c->GetDuration();
    }
    // At this point `i` is already advanced due to last check in the loop.
    if (i < mChunks.Length()) {
      mChunks.RemoveLastElements(mChunks.Length() - i);
    }
    MOZ_ASSERT(mChunks.Capacity() >= DEFAULT_SEGMENT_CAPACITY,
               "Capacity must be retained after removing chunks");
    // Caller must adjust mDuration
  }

  AutoTArray<Chunk, DEFAULT_SEGMENT_CAPACITY> mChunks;
};

}  // namespace mozilla

#endif /* MOZILLA_MEDIASEGMENT_H_ */
