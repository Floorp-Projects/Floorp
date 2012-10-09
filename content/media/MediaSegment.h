/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASEGMENT_H_
#define MOZILLA_MEDIASEGMENT_H_

#include "nsTArray.h"

namespace mozilla {

/**
 * We represent media times in 64-bit fixed point. So 1 MediaTime is
 * 1/(2^MEDIA_TIME_FRAC_BITS) seconds.
 */
typedef int64_t MediaTime;
const int64_t MEDIA_TIME_FRAC_BITS = 20;
const int64_t MEDIA_TIME_MAX = INT64_MAX;

inline MediaTime MillisecondsToMediaTime(int32_t aMS)
{
  return (MediaTime(aMS) << MEDIA_TIME_FRAC_BITS)/1000;
}

inline MediaTime SecondsToMediaTime(double aS)
{
  NS_ASSERTION(aS <= (MEDIA_TIME_MAX >> MEDIA_TIME_FRAC_BITS),
               "Out of range");
  return MediaTime(aS * (1 << MEDIA_TIME_FRAC_BITS));
}

inline double MediaTimeToSeconds(MediaTime aTime)
{
  return aTime*(1.0/(1 << MEDIA_TIME_FRAC_BITS));
}

/**
 * A number of ticks at a rate determined by some underlying track (e.g.
 * audio sample rate). We want to make sure that multiplying TrackTicks by
 * 2^MEDIA_TIME_FRAC_BITS doesn't overflow, so we set its max accordingly.
 */
typedef int64_t TrackTicks;
const int64_t TRACK_TICKS_MAX = INT64_MAX >> MEDIA_TIME_FRAC_BITS;

/**
 * A MediaSegment is a chunk of media data sequential in time. Different
 * types of data have different subclasses of MediaSegment, all inheriting
 * from MediaSegmentBase.
 * All MediaSegment data is timed using TrackTicks. The actual tick rate
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
  TrackTicks GetDuration() const { return mDuration; }
  Type GetType() const { return mType; }

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
                           TrackTicks aStart, TrackTicks aEnd) = 0;
  /**
   * Replace all contents up to aDuration with null data.
   */
  virtual void ForgetUpTo(TrackTicks aDuration) = 0;
  /**
   * Insert aDuration of null data at the start of the segment.
   */
  virtual void InsertNullDataAtStart(TrackTicks aDuration) = 0;
  /**
   * Insert aDuration of null data at the end of the segment.
   */
  virtual void AppendNullData(TrackTicks aDuration) = 0;

protected:
  MediaSegment(Type aType) : mDuration(0), mType(aType)
  {
    MOZ_COUNT_CTOR(MediaSegment);
  }

  TrackTicks mDuration; // total of mDurations of all chunks
  Type mType;
};

/**
 * C is the implementation class subclassed from MediaSegmentBase.
 * C must contain a Chunk class.
 */
