/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIASTREAMTRACK_H_
#define MEDIASTREAMTRACK_H_

#include "mozilla/DOMEventTargetHelper.h"
#include "nsError.h"
#include "nsID.h"
#include "StreamBuffer.h"
#include "MediaTrackConstraints.h"
#include "PrincipalChangeObserver.h"

namespace mozilla {

class DOMMediaStream;
class MediaEnginePhotoCallback;
class MediaStream;
class MediaStreamGraph;
class ProcessedMediaStream;

namespace dom {

class AudioStreamTrack;
class VideoStreamTrack;

/**
 * Common interface through which a MediaStreamTrack can communicate with its
 * producer on the main thread.
 *
 * Kept alive by a strong ref in all MediaStreamTracks (original and clones)
 * sharing this source.
 */
class MediaStreamTrackSource : public nsISupports
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(MediaStreamTrackSource)

public:
  class Sink
  {
  public:
    virtual void PrincipalChanged() = 0;
  };

  MediaStreamTrackSource(nsIPrincipal* aPrincipal, const bool aIsRemote)
    : mPrincipal(aPrincipal),
      mIsRemote(aIsRemote),
      mStopped(false)
  {
    MOZ_COUNT_CTOR(MediaStreamTrackSource);
  }

  /**
   * Gets the source's MediaSourceEnum for usage by PeerConnections.
   */
  virtual MediaSourceEnum GetMediaSource() const = 0;

  /**
   * Get this TrackSource's principal.
   */
  nsIPrincipal* GetPrincipal() const { return mPrincipal; }

  /**
   * Indicates whether the track is remote or not per the MediaCapture and
   * Streams spec.
   */
  virtual bool IsRemote() const { return mIsRemote; }

  /**
   * Forwards a photo request to backends that support it. Other backends return
   * NS_ERROR_NOT_IMPLEMENTED to indicate that a MediaStreamGraph-based fallback
   * should be used.
   */
  virtual nsresult TakePhoto(MediaEnginePhotoCallback*) const { return NS_ERROR_NOT_IMPLEMENTED; }

  /**
   * Called by the source interface when all registered sinks have unregistered.
   */
  virtual void Stop() = 0;

  /**
   * Called by each MediaStreamTrack clone on initialization.
   */
  void RegisterSink(Sink* aSink)
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mStopped) {
      return;
    }
    mSinks.AppendElement(aSink);
  }

  /**
   * Called by each MediaStreamTrack clone on Stop() if supported by the
   * source (us) or destruction.
   */
  void UnregisterSink(Sink* aSink)
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mSinks.RemoveElement(aSink) && mSinks.IsEmpty() && !IsRemote()) {
      Stop();
      mStopped = true;
    }
  }

protected:
  virtual ~MediaStreamTrackSource()
  {
    MOZ_COUNT_DTOR(MediaStreamTrackSource);
  }

  /**
   * Called by a sub class when the principal has changed.
   * Notifies all sinks.
   */
  void PrincipalChanged()
  {
    for (Sink* sink : mSinks) {
      sink->PrincipalChanged();
    }
  }

  // Principal identifying who may access the contents of this source.
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // Currently registered sinks.
  nsTArray<Sink*> mSinks;

  // True if this is a remote track source, i.e., a PeerConnection.
  const bool mIsRemote;

  // True if this source is not remote, all MediaStreamTrack users have
  // unregistered from this source and Stop() has been called.
  bool mStopped;
};

/**
 * Basic implementation of MediaStreamTrackSource that ignores Stop().
 */
class BasicUnstoppableTrackSource : public MediaStreamTrackSource
{
public:
  explicit BasicUnstoppableTrackSource(const MediaSourceEnum aMediaSource =
                                         MediaSourceEnum::Other)
    : MediaStreamTrackSource(nullptr, true), mMediaSource(aMediaSource) {}

  MediaSourceEnum GetMediaSource() const override { return mMediaSource; }

  void Stop() override {}

protected:
  ~BasicUnstoppableTrackSource() {}

  const MediaSourceEnum mMediaSource;
};

