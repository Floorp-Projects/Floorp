/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASEGMENT_H_
#define MOZILLA_MEDIASEGMENT_H_

#include "nsTArray.h"
#include "nsIPrincipal.h"
#include "nsProxyRelease.h"
#ifdef MOZILLA_INTERNAL_API
#include "mozilla/TimeStamp.h"
#endif
#include <algorithm>
#include "Latency.h"

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
 * A number of ticks at a rate determined by some underlying track (e.g.
 * audio sample rate). We want to make sure that multiplying TrackTicks by
 * a TrackRate doesn't overflow, so we set its max accordingly.
 * StreamTime should be used instead when we're working with MediaStreamGraph's
 * rate, but TrackTicks can be used outside MediaStreams when we have data
 * at a different rate.
 */
typedef int64_t TrackTicks;
const int64_t TRACK_TICKS_MAX = INT64_MAX >> TRACK_RATE_MAX_BITS;

/**
 * We represent media times in 64-bit audio frame counts or ticks.
 * All tracks in a MediaStreamGraph have the same rate.
 */
typedef int64_t MediaTime;
const int64_t MEDIA_TIME_MAX = TRACK_TICKS_MAX;

/**
 * Media time relative to the start of a StreamTracks.
 */
typedef MediaTime StreamTime;
const StreamTime STREAM_TIME_MAX = MEDIA_TIME_MAX;

/**
 * Media time relative to the start of the graph timeline.
 */
typedef MediaTime GraphTime;
const GraphTime GRAPH_TIME_MAX = MEDIA_TIME_MAX;

/**
 * We pass the principal through the MediaStreamGraph by wrapping it in a thread
 * safe nsMainThreadPtrHandle, since it cannot be used directly off the main
 * thread. We can compare two PrincipalHandles to each other on any thread, but
 * they can only be created and converted back to nsIPrincipal* on main thread.
 */
typedef nsMainThreadPtrHandle<nsIPrincipal> PrincipalHandle;

inline PrincipalHandle MakePrincipalHandle(nsIPrincipal* aPrincipal)
{
  RefPtr<nsMainThreadPtrHolder<nsIPrincipal>> holder =
    new nsMainThreadPtrHolder<nsIPrincipal>(aPrincipal);
  return PrincipalHandle(holder);
}

#define PRINCIPAL_HANDLE_NONE nullptr

inline nsIPrincipal* GetPrincipalFromHandle(PrincipalHandle& aPrincipalHandle)
{
  MOZ_ASSERT(NS_IsMainThread());
  return aPrincipalHandle.get();
}

inline bool PrincipalHandleMatches(PrincipalHandle& aPrincipalHandle,
                                   nsIPrincipal* aOther)
{
  if (!aOther) {
    return false;
  }

  nsIPrincipal* principal = GetPrincipalFromHandle(aPrincipalHandle);
  if (!principal) {
    return false;
  }

  bool result;
  if (NS_FAILED(principal->Equals(aOther, &result))) {
    NS_ERROR("Principal check failed");
    return false;
  }

  return result;
}

/**
 * A MediaSegment is a chunk of media data sequential in time. Different
 * types of data have different subclasses of MediaSegment, all inheriting
 * from MediaSegmentBase.
 * All MediaSegment data is timed using StreamTime. The actual tick rate
 * is defined on a per-track basis. For some track types, this can be
 * a fixed constant for all tracks of that type (e.g. 1MHz for video).
 *
 * Each media segment defines a concept of "null media data" (e.g. silence
 * for audio or "no video frame" for video), which can be efficiently
 * represented. This is used for padding.
 */
class MediaSegment {
public:
  virtual ~MediaSegment()
  {
    MOZ_COUNT_DTOR(MediaSegment);
  }

  enum Type {
    AUDIO,
    VIDEO,
    TYPE_COUNT
  };

  /**
   * Gets the total duration of the segment.
   */
  StreamTime GetDuration() const { return mDuration; }
  Type GetType() const { return mType; }

