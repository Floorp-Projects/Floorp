/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_STREAMBUFFER_H_
#define MOZILLA_STREAMBUFFER_H_

#include "MediaSegment.h"
#include "nsAutoPtr.h"

namespace mozilla {

/**
 * Media time relative to the start of a StreamBuffer.
 */
typedef MediaTime StreamTime;
const StreamTime STREAM_TIME_MAX = MEDIA_TIME_MAX;

/**
 * Unique ID for track within a StreamBuffer. Tracks from different
 * StreamBuffers may have the same ID; this matters when appending StreamBuffers,
 * since tracks with the same ID are matched. Only IDs greater than 0 are allowed.
 */
typedef int32_t TrackID;
const TrackID TRACK_NONE = 0;
const TrackID TRACK_INVALID = -1;

inline TrackTicks RateConvertTicksRoundDown(TrackRate aOutRate,
                                            TrackRate aInRate,
                                            TrackTicks aTicks)
{
  NS_ASSERTION(0 < aOutRate && aOutRate <= TRACK_RATE_MAX, "Bad out rate");
  NS_ASSERTION(0 < aInRate && aInRate <= TRACK_RATE_MAX, "Bad in rate");
  NS_WARN_IF_FALSE(0 <= aTicks && aTicks <= TRACK_TICKS_MAX, "Bad ticks"); // bug 957691
  return (aTicks * aOutRate) / aInRate;
}
inline TrackTicks RateConvertTicksRoundUp(TrackRate aOutRate,
                                          TrackRate aInRate, TrackTicks aTicks)
{
  NS_ASSERTION(0 < aOutRate && aOutRate <= TRACK_RATE_MAX, "Bad out rate");
  NS_ASSERTION(0 < aInRate && aInRate <= TRACK_RATE_MAX, "Bad in rate");
  NS_ASSERTION(0 <= aTicks && aTicks <= TRACK_TICKS_MAX, "Bad ticks");
  return (aTicks * aOutRate + aInRate - 1) / aInRate;
}

inline TrackTicks SecondsToTicksRoundDown(TrackRate aRate, double aSeconds)
{
  NS_ASSERTION(0 < aRate && aRate <= TRACK_RATE_MAX, "Bad rate");
  NS_ASSERTION(0 <= aSeconds && aSeconds <= TRACK_TICKS_MAX/TRACK_RATE_MAX,
               "Bad seconds");
  return aSeconds * aRate;
}
inline double TrackTicksToSeconds(TrackRate aRate, TrackTicks aTicks)
{
  NS_ASSERTION(0 < aRate && aRate <= TRACK_RATE_MAX, "Bad rate");
  NS_ASSERTION(0 <= aTicks && aTicks <= TRACK_TICKS_MAX, "Bad ticks");
  return static_cast<double>(aTicks)/aRate;
}

/**
 * This object contains the decoded data for a stream's tracks.
 * A StreamBuffer can be appended to. Logically a StreamBuffer only gets longer,
 * but we also have the ability to "forget" data before a certain time that
 * we know won't be used again. (We prune a whole number of seconds internally.)
 *
 * StreamBuffers should only be used from one thread at a time.
 *
 * A StreamBuffer has a set of tracks that can be of arbitrary types ---
 * the data for each track is a MediaSegment. The set of tracks can vary
 * over the timeline of the StreamBuffer.
 */
class StreamBuffer {
public:
  /**
   * Every track has a start time --- when it started in the StreamBuffer.
   * It has an end flag; when false, no end point is known; when true,
   * the track ends when the data we have for the track runs out.
   * Tracks have a unique ID assigned at creation. This allows us to identify
   * the same track across StreamBuffers. A StreamBuffer should never have
   * two tracks with the same ID (even if they don't overlap in time).
   * TODO Tracks can also be enabled and disabled over time.
   * TODO Add TimeVarying<TrackTicks,bool> mEnabled.
   * Takes ownership of aSegment.
   */
  class Track {
    Track(TrackID aID, TrackRate aRate, TrackTicks aStart, MediaSegment* aSegment, TrackRate aGraphRate)
      : mStart(aStart),
        mSegment(aSegment),
        mRate(aRate),
        mGraphRate(aGraphRate),
        mID(aID),
        mEnded(false)
    {
      MOZ_COUNT_CTOR(Track);

      NS_ASSERTION(aID > TRACK_NONE, "Bad track ID");
      NS_ASSERTION(0 < aRate && aRate <= TRACK_RATE_MAX, "Invalid rate");
      NS_ASSERTION(0 <= aStart && aStart <= aSegment->GetDuration(), "Bad start position");
    }
  public:
    ~Track()
    {
      MOZ_COUNT_DTOR(Track);
    }
    template <class T> T* Get() const
    {
      if (mSegment->GetType() == T::StaticType()) {
        return static_cast<T*>(mSegment.get());
      }
      return nullptr;
    }
    MediaSegment* GetSegment() const { return mSegment; }
    TrackRate GetRate() const { return mRate; }
    TrackID GetID() const { return mID; }
    bool IsEnded() const { return mEnded; }
    TrackTicks GetStart() const { return mStart; }
    TrackTicks GetEnd() const { return mSegment->GetDuration(); }
    StreamTime GetEndTimeRoundDown() const
    {
      return TicksToTimeRoundDown(mSegment->GetDuration());
    }
    StreamTime GetStartTimeRoundDown() const
    {
      return TicksToTimeRoundDown(mStart);
    }
    TrackTicks TimeToTicksRoundDown(StreamTime aTime) const
    {
      return RateConvertTicksRoundDown(mRate, mGraphRate, aTime);
    }
    StreamTime TicksToTimeRoundDown(TrackTicks aTicks) const
    {
      return RateConvertTicksRoundDown(mGraphRate, mRate, aTicks);
    }
    MediaSegment::Type GetType() const { return mSegment->GetType(); }