template <class C, class Chunk> class MediaSegmentBase : public MediaSegment {
public:
  virtual MediaSegment* CreateEmptyClone() const
  {
    C* s = new C();
    s->InitFrom(*static_cast<const C*>(this));
    return s;
  }
  virtual void AppendFrom(MediaSegment* aSource)
  {
    NS_ASSERTION(aSource->GetType() == C::StaticType(), "Wrong type");
    AppendFromInternal(static_cast<C*>(aSource));
  }
  void AppendFrom(C* aSource)
  {
    AppendFromInternal(aSource);
  }
  virtual void AppendSlice(const MediaSegment& aSource,
                           TrackTicks aStart, TrackTicks aEnd)
  {
    NS_ASSERTION(aSource.GetType() == C::StaticType(), "Wrong type");
    AppendSliceInternal(static_cast<const C&>(aSource), aStart, aEnd);
  }
  void AppendSlice(const C& aOther, TrackTicks aStart, TrackTicks aEnd)
  {
    AppendSliceInternal(aOther, aStart, aEnd);
  }
  void InitToSlice(const C& aOther, TrackTicks aStart, TrackTicks aEnd)
  {
    static_cast<C*>(this)->InitFrom(aOther);
    AppendSliceInternal(aOther, aStart, aEnd);
  }
  /**
   * Replace the first aDuration ticks with null media data, because the data
   * will not be required again.
   */
  virtual void ForgetUpTo(TrackTicks aDuration)
  {
    if (mChunks.IsEmpty() || aDuration <= 0) {
      return;
    }
    if (mChunks[0].IsNull()) {
      TrackTicks extraToForget = NS_MIN(aDuration, mDuration) - mChunks[0].GetDuration();
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
  virtual void InsertNullDataAtStart(TrackTicks aDuration)
  {
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
  virtual void AppendNullData(TrackTicks aDuration)
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

  class ChunkIterator {
  public:
    ChunkIterator(MediaSegmentBase<C, Chunk>& aSegment)
      : mSegment(aSegment), mIndex(0) {}
    bool IsEnded() { return mIndex >= mSegment.mChunks.Length(); }
    void Next() { ++mIndex; }
    Chunk& operator*() { return mSegment.mChunks[mIndex]; }
    Chunk* operator->() { return &mSegment.mChunks[mIndex]; }
  private:
    MediaSegmentBase<C, Chunk>& mSegment;
    uint32_t mIndex;
  };

protected:
  MediaSegmentBase(Type aType) : MediaSegment(aType) {}

  /**
   * Appends the contents of aSource to this segment, clearing aSource.
   */
  void AppendFromInternal(MediaSegmentBase<C, Chunk>* aSource)
  {
    static_cast<C*>(this)->CheckCompatible(*static_cast<C*>(aSource));
    mDuration += aSource->mDuration;
    aSource->mDuration = 0;
    if (!mChunks.IsEmpty() && !aSource->mChunks.IsEmpty() &&
        mChunks[mChunks.Length() - 1].CanCombineWithFollowing(aSource->mChunks[0])) {
      mChunks[mChunks.Length() - 1].mDuration += aSource->mChunks[0].mDuration;
      aSource->mChunks.RemoveElementAt(0);
    }
    mChunks.MoveElementsFrom(aSource->mChunks);
  }

  void AppendSliceInternal(const MediaSegmentBase<C, Chunk>& aSource,
                           TrackTicks aStart, TrackTicks aEnd)
  {
    static_cast<C*>(this)->CheckCompatible(static_cast<const C&>(aSource));
    NS_ASSERTION(aStart <= aEnd, "Endpoints inverted");
    NS_ASSERTION(aStart >= 0 && aEnd <= aSource.mDuration,
                 "Slice out of range");
    mDuration += aEnd - aStart;
    TrackTicks offset = 0;
    for (uint32_t i = 0; i < aSource.mChunks.Length() && offset < aEnd; ++i) {
      const Chunk& c = aSource.mChunks[i];
      TrackTicks start = NS_MAX(aStart, offset);
      TrackTicks nextOffset = offset + c.GetDuration();
      TrackTicks end = NS_MIN(aEnd, nextOffset);
      if (start < end) {
        mChunks.AppendElement(c)->SliceTo(start - offset, end - offset);
      }
      offset = nextOffset;
    }
  }

  Chunk* AppendChunk(TrackTicks aDuration)
  {
    Chunk* c = mChunks.AppendElement();
    c->mDuration = aDuration;
    mDuration += aDuration;
    return c;
  }

  Chunk* FindChunkContaining(TrackTicks aOffset, TrackTicks* aStart = nullptr)
  {
    if (aOffset < 0) {
      return nullptr;
    }
    TrackTicks offset = 0;
    for (uint32_t i = 0; i < mChunks.Length(); ++i) {
      Chunk& c = mChunks[i];
      TrackTicks nextOffset = offset + c.GetDuration();
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

  Chunk* GetLastChunk()
  {
    if (mChunks.IsEmpty()) {
      return nullptr;
    }
    return &mChunks[mChunks.Length() - 1];
  }

  void RemoveLeading(TrackTicks aDuration, uint32_t aStartIndex)
  {
    NS_ASSERTION(aDuration >= 0, "Can't remove negative duration");
    TrackTicks t = aDuration;
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

  nsTArray<Chunk> mChunks;
};

}

#endif /* MOZILLA_MEDIASEGMENT_H_ */