  /**
   * Gets the last principal id that was appended to this segment.
   */
  PrincipalHandle GetLastPrincipalHandle() const { return mLastPrincipalHandle; }
  /**
   * Called by the MediaStreamGraph as it appends a chunk with a different
   * principal id than the current one.
   */
  void SetLastPrincipalHandle(PrincipalHandle aLastPrincipalHandle)
  {
    mLastPrincipalHandle = aLastPrincipalHandle;
  }

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
  virtual void AppendSlice(const MediaSegment& aSource,
                           StreamTime aStart, StreamTime aEnd) = 0;
  /**
   * Replace all contents up to aDuration with null data.
   */
  virtual void ForgetUpTo(StreamTime aDuration) = 0;
  /**
   * Forget all data buffered after a given point
   */
  virtual void FlushAfter(StreamTime aNewEnd) = 0;
  /**
   * Insert aDuration of null data at the start of the segment.
   */
  virtual void InsertNullDataAtStart(StreamTime aDuration) = 0;
  /**
   * Insert aDuration of null data at the end of the segment.
   */
  virtual void AppendNullData(StreamTime aDuration) = 0;
  /**
   * Replace contents with disabled data of the same duration
   */
  virtual void ReplaceWithDisabled() = 0;
  /**
   * Remove all contents, setting duration to 0.
   */
  virtual void Clear() = 0;

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return 0;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

protected:
  explicit MediaSegment(Type aType)
    : mDuration(0), mType(aType), mLastPrincipalHandle(PRINCIPAL_HANDLE_NONE)
  {
    MOZ_COUNT_CTOR(MediaSegment);
  }

  StreamTime mDuration; // total of mDurations of all chunks
  Type mType;

  // The latest principal handle that the MediaStreamGraph has processed for
  // this segment.
  PrincipalHandle mLastPrincipalHandle;
};

/**
 * C is the implementation class subclassed from MediaSegmentBase.
 * C must contain a Chunk class.
 */