/**
 * Class representing a track in a DOMMediaStream.
 */
class MediaStreamTrack : public DOMEventTargetHelper,
                         public MediaStreamTrackSource::Sink
{
public:
  /**
   * aTrackID is the MediaStreamGraph track ID for the track in the
   * MediaStream owned by aStream.
   */
  MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID,
                   TrackID aInputTrackID, const nsString& aLabel,
                   MediaStreamTrackSource* aSource);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamTrack,
                                           DOMEventTargetHelper)

  DOMMediaStream* GetParentObject() const { return mOwningStream; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override = 0;

  /**
   * Returns the DOMMediaStream owning this track.
   */
  DOMMediaStream* GetStream() const { return mOwningStream; }

  /**
   * Returns the TrackID this stream has in its owning DOMMediaStream's Owned
   * stream.
   */
  TrackID GetTrackID() const { return mTrackID; }

  /**
   * Returns the TrackID this MediaStreamTrack has in its input MSG-MediaStream.
   */
  TrackID GetInputTrackID() const { return mInputTrackID; }

  virtual AudioStreamTrack* AsAudioStreamTrack() { return nullptr; }
  virtual VideoStreamTrack* AsVideoStreamTrack() { return nullptr; }

  // WebIDL
  virtual void GetKind(nsAString& aKind) = 0;
  void GetId(nsAString& aID) const;
  void GetLabel(nsAString& aLabel) { aLabel.Assign(mLabel); }
  bool Enabled() { return mEnabled; }
  void SetEnabled(bool aEnabled);
  void Stop();
  already_AddRefed<Promise>
  ApplyConstraints(const dom::MediaTrackConstraints& aConstraints, ErrorResult &aRv);

  bool Ended() const { return mEnded; }
  // Notifications from the MediaStreamGraph
  void NotifyEnded() { mEnded = true; }

  /**
   * Get this track's principal.
   */
  nsIPrincipal* GetPrincipal() const { return GetSource().GetPrincipal(); }

  MediaStreamGraph* Graph();

  MediaStreamTrackSource& GetSource() const
  {
    MOZ_RELEASE_ASSERT(mSource, "The track source is only removed on destruction");
    return *mSource;
  }

  // Webrtc allows the remote side to name tracks whatever it wants, and we
  // need to surface this to content.
  void AssignId(const nsAString& aID) { mID = aID; }

  // Implementation of MediaStreamTrackSource::Sink
  void PrincipalChanged() override;

  /**
   * Add a PrincipalChangeObserver to this track.
   *
   * Returns true if it was successfully added.
   *
   * Ownership of the PrincipalChangeObserver remains with the caller, and it's
   * the caller's responsibility to remove the observer before it dies.
   */
  bool AddPrincipalChangeObserver(PrincipalChangeObserver<MediaStreamTrack>* aObserver);

  /**
   * Remove an added PrincipalChangeObserver from this track.
   *
   * Returns true if it was successfully removed.
   */
  bool RemovePrincipalChangeObserver(PrincipalChangeObserver<MediaStreamTrack>* aObserver);

protected:
  virtual ~MediaStreamTrack();

  // Returns the original DOMMediaStream's underlying input stream.
  MediaStream* GetInputStream();

  // Returns the owning DOMMediaStream's underlying owned stream.
  ProcessedMediaStream* GetOwnedStream();

  // Returns the original DOMMediaStream. If this track is a clone,
  // the original track's owning DOMMediaStream is returned.
  DOMMediaStream* GetInputDOMStream();

  nsTArray<PrincipalChangeObserver<MediaStreamTrack>*> mPrincipalChangeObservers;

  RefPtr<DOMMediaStream> mOwningStream;
  TrackID mTrackID;
  TrackID mInputTrackID;
  RefPtr<MediaStreamTrackSource> mSource;
  RefPtr<MediaStreamTrack> mOriginalTrack;
  nsString mID;
  nsString mLabel;
  bool mEnded;
  bool mEnabled;
  const bool mRemote;
  bool mStopped;
};

} // namespace dom
} // namespace mozilla

#endif /* MEDIASTREAMTRACK_H_ */