    void SetEnded() { mEnded = true; }
    void AppendFrom(Track* aTrack)
    {
      NS_ASSERTION(!mEnded, "Can't append to ended track");
      NS_ASSERTION(aTrack->mID == mID, "IDs must match");
      NS_ASSERTION(aTrack->mStart == 0, "Source track must start at zero");
      NS_ASSERTION(aTrack->mSegment->GetType() == GetType(), "Track types must match");
      NS_ASSERTION(aTrack->mRate == mRate, "Track rates must match");

      mSegment->AppendFrom(aTrack->mSegment);
      mEnded = aTrack->mEnded;
    }
    MediaSegment* RemoveSegment()
    {
      return mSegment.forget();
    }
    void ForgetUpTo(TrackTicks aTime)
    {
      mSegment->ForgetUpTo(aTime);
    }

    size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
    {
      size_t amount = aMallocSizeOf(this);
      if (mSegment) {
        amount += mSegment->SizeOfIncludingThis(aMallocSizeOf);
      }
      return amount;
    }

  protected:
    friend class StreamBuffer;

    // Start offset is in ticks at rate mRate
    TrackTicks mStart;
    // The segment data starts at the start of the owning StreamBuffer, i.e.,
    // there's mStart silence/no video at the beginning.
    nsAutoPtr<MediaSegment> mSegment;
    TrackRate mRate; // track rate in ticks per second
    TrackRate mGraphRate; // graph rate in StreamTime per second
    // Unique ID
    TrackID mID;
    // True when the track ends with the data in mSegment
    bool mEnded;
  };

  class CompareTracksByID {
  public:
    bool Equals(Track* aA, Track* aB) const {
      return aA->GetID() == aB->GetID();
    }
    bool LessThan(Track* aA, Track* aB) const {
      return aA->GetID() < aB->GetID();
    }
  };

  StreamBuffer()
    : mTracksKnownTime(0), mForgottenTime(0)
#ifdef DEBUG
    , mGraphRateIsSet(false)
#endif
  {
    MOZ_COUNT_CTOR(StreamBuffer);
  }
  ~StreamBuffer()
  {
    MOZ_COUNT_DTOR(StreamBuffer);
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    size_t amount = 0;
    amount += mTracks.SizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < mTracks.Length(); i++) {
      amount += mTracks[i]->SizeOfIncludingThis(aMallocSizeOf);
    }
    return amount;
  }

  /**
   * Initialize the graph rate for use in calculating StreamTimes from track
   * ticks.  Called when a MediaStream's graph pointer is initialized.
   */
  void InitGraphRate(TrackRate aGraphRate)
  {
    mGraphRate = aGraphRate;
#if DEBUG
    MOZ_ASSERT(!mGraphRateIsSet);
    mGraphRateIsSet = true;
#endif
  }

