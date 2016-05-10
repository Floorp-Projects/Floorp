/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIASTREAMTRACK_H_
#define MEDIASTREAMTRACK_H_

#include "mozilla/DOMEventTargetHelper.h"
#include "nsError.h"
#include "nsID.h"
#include "nsIPrincipal.h"
#include "StreamTracks.h"
#include "MediaTrackConstraints.h"
#include "mozilla/CORSMode.h"
#include "PrincipalChangeObserver.h"

namespace mozilla {

class DOMMediaStream;
class MediaEnginePhotoCallback;
class MediaInputPort;
class MediaStream;
class MediaStreamGraph;
class MediaStreamGraphImpl;
class MediaStreamTrackListener;
class MediaStreamTrackDirectListener;
class PeerConnectionImpl;
class PeerConnectionMedia;
class PeerIdentity;
class ProcessedMediaStream;
class RemoteSourceStreamInfo;
class SourceStreamInfo;

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

  MediaStreamTrackSource(nsIPrincipal* aPrincipal,
                         const bool aIsRemote,
                         const nsString& aLabel)
    : mPrincipal(aPrincipal),
      mIsRemote(aIsRemote),
      mLabel(aLabel),
      mStopped(false)
  {
    MOZ_COUNT_CTOR(MediaStreamTrackSource);
  }

  /**
   * Use to clean up any resources that have to be cleaned before the
   * destructor is called. It is often too late in the destructor because
   * of garbage collection having removed the members already.
   */
  virtual void Destroy() {}

  /**
   * Gets the source's MediaSourceEnum for usage by PeerConnections.
   */
  virtual MediaSourceEnum GetMediaSource() const = 0;

  /**
   * Get this TrackSource's principal.
   */
  nsIPrincipal* GetPrincipal() const { return mPrincipal; }

  /**
   * Get the source's current CORSMode. If not applicable CORS_NONE is returned.
   * The sink will be notified of changes to our CORSMode through
   * PrincipalChanged().
   */
  virtual CORSMode GetCORSMode() const { return CORS_NONE; }

  /**
   * This is used in WebRTC. A peerIdentity constrained MediaStreamTrack cannot
   * be sent across the network to anything other than a peer with the provided
   * identity. If this is set, then GetPrincipal() should return an instance of
   * nsNullPrincipal.
   *
   * A track's PeerIdentity is immutable and will not change during the track's
   * lifetime.
   */
  virtual const PeerIdentity* GetPeerIdentity() const { return nullptr; }

  /**
   * Indicates whether the track is remote or not per the MediaCapture and
   * Streams spec.
   */
  virtual bool IsRemote() const { return mIsRemote; }

  /**
   * MediaStreamTrack::GetLabel (see spec) calls through to here.
   */
  void GetLabel(nsAString& aLabel) { aLabel.Assign(mLabel); }

  /**
   * Forwards a photo request to backends that support it. Other backends return
   * NS_ERROR_NOT_IMPLEMENTED to indicate that a MediaStreamGraph-based fallback
   * should be used.
   */
  virtual nsresult TakePhoto(MediaEnginePhotoCallback*) const { return NS_ERROR_NOT_IMPLEMENTED; }

  /**
   * We provide a fallback solution to ApplyConstraints() here.
   * Sources that support ApplyConstraints() will have to override it.
   */
  virtual already_AddRefed<Promise>
  ApplyConstraints(nsPIDOMWindowInner* aWindow,
                   const dom::MediaTrackConstraints& aConstraints,
                   ErrorResult &aRv);

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

  // The label of the track we are the source of per the MediaStreamTrack spec.
  const nsString mLabel;

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
  explicit BasicUnstoppableTrackSource(nsIPrincipal* aPrincipal,
                                       const MediaSourceEnum aMediaSource =
                                         MediaSourceEnum::Other)
    : MediaStreamTrackSource(aPrincipal, true, nsString())
    , mMediaSource(aMediaSource)
  {}

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
  // DOMMediaStream owns MediaStreamTrack instances, and requires access to
  // some internal state, e.g., GetInputStream(), GetOwnedStream().
  friend class mozilla::DOMMediaStream;

  // PeerConnection and friends need to know our owning DOMStream and track id.
  friend class mozilla::PeerConnectionImpl;
  friend class mozilla::PeerConnectionMedia;
  friend class mozilla::SourceStreamInfo;
  friend class mozilla::RemoteSourceStreamInfo;

