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
typedef PRInt64 MediaTime;
const PRInt64 MEDIA_TIME_FRAC_BITS = 20;
const PRInt64 MEDIA_TIME_MAX = PR_INT64_MAX;

inline MediaTime MillisecondsToMediaTime(PRInt32 aMS)
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
typedef PRInt64 TrackTicks;
const PRInt64 TRACK_TICKS_MAX = PR_INT64_MAX >> MEDIA_TIME_FRAC_BITS;

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
  TrackTicks GetDuration() { return mDuration; }
  Type GetType() { return mType; }

  /**
   * Create a MediaSegment of the same type.
   */
  virtual MediaSegment* CreateEmptyClone() = 0;
  /**
   * Moves contents of aSource to the end of this segment.
   */
  virtual void AppendFrom(MediaSegment* aSource) = 0;
  /**
   * Replace all contents up to aDuration with null data.
   */
  virtual void ForgetUpTo(TrackTicks aDuration) = 0;
  /**
   * Insert aDuration of null data at the start of the segment.
   */
  virtual void InsertNullDataAtStart(TrackTicks aDuration) = 0;

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
  virtual MediaSegment* CreateEmptyClone()
  {
    C* s = new C();
    s->InitFrom(*static_cast<C*>(this));
    return s;
  }

  /**
   * Appends the contents of aSource to this segment, clearing aSource.
   */
  virtual void AppendFrom(MediaSegmentBase<C, Chunk>* aSource)
  {
    mDuration += aSource->mDuration;
    aSource->mDuration = 0;
    if (!mChunks.IsEmpty() && !aSource->mChunks.IsEmpty() &&
        mChunks[mChunks.Length() - 1].CanCombineWithFollowing(aSource->mChunks[0])) {
      mChunks[mChunks.Length() - 1].mDuration += aSource->mChunks[0].mDuration;
      aSource->mChunks.RemoveElementAt(0);
    }
    mChunks.MoveElementsFrom(aSource->mChunks);
  }
  void RemoveLeading(TrackTicks aDuration)
  {
    RemoveLeadingInternal(aDuration, 0);
  }
  virtual void AppendFrom(MediaSegment* aSource)
  {
    NS_ASSERTION(aSource->GetType() == C::StaticType(), "Wrong type");
    AppendFrom(static_cast<C*>(aSource));
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
        RemoveLeadingInternal(extraToForget, 1);
        mChunks[0].mDuration += extraToForget;
        mDuration += extraToForget;
      }
      return;
    }
    RemoveLeading(aDuration);
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

protected:
  MediaSegmentBase(Type aType) : MediaSegment(aType) {}

  void BaseSliceFrom(const MediaSegmentBase<C, Chunk>& aOther,
                     TrackTicks aStart, TrackTicks aEnd)
  {
    NS_ASSERTION(aStart >= 0 && aEnd <= aOther.mDuration,
                 "Slice out of range");
    TrackTicks offset = 0;
    for (PRUint32 i = 0; i < aOther.mChunks.Length() && offset < aEnd; ++i) {
      const Chunk& c = aOther.mChunks[i];
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

  Chunk* FindChunkContaining(TrackTicks aOffset, TrackTicks* aStart = nsnull)
  {
    if (aOffset < 0) {
      return nsnull;
    }
    TrackTicks offset = 0;
    for (PRUint32 i = 0; i < mChunks.Length(); ++i) {
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
    return nsnull;
  }

  Chunk* GetLastChunk()
  {
    if (mChunks.IsEmpty()) {
      return nsnull;
    }
    return &mChunks[mChunks.Length() - 1];
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
    PRUint32 mIndex;
  };

protected:
  void RemoveLeadingInternal(TrackTicks aDuration, PRUint32 aStartIndex)
  {
    NS_ASSERTION(aDuration >= 0, "Can't remove negative duration");
    TrackTicks t = aDuration;
    PRUint32 chunksToRemove = 0;
    for (PRUint32 i = aStartIndex; i < mChunks.Length() && t > 0; ++i) {
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
