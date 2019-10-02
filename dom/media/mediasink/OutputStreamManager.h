/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OutputStreamManager_h
#define OutputStreamManager_h

#include "mozilla/RefPtr.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/WeakPtr.h"
#include "nsTArray.h"

namespace mozilla {

class DOMMediaStream;
class MediaInputPort;
class OutputStreamManager;
class ProcessedMediaTrack;
class SourceMediaTrack;

namespace dom {
class MediaStreamTrack;
}

class OutputStreamData {
 public:
  OutputStreamData(OutputStreamManager* aManager,
                   AbstractThread* aAbstractMainThread,
                   DOMMediaStream* aDOMStream);
  OutputStreamData(const OutputStreamData& aOther) = delete;
  OutputStreamData(OutputStreamData&& aOther) = delete;
  ~OutputStreamData();

  // Creates and adds a MediaStreamTrack to mDOMStream so that we can feed data
  // to it. For a true aAsyncAddTrack we will dispatch a task to add the
  // created track to mDOMStream, as is required by spec for the "addtrack"
  // event.
  void AddTrack(SourceMediaTrack* aTrack, MediaSegment::Type aType,
                nsIPrincipal* aPrincipal, bool aAsyncAddTrack);
  // Ends any MediaStreamTracks sourced from aTrack.
  void RemoveTrack(SourceMediaTrack* aTrack);

  void SetPrincipal(nsIPrincipal* aPrincipal);

  const RefPtr<OutputStreamManager> mManager;
  const RefPtr<AbstractThread> mAbstractMainThread;
  // The DOMMediaStream we add tracks to and represent.
  const WeakPtr<DOMMediaStream> mDOMStream;

 private:
  // Tracks that have been added and not yet removed.
  nsTArray<WeakPtr<dom::MediaStreamTrack>> mTracks;
};

class OutputStreamManager {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OutputStreamManager);

 public:
  OutputStreamManager(SharedDummyTrack* aDummyStream, nsIPrincipal* aPrincipal,
                      AbstractThread* aAbstractMainThread);
  // Add the output stream to the collection.
  void Add(DOMMediaStream* aDOMStream);
  // Remove the output stream from the collection.
  void Remove(DOMMediaStream* aDOMStream);
  // Returns true if there's a live track of the given type.
  bool HasTrackType(MediaSegment::Type aType);
  // Returns true if the given tracks are sourcing all currently live tracks.
  // Use nullptr to make it ignored for that type.
  bool HasTracks(SourceMediaTrack* aAudioStream,
                 SourceMediaTrack* aVideoStream);
  // Gets the underlying track for the given type if it has never been played,
  // or nullptr if there is none.
  SourceMediaTrack* GetPrecreatedTrackOfType(MediaSegment::Type aType) const;
  // Returns the number of live tracks.
  size_t NumberOfTracks();
  // Add a track sourced to all output tracks and return the MediaTrack that
  // sources it.
  already_AddRefed<SourceMediaTrack> AddTrack(MediaSegment::Type aType);
  // Remove all currently live tracks.
  void RemoveTracks();
  // Remove all currently live tracks and all output streams.
  void Disconnect();
  // The principal handle for the underlying decoder.
  AbstractCanonical<PrincipalHandle>* CanonicalPrincipalHandle();
  // Called when the underlying decoder's principal has changed.
  void SetPrincipal(nsIPrincipal* aPrincipal);
  // Called by DecodedStream when its playing state changes. While not playing
  // we suspend mSourceTrack.
  void SetPlaying(bool aPlaying);
  // Return true if the collection of output streams is empty.
  bool IsEmpty() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mStreams.IsEmpty();
  }

  const RefPtr<AbstractThread> mAbstractMainThread;

 private:
  ~OutputStreamManager();

  class LiveTrack {
   public:
    LiveTrack(SourceMediaTrack* aSourceTrack, MediaSegment::Type aType);
    ~LiveTrack();

    const RefPtr<SourceMediaTrack> mSourceTrack;
    const MediaSegment::Type mType;
    bool mEverPlayed = false;
  };

  struct StreamComparator {
    static bool Equals(const UniquePtr<OutputStreamData>& aData,
                       DOMMediaStream* aStream) {
      return aData->mDOMStream == aStream;
    }
  };
  struct TrackStreamComparator {
    static bool Equals(const UniquePtr<LiveTrack>& aLiveTrack,
                       SourceMediaTrack* aTrack) {
      return aLiveTrack->mSourceTrack == aTrack;
    }
  };
  struct TrackTypeComparator {
    static bool Equals(const UniquePtr<LiveTrack>& aLiveTrack,
                       MediaSegment::Type aType) {
      return aLiveTrack->mType == aType;
    }
  };
  struct PrecreatedTrackTypeComparator {
    static bool Equals(const UniquePtr<LiveTrack>& aLiveTrack,
                       MediaSegment::Type aType) {
      return !aLiveTrack->mEverPlayed && aLiveTrack->mType == aType;
    }
  };
  struct TrackComparator {
    static bool Equals(
        const UniquePtr<LiveTrack>& aLiveTrack,
        const Pair<SourceMediaTrack*, MediaSegment::Type>& aOther) {
      return aLiveTrack->mSourceTrack == aOther.first() &&
             aLiveTrack->mType == aOther.second();
    }
  };

  // Goes through mStreams and removes any entries that have been destroyed.
  void AutoRemoveDestroyedStreams();

  // Remove tracks sourced from aTrack from all output tracks.
  void RemoveTrack(SourceMediaTrack* aTrack);

  const RefPtr<SharedDummyTrack> mDummyStream;
  nsTArray<UniquePtr<OutputStreamData>> mStreams;
  nsTArray<UniquePtr<LiveTrack>> mLiveTracks;
  Canonical<PrincipalHandle> mPrincipalHandle;
  bool mPlaying = false;
};

}  // namespace mozilla

#endif  // OutputStreamManager_h