  TrackRate GraphRate() const
  {
    MOZ_ASSERT(mGraphRateIsSet);
    return mGraphRate;
  }

  /**
   * Takes ownership of aSegment. Don't do this while iterating, or while
   * holding a Track reference.
   * aSegment must have aStart worth of null data.
   */
  Track& AddTrack(TrackID aID, TrackRate aRate, TrackTicks aStart, MediaSegment* aSegment)
  {
    NS_ASSERTION(!FindTrack(aID), "Track with this ID already exists");

    Track* track = new Track(aID, aRate, aStart, aSegment, GraphRate());
    mTracks.InsertElementSorted(track, CompareTracksByID());

    if (mTracksKnownTime == STREAM_TIME_MAX) {
      // There exists code like
      // http://mxr.mozilla.org/mozilla-central/source/media/webrtc/signaling/src/mediapipeline/MediaPipeline.cpp?rev=96b197deb91e&mark=1292-1297#1292
      NS_WARNING("Adding track to StreamBuffer that should have no more tracks");
    } else {
      NS_ASSERTION(track->TimeToTicksRoundDown(mTracksKnownTime) <= aStart,
                   "Start time too early");
    }
    return *track;
  }
  void AdvanceKnownTracksTime(StreamTime aKnownTime)
  {
    NS_ASSERTION(aKnownTime >= mTracksKnownTime, "Can't move tracks-known time earlier");
    mTracksKnownTime = aKnownTime;
  }

  /**
   * The end time for the StreamBuffer is the latest time for which we have
   * data for all tracks that haven't ended by that time.
   */
  StreamTime GetEnd() const;

  /**
   * Returns the earliest time >= 0 at which all tracks have ended
   * and all their data has been played out and no new tracks can be added,
   * or STREAM_TIME_MAX if there is no such time.
   */
  StreamTime GetAllTracksEnd() const;

#ifdef DEBUG
  void DumpTrackInfo() const;
#endif

  Track* FindTrack(TrackID aID);

  class TrackIter {
  public:
    /**
     * Iterate through the tracks of aBuffer in order of ID.
     */
    explicit TrackIter(const StreamBuffer& aBuffer) :
      mBuffer(&aBuffer.mTracks), mIndex(0), mMatchType(false) {}
    /**
     * Iterate through the tracks of aBuffer with type aType, in order of ID.
     */
    TrackIter(const StreamBuffer& aBuffer, MediaSegment::Type aType) :
      mBuffer(&aBuffer.mTracks), mIndex(0), mType(aType), mMatchType(true) { FindMatch(); }
    bool IsEnded() { return mIndex >= mBuffer->Length(); }
    void Next()
    {
      ++mIndex;
      FindMatch();
    }
    Track* get() { return mBuffer->ElementAt(mIndex); }
    Track& operator*() { return *mBuffer->ElementAt(mIndex); }
    Track* operator->() { return mBuffer->ElementAt(mIndex); }
  private:
    void FindMatch()
    {
      if (!mMatchType)
        return;
      while (mIndex < mBuffer->Length() &&
             mBuffer->ElementAt(mIndex)->GetType() != mType) {
        ++mIndex;
      }
    }

    const nsTArray<nsAutoPtr<Track> >* mBuffer;
    uint32_t mIndex;
    MediaSegment::Type mType;
    bool mMatchType;
  };
  friend class TrackIter;

  /**
   * Forget stream data before aTime; they will no longer be needed.
   * Also can forget entire tracks that have ended at or before aTime.
   * Can't be used to forget beyond GetEnd().
   */
  void ForgetUpTo(StreamTime aTime);
  /**
   * Returns the latest time passed to ForgetUpTo.
   */
  StreamTime GetForgottenDuration()
  {
    return mForgottenTime;
  }

protected:
  TrackRate mGraphRate; // StreamTime per second
  // Any new tracks added will start at or after this time. In other words, the track
  // list is complete and correct for all times less than this time.
  StreamTime mTracksKnownTime;
  StreamTime mForgottenTime;
  // All known tracks for this StreamBuffer
  nsTArray<nsAutoPtr<Track> > mTracks;
#ifdef DEBUG
  bool mGraphRateIsSet;
#endif
};

}

#endif /* MOZILLA_STREAMBUFFER_H_ */