  class PrincipalHandleListener;

public:
  /**
   * aTrackID is the MediaStreamGraph track ID for the track in the
   * MediaStream owned by aStream.
   */
  MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID,
                   TrackID aInputTrackID,
                   MediaStreamTrackSource* aSource);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamTrack,
                                           DOMEventTargetHelper)

  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override = 0;

  virtual AudioStreamTrack* AsAudioStreamTrack() { return nullptr; }
  virtual VideoStreamTrack* AsVideoStreamTrack() { return nullptr; }

  virtual const AudioStreamTrack* AsAudioStreamTrack() const { return nullptr; }
  virtual const VideoStreamTrack* AsVideoStreamTrack() const { return nullptr; }

  // WebIDL
  virtual void GetKind(nsAString& aKind) = 0;
  void GetId(nsAString& aID) const;
  void GetLabel(nsAString& aLabel) { GetSource().GetLabel(aLabel); }
  bool Enabled() { return mEnabled; }
  void SetEnabled(bool aEnabled);
  void Stop();
  already_AddRefed<Promise>
  ApplyConstraints(const dom::MediaTrackConstraints& aConstraints, ErrorResult &aRv);
  already_AddRefed<MediaStreamTrack> Clone();

  IMPL_EVENT_HANDLER(ended)

  bool Ended() const { return mEnded; }

  /**
   * Notified by the MediaStreamGraph, through our owning MediaStream on the
   * main thread.
   *
   * Note that this sets the track to ended and raises the "ended" event
   * synchronously.
   */
  void NotifyEnded();

  /**
   * Get this track's principal.
   */
  nsIPrincipal* GetPrincipal() const { return mPrincipal; }

  /**
   * Called by the PrincipalHandleListener when this track's PrincipalHandle changes on
   * the MediaStreamGraph thread. When the PrincipalHandle matches the pending
   * principal we know that the principal change has propagated to consumers.
   */
  void NotifyPrincipalHandleChanged(const PrincipalHandle& aPrincipalHandle);

  /**
   * Get this track's CORS mode.
   */
  CORSMode GetCORSMode() const { return GetSource().GetCORSMode(); }

  /**
   * Get this track's PeerIdentity.
   */
  const PeerIdentity* GetPeerIdentity() const { return GetSource().GetPeerIdentity(); }

  MediaStreamGraph* Graph();
  MediaStreamGraphImpl* GraphImpl();

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

  /**
   * Adds a MediaStreamTrackListener to the MediaStreamGraph representation of
   * this track.
   */
  void AddListener(MediaStreamTrackListener* aListener);

  /**
   * Removes a MediaStreamTrackListener from the MediaStreamGraph representation
   * of this track.
   */
  void RemoveListener(MediaStreamTrackListener* aListener);

  /**
   * Attempts to add a direct track listener to this track.
   * Callers must listen to the NotifyInstalled event to know if installing
   * the listener succeeded (tracks originating from SourceMediaStreams) or
   * failed (e.g., WebAudio originated tracks).
   */
  void AddDirectListener(MediaStreamTrackDirectListener *aListener);
  void RemoveDirectListener(MediaStreamTrackDirectListener  *aListener);

  /**
   * Sets up a MediaInputPort from the underlying track that this
   * MediaStreamTrack represents, to aStream, and returns it.
   */
  already_AddRefed<MediaInputPort> ForwardTrackContentsTo(ProcessedMediaStream* aStream);

  /**
   * Returns true if this track is connected to aPort and forwarded to aPort's
   * output stream.
   */
  bool IsForwardedThrough(MediaInputPort* aPort);

protected:
  virtual ~MediaStreamTrack();

  void Destroy();

  // Returns the original DOMMediaStream's underlying input stream.
  MediaStream* GetInputStream();

  // Returns the owning DOMMediaStream's underlying owned stream.
  ProcessedMediaStream* GetOwnedStream();

  // Returns the original DOMMediaStream. If this track is a clone,
  // the original track's owning DOMMediaStream is returned.
  DOMMediaStream* GetInputDOMStream();

  /**
   * Sets the principal and notifies PrincipalChangeObservers if it changes.
   */
  void SetPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Creates a new MediaStreamTrack with the same type, input track ID and
   * source as this MediaStreamTrack.
   * aTrackID is the TrackID the new track will have in its owned stream.
   */
  virtual already_AddRefed<MediaStreamTrack> CloneInternal(DOMMediaStream* aOwningStream,
                                                           TrackID aTrackID) = 0;

  nsTArray<PrincipalChangeObserver<MediaStreamTrack>*> mPrincipalChangeObservers;

  RefPtr<DOMMediaStream> mOwningStream;
  TrackID mTrackID;
  TrackID mInputTrackID;
  RefPtr<MediaStreamTrackSource> mSource;
  RefPtr<MediaStreamTrack> mOriginalTrack;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPrincipal> mPendingPrincipal;
  RefPtr<PrincipalHandleListener> mPrincipalHandleListener;
  nsString mID;
  bool mEnded;
  bool mEnabled;
  const bool mRemote;
  bool mStopped;
};

} // namespace dom
} // namespace mozilla

#endif /* MEDIASTREAMTRACK_H_ */