template <class C, class Chunk> class MediaSegmentBase : public MediaSegment {
public:
  MediaSegment* CreateEmptyClone() const override
  {
    return new C();
  }
  void AppendFrom(MediaSegment* aSource) override
  {
    NS_ASSERTION(aSource->GetType() == C::StaticType(), "Wrong type");
    AppendFromInternal(static_cast<C*>(aSource));
  }
  void AppendFrom(C* aSource)
  {
    AppendFromInternal(aSource);
  }
  void AppendSlice(const MediaSegment& aSource,
                   StreamTime aStart, StreamTime aEnd) override
  {
    NS_ASSERTION(aSource.GetType() == C::StaticType(), "Wrong type");
    AppendSliceInternal(static_cast<const C&>(aSource), aStart, aEnd);
  }
  void AppendSlice(const C& aOther, StreamTime aStart, StreamTime aEnd)
  {
    AppendSliceInternal(aOther, aStart, aEnd);
  }
  /**
   * Replace the first aDuration ticks with null media data, because the data
   * will not be required again.
   */
  void ForgetUpTo(StreamTime aDuration) override
  {
    if (mChunks.IsEmpty() || aDuration <= 0) {
      return;
    }
    if (mChunks[0].IsNull()) {
      StreamTime extraToForget = std::min(aDuration, mDuration) - mChunks[0].GetDuration();
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
  void FlushAfter(StreamTime aNewEnd) override
  {
    if (mChunks.IsEmpty()) {
      return;
    }

    if (mChunks[0].IsNull()) {
      StreamTime extraToKeep = aNewEnd - mChunks[0].GetDuration();
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
  void InsertNullDataAtStart(StreamTime aDuration) override
  {
    if (aDuration <= 0) {
      return;
    }
    if (!mChunks.IsEmpty() && mChunks[0].IsNull()) {
      mChunks[0].mDuration += aDuration;
    } else {
      mChunks.InsertElementAt(0)->SetNull(aDuration);
    }
#ifdef MOZILLA_INTERNAL_API
    mChunks[0].mTimeStamp = mozilla::TimeStamp::Now();
#endif
    mDuration += aDuration;
  }
  void AppendNullData(StreamTime aDuration) override
  {
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
  void ReplaceWithDisabled() override
  {
    if (GetType() != AUDIO) {
      MOZ_CRASH("Disabling unknown segment type");
    }
    StreamTime duration = GetDuration();
    Clear();
    AppendNullData(duration);
  }
  void Clear() override
  {
    mDuration = 0;
    mChunks.Clear();
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

  Chunk* FindChunkContaining(StreamTime aOffset, StreamTime* aStart = nullptr)
  {
    if (aOffset < 0) {
      return nullptr;
    }
    StreamTime offset = 0;
    for (uint32_t i = 0; i < mChunks.Length(); ++i) {
      Chunk& c = mChunks[i];
      StreamTime nextOffset = offset + c.GetDuration();
      if (aOffset < nextOffset) {
        if (aStart) {
          *aStart = offset;
        }
        return &c;
      }
      offset = nextOffset;
    }
    return nullptr;
  }

  void RemoveLeading(StreamTime aDuration)
  {
    RemoveLeading(aDuration, 0);
  }

#ifdef MOZILLA_INTERNAL_API
  void GetStartTime(TimeStamp &aTime) {
    aTime = mChunks[0].mTimeStamp;
  }
#endif

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = mChunks.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < mChunks.Length(); i++) {
      amount += mChunks[i].SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

protected:
  explicit MediaSegmentBase(Type aType) : MediaSegment(aType) {}

  /**
   * Appends the contents of aSource to this segment, clearing aSource.
   */
  void AppendFromInternal(MediaSegmentBase<C, Chunk>* aSource)
  {
    MOZ_ASSERT(aSource->mDuration >= 0);
    mDuration += aSource->mDuration;
    aSource->mDuration = 0;
    if (!mChunks.IsEmpty() && !aSource->mChunks.IsEmpty() &&
        mChunks[mChunks.Length() - 1].CanCombineWithFollowing(aSource->mChunks[0])) {
      mChunks[mChunks.Length() - 1].mDuration += aSource->mChunks[0].mDuration;
      aSource->mChunks.RemoveElementAt(0);
    }
    mChunks.AppendElements(Move(aSource->mChunks));
  }

  void AppendSliceInternal(const MediaSegmentBase<C, Chunk>& aSource,
                           StreamTime aStart, StreamTime aEnd)
  {
    MOZ_ASSERT(aStart <= aEnd, "Endpoints inverted");
    NS_ASSERTION(aStart >= 0 && aEnd <= aSource.mDuration, "Slice out of range");
    mDuration += aEnd - aStart;
    StreamTime offset = 0;
    for (uint32_t i = 0; i < aSource.mChunks.Length() && offset < aEnd; ++i) {
      const Chunk& c = aSource.mChunks[i];
      StreamTime start = std::max(aStart, offset);
      StreamTime nextOffset = offset + c.GetDuration();
      StreamTime end = std::min(aEnd, nextOffset);
      if (start < end) {
        mChunks.AppendElement(c)->SliceTo(start - offset, end - offset);
      }
      offset = nextOffset;
    }
  }

  Chunk* AppendChunk(StreamTime aDuration)
  {
    MOZ_ASSERT(aDuration >= 0);
    Chunk* c = mChunks.AppendElement();
    c->mDuration = aDuration;
    mDuration += aDuration;
    return c;
  }

  Chunk* GetLastChunk()
  {
    if (mChunks.IsEmpty()) {
      return nullptr;
    }
    return &mChunks[mChunks.Length() - 1];
  }

  void RemoveLeading(StreamTime aDuration, uint32_t aStartIndex)
  {
    NS_ASSERTION(aDuration >= 0, "Can't remove negative duration");
    StreamTime t = aDuration;
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
    mChunks.RemoveElementsAt(aStartIndex, chunksToRemove);
    mDuration -= aDuration - t;
  }

  void RemoveTrailing(StreamTime aKeep, uint32_t aStartIndex)
  {
    NS_ASSERTION(aKeep >= 0, "Can't keep negative duration");
    StreamTime t = aKeep;
    uint32_t i;
    for (i = aStartIndex; i < mChunks.Length(); ++i) {
      Chunk* c = &mChunks[i];
      if (c->GetDuration() > t) {
        c->SliceTo(0, t);
        break;
      }
      t -= c->GetDuration();
      if (t == 0) {
        break;
      }
    }
    if (i+1 < mChunks.Length()) {
      mChunks.RemoveElementsAt(i+1, mChunks.Length() - (i+1));
    }
    // Caller must adjust mDuration
  }

  nsTArray<Chunk> mChunks;
#ifdef MOZILLA_INTERNAL_API
  mozilla::TimeStamp mTimeStamp;
#endif
};

} // namespace mozilla

#endif /* MOZILLA_MEDIASEGMENT_H_ */
