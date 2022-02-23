/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIASTREAMTRACK_H_
#define MEDIASTREAMTRACK_H_

#include "MediaTrackConstraints.h"
#include "PrincipalChangeObserver.h"
#include "PrincipalHandle.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaTrackSettingsBinding.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/WeakPtr.h"
#include "nsError.h"
#include "nsID.h"
#include "nsIPrincipal.h"

namespace mozilla {

class DOMMediaStream;
class MediaEnginePhotoCallback;
class MediaInputPort;
class MediaTrack;
class MediaTrackGraph;
class MediaTrackGraphImpl;
class MediaTrackListener;
class DirectMediaTrackListener;
class PeerConnectionImpl;
class PeerConnectionMedia;
class PeerIdentity;
class ProcessedMediaTrack;
class RemoteSourceStreamInfo;
class SourceStreamInfo;
class MediaMgrError;

namespace dom {

class AudioStreamTrack;
class VideoStreamTrack;
enum class CallerType : uint32_t;

/**
 * Common interface through which a MediaStreamTrack can communicate with its
 * producer on the main thread.
 *
 * Kept alive by a strong ref in all MediaStreamTracks (original and clones)
 * sharing this source.
 */
class MediaStreamTrackSource : public nsISupports {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(MediaStreamTrackSource)

 public:
  class Sink : public SupportsWeakPtr {
   public:
    /**
     * Must be constant throughout the Sink's lifetime.
     *
     * Return true to keep the MediaStreamTrackSource where this sink is
     * registered alive.
     * Return false to allow the source to stop.
     *
     * Typically MediaStreamTrack::Sink returns true and other Sinks
     * (like HTMLMediaElement::StreamCaptureTrackSource) return false.
     */
    virtual bool KeepsSourceAlive() const = 0;

    /**
     * Return true to ensure that the MediaStreamTrackSource where this Sink is
     * registered is kept turned on and active.
     * Return false to allow the source to pause, and any underlying devices to
     * temporarily stop.
     *
     * When the underlying enabled state of the sink changes,
     * call MediaStreamTrackSource::SinkEnabledStateChanged().
     *
     * Typically MediaStreamTrack returns the track's enabled state and other
     * Sinks (like HTMLMediaElement::StreamCaptureTrackSource) return false so
     * control over device state remains with tracks and their enabled state.
     */
    virtual bool Enabled() const = 0;

    /**
     * Called when the principal of the MediaStreamTrackSource where this sink
     * is registered has changed.
     */
    virtual void PrincipalChanged() = 0;

    /**
     * Called when the muted state of the MediaStreamTrackSource where this sink
     * is registered has changed.
     */
    virtual void MutedChanged(bool aNewState) = 0;

    /**
     * Called when the MediaStreamTrackSource where this sink is registered has
     * stopped producing data for good, i.e., it has ended.
     */
    virtual void OverrideEnded() = 0;

   protected:
    virtual ~Sink() = default;
  };

  MediaStreamTrackSource(nsIPrincipal* aPrincipal, const nsString& aLabel)
      : mPrincipal(aPrincipal), mLabel(aLabel), mStopped(false) {}

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
   * This is used in WebRTC. A peerIdentity constrained MediaStreamTrack cannot
   * be sent across the network to anything other than a peer with the provided
   * identity. If this is set, then GetPrincipal() should return an instance of
   * NullPrincipal.
   *
   * A track's PeerIdentity is immutable and will not change during the track's
   * lifetime.
   */
  virtual const PeerIdentity* GetPeerIdentity() const { return nullptr; }

  /**
   * MediaStreamTrack::GetLabel (see spec) calls through to here.
   */
  void GetLabel(nsAString& aLabel) { aLabel.Assign(mLabel); }

  /**
   * Whether this TrackSource provides video frames with an alpha channel. Only
   * applies to video sources. Used by HTMLVideoElement.
   */
  virtual bool HasAlpha() const { return false; }

  /**
   * Forwards a photo request to backends that support it. Other backends return
   * NS_ERROR_NOT_IMPLEMENTED to indicate that a MediaTrackGraph-based fallback
   * should be used.
   */
  virtual nsresult TakePhoto(MediaEnginePhotoCallback*) const {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  typedef MozPromise<bool /* aIgnored */, RefPtr<MediaMgrError>, true>
      ApplyConstraintsPromise;

  /**
   * We provide a fallback solution to ApplyConstraints() here.
   * Sources that support ApplyConstraints() will have to override it.
   */
  virtual RefPtr<ApplyConstraintsPromise> ApplyConstraints(
      const dom::MediaTrackConstraints& aConstraints, CallerType aCallerType);

  /**
   * Same for GetSettings (no-op).
   */
  virtual void GetSettings(dom::MediaTrackSettings& aResult){};

  /**
   * Called by the source interface when all registered sinks with
   * KeepsSourceAlive() == true have unregistered.
   */
  virtual void Stop() = 0;

  /**
   * Called by the source interface when all registered sinks with
   * KeepsSourceAlive() == true become disabled.
   */
  virtual void Disable() = 0;

  /**
   * Called by the source interface when at least one registered sink with
   * KeepsSourceAlive() == true become enabled.
   */
  virtual void Enable() = 0;

  /**
   * Called when a Sink's Enabled() state changed. Will iterate through all
   * sinks and notify the source of the aggregated enabled state.
   *
   * Note that a Sink with KeepsSourceAlive() == false counts as disabled.
   */
  void SinkEnabledStateChanged() {
    if (IsEnabled()) {
      Enable();
    } else {
      Disable();
    }
  }

  /**
   * Called by each MediaStreamTrack clone on initialization.
   */
  void RegisterSink(Sink* aSink) {
    MOZ_ASSERT(NS_IsMainThread());
    if (mStopped) {
      return;
    }
    mSinks.AppendElement(aSink);
    while (mSinks.RemoveElement(nullptr)) {
      MOZ_ASSERT_UNREACHABLE("Sink was not explicitly removed");
    }
  }

  /**
   * Called by each MediaStreamTrack clone on Stop() if supported by the
   * source (us) or destruction.
   */
  void UnregisterSink(Sink* aSink) {
    MOZ_ASSERT(NS_IsMainThread());
    while (mSinks.RemoveElement(nullptr)) {
      MOZ_ASSERT_UNREACHABLE("Sink was not explicitly removed");
    }
    if (mSinks.RemoveElement(aSink) && !IsActive()) {
      MOZ_ASSERT(!aSink->KeepsSourceAlive() || !mStopped,
                 "When the last sink keeping the source alive is removed, "
                 "we should still be live");
      Stop();
      mStopped = true;
    }
    if (!mStopped) {
      SinkEnabledStateChanged();
    }
  }

 protected:
  virtual ~MediaStreamTrackSource() = default;

  bool IsActive() {
    for (const WeakPtr<Sink>& sink : mSinks) {
      if (sink && sink->KeepsSourceAlive()) {
        return true;
      }
    }
    return false;
  }

  bool IsEnabled() {
    for (const WeakPtr<Sink>& sink : mSinks) {
      if (sink && sink->KeepsSourceAlive() && sink->Enabled()) {
        return true;
      }
    }
    return false;
  }

  /**
   * Called by a sub class when the principal has changed.
   * Notifies all sinks.
   */
  void PrincipalChanged() {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& sink : mSinks.Clone()) {
      if (!sink) {
        DebugOnly<bool> removed = mSinks.RemoveElement(sink);
        MOZ_ASSERT(!removed, "Sink was not explicitly removed");
        continue;
      }
      sink->PrincipalChanged();
    }
  }

  /**
   * Called by a sub class when the source's muted state has changed. Note that
   * the source is responsible for making the content black/silent during mute.
   * Notifies all sinks.
   */
  void MutedChanged(bool aNewState) {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& sink : mSinks.Clone()) {
      if (!sink) {
        DebugOnly<bool> removed = mSinks.RemoveElement(sink);
        MOZ_ASSERT(!removed, "Sink was not explicitly removed");
        continue;
      }
      sink->MutedChanged(aNewState);
    }
  }

  /**
   * Called by a sub class when the source has stopped producing data for good,
   * i.e., it has ended. Notifies all sinks.
   */
  void OverrideEnded() {
    MOZ_ASSERT(NS_IsMainThread());
    for (auto& sink : mSinks.Clone()) {
      if (!sink) {
        DebugOnly<bool> removed = mSinks.RemoveElement(sink);
        MOZ_ASSERT(!removed, "Sink was not explicitly removed");
        continue;
      }
      sink->OverrideEnded();
    }
  }

  // Principal identifying who may access the contents of this source.
  RefPtr<nsIPrincipal> mPrincipal;

  // Currently registered sinks.
  nsTArray<WeakPtr<Sink>> mSinks;

  // The label of the track we are the source of per the MediaStreamTrack spec.
  const nsString mLabel;

  // True if all MediaStreamTrack users have unregistered from this source and
  // Stop() has been called.
  bool mStopped;
};

/**
 * Basic implementation of MediaStreamTrackSource that doesn't forward Stop().
 */
class BasicTrackSource : public MediaStreamTrackSource {
 public:
  explicit BasicTrackSource(
      nsIPrincipal* aPrincipal,
      const MediaSourceEnum aMediaSource = MediaSourceEnum::Other)
      : MediaStreamTrackSource(aPrincipal, nsString()),
        mMediaSource(aMediaSource) {}

  MediaSourceEnum GetMediaSource() const override { return mMediaSource; }

  void Stop() override {}
  void Disable() override {}
  void Enable() override {}

 protected:
  ~BasicTrackSource() = default;

  const MediaSourceEnum mMediaSource;
};

/**
 * Base class that consumers of a MediaStreamTrack can use to get notifications
 * about state changes in the track.
 */
class MediaStreamTrackConsumer : public SupportsWeakPtr {
 public:
  /**
   * Called when the track's readyState transitions to "ended".
   * Unlike the "ended" event exposed to script this is called for any reason,
   * including MediaStreamTrack::Stop().
   */
  virtual void NotifyEnded(MediaStreamTrack* aTrack){};

  /**
   * Called when the track's enabled state changes.
   */
  virtual void NotifyEnabledChanged(MediaStreamTrack* aTrack, bool aEnabled){};
};

// clang-format off
/**
 * DOM wrapper for MediaTrackGraph-MediaTracks.
 *
 * To account for cloning, a MediaStreamTrack wraps two internal (and chained)
 * MediaTracks:
 *   1. mInputTrack
 *      - Controlled by the producer of the data in the track. The producer
 *        decides on lifetime of the MediaTrack and the track inside it.
 *      - It can be any type of MediaTrack.
 *      - Contains one track only.
 *   2. mTrack
 *      - A ForwardedInputTrack representing this MediaStreamTrack.
 *      - Its data is piped from mInputTrack through mPort.
 *      - Contains one track only.
 *      - When this MediaStreamTrack is enabled/disabled this is reflected in
 *        the chunks in the track in mTrack.
 *      - When this MediaStreamTrack has ended, mTrack gets destroyed.
 *        Note that mInputTrack is unaffected, such that any clones of mTrack
 *        can live on. When all clones are ended, this is signaled to the
 *        producer via our MediaStreamTrackSource. It is then likely to destroy
 *        mInputTrack.
 *
 * A graphical representation of how tracks are connected when cloned follows:
 *
 * MediaStreamTrack A
 *       mInputTrack     mTrack
 *            t1 ---------> t1
 *               \
 *                -----
 * MediaStreamTrack B  \  (clone of A)
 *       mInputTrack   \ mTrack
 *            *          -> t1
 *
 *   (*) is a copy of A's mInputTrack
 */
// clang-format on
class MediaStreamTrack : public DOMEventTargetHelper, public SupportsWeakPtr {
  // PeerConnection and friends need to know our owning DOMStream and track id.
  friend class mozilla::PeerConnectionImpl;
  friend class mozilla::PeerConnectionMedia;
  friend class mozilla::SourceStreamInfo;
  friend class mozilla::RemoteSourceStreamInfo;

  class MTGListener;
  class TrackSink;

 public:
  MediaStreamTrack(
      nsPIDOMWindowInner* aWindow, mozilla::MediaTrack* aInputTrack,
      MediaStreamTrackSource* aSource,
      MediaStreamTrackState aReadyState = MediaStreamTrackState::Live,
      bool aMuted = false,
      const MediaTrackConstraints& aConstraints = MediaTrackConstraints());

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamTrack,
                                           DOMEventTargetHelper)

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  virtual AudioStreamTrack* AsAudioStreamTrack() { return nullptr; }
  virtual VideoStreamTrack* AsVideoStreamTrack() { return nullptr; }

  virtual const AudioStreamTrack* AsAudioStreamTrack() const { return nullptr; }
  virtual const VideoStreamTrack* AsVideoStreamTrack() const { return nullptr; }

  // WebIDL
  virtual void GetKind(nsAString& aKind) = 0;
  void GetId(nsAString& aID) const;
  virtual void GetLabel(nsAString& aLabel, CallerType /* aCallerType */) {
    GetSource().GetLabel(aLabel);
  }
  bool Enabled() const { return mEnabled; }
  void SetEnabled(bool aEnabled);
  bool Muted() { return mMuted; }
  void Stop();
  void GetConstraints(dom::MediaTrackConstraints& aResult);
  void GetSettings(dom::MediaTrackSettings& aResult, CallerType aCallerType);

  already_AddRefed<Promise> ApplyConstraints(
      const dom::MediaTrackConstraints& aConstraints, CallerType aCallerType,
      ErrorResult& aRv);
  already_AddRefed<MediaStreamTrack> Clone();
  MediaStreamTrackState ReadyState() { return mReadyState; }

  IMPL_EVENT_HANDLER(mute)
  IMPL_EVENT_HANDLER(unmute)
  IMPL_EVENT_HANDLER(ended)

  /**
   * Convenience (and legacy) method for when ready state is "ended".
   */
  bool Ended() const { return mReadyState == MediaStreamTrackState::Ended; }

  /**
   * Get this track's principal.
   */
  nsIPrincipal* GetPrincipal() const { return mPrincipal; }

  /**
   * Get this track's PeerIdentity.
   */
  const PeerIdentity* GetPeerIdentity() const {
    return GetSource().GetPeerIdentity();
  }

  ProcessedMediaTrack* GetTrack() const;
  MediaTrackGraph* Graph() const;
  MediaTrackGraphImpl* GraphImpl() const;

  MediaStreamTrackSource& GetSource() const {
    MOZ_RELEASE_ASSERT(mSource,
                       "The track source is only removed on destruction");
    return *mSource;
  }

  // Webrtc allows the remote side to name tracks whatever it wants, and we
  // need to surface this to content.
  void AssignId(const nsAString& aID) { mID = aID; }

  /**
   * Add a PrincipalChangeObserver to this track.
   *
   * Returns true if it was successfully added.
   *
   * Ownership of the PrincipalChangeObserver remains with the caller, and it's
   * the caller's responsibility to remove the observer before it dies.
   */
  bool AddPrincipalChangeObserver(
      PrincipalChangeObserver<MediaStreamTrack>* aObserver);

  /**
   * Remove an added PrincipalChangeObserver from this track.
   *
   * Returns true if it was successfully removed.
   */
  bool RemovePrincipalChangeObserver(
      PrincipalChangeObserver<MediaStreamTrack>* aObserver);

  /**
   * Add a MediaStreamTrackConsumer to this track.
   *
   * Adding the same consumer multiple times is prohibited.
   */
  void AddConsumer(MediaStreamTrackConsumer* aConsumer);

  /**
   * Remove an added MediaStreamTrackConsumer from this track.
   */
  void RemoveConsumer(MediaStreamTrackConsumer* aConsumer);

  /**
   * Adds a MediaTrackListener to the MediaTrackGraph representation of
   * this track.
   */
  virtual void AddListener(MediaTrackListener* aListener);

  /**
   * Removes a MediaTrackListener from the MediaTrackGraph representation
   * of this track.
   */
  void RemoveListener(MediaTrackListener* aListener);

  /**
   * Attempts to add a direct track listener to this track.
   * Callers must listen to the NotifyInstalled event to know if installing
   * the listener succeeded (tracks originating from SourceMediaTracks) or
   * failed (e.g., WebAudio originated tracks).
   */
  virtual void AddDirectListener(DirectMediaTrackListener* aListener);
  void RemoveDirectListener(DirectMediaTrackListener* aListener);

  /**
   * Sets up a MediaInputPort from the underlying track that this
   * MediaStreamTrack represents, to aTrack, and returns it.
   */
  already_AddRefed<MediaInputPort> ForwardTrackContentsTo(
      ProcessedMediaTrack* aTrack);

 protected:
  virtual ~MediaStreamTrack();

  /**
   * Forces the ready state to a particular value, for instance when we're
   * cloning an already ended track.
   */
  virtual void SetReadyState(MediaStreamTrackState aState);

  /**
   * Notified by the MediaTrackGraph, through our owning MediaStream on the
   * main thread.
   *
   * Note that this sets the track to ended and raises the "ended" event
   * synchronously.
   */
  void OverrideEnded();

  /**
   * Called by the MTGListener when this track's PrincipalHandle changes on
   * the MediaTrackGraph thread. When the PrincipalHandle matches the pending
   * principal we know that the principal change has propagated to consumers.
   */
  void NotifyPrincipalHandleChanged(const PrincipalHandle& aNewPrincipalHandle);

  /**
   * Called when this track's readyState transitions to "ended".
   * Notifies all MediaStreamTrackConsumers that this track ended.
   */
  void NotifyEnded();

  /**
   * Called when this track's enabled state has changed.
   * Notifies all MediaStreamTrackConsumers.
   */
  void NotifyEnabledChanged();

  /**
   * Called when mSource's principal has changed.
   */
  void PrincipalChanged();

  /**
   * Called when mSource's muted state has changed.
   */
  void MutedChanged(bool aNewState);

  /**
   * Sets this track's muted state without raising any events.
   * Only really set by cloning. See MutedChanged for runtime changes.
   */
  void SetMuted(bool aMuted) { mMuted = aMuted; }

  virtual void Destroy();

  /**
   * Sets the principal and notifies PrincipalChangeObservers if it changes.
   */
  void SetPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Creates a new MediaStreamTrack with the same kind, input track, input
   * track ID and source as this MediaStreamTrack.
   */
  virtual already_AddRefed<MediaStreamTrack> CloneInternal() = 0;

  nsTArray<PrincipalChangeObserver<MediaStreamTrack>*>
      mPrincipalChangeObservers;

  nsTArray<WeakPtr<MediaStreamTrackConsumer>> mConsumers;

  // We need this to track our parent object.
  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  // The input MediaTrack assigned us by the data producer.
  // Owned by the producer.
  const RefPtr<mozilla::MediaTrack> mInputTrack;
  // The MediaTrack representing this MediaStreamTrack in the MediaTrackGraph.
  // Set on construction if we're live. Valid until we end. Owned by us.
  RefPtr<ProcessedMediaTrack> mTrack;
  // The MediaInputPort connecting mInputTrack to mTrack. Set on construction
  // if mInputTrack is non-destroyed and we're live. Valid until we end. Owned
  // by us.
  RefPtr<MediaInputPort> mPort;
  RefPtr<MediaStreamTrackSource> mSource;
  const UniquePtr<TrackSink> mSink;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPrincipal> mPendingPrincipal;
  RefPtr<MTGListener> mMTGListener;
  // Keep tracking MediaTrackListener and DirectMediaTrackListener,
  // so we can remove them in |Destory|.
  nsTArray<RefPtr<MediaTrackListener>> mTrackListeners;
  nsTArray<RefPtr<DirectMediaTrackListener>> mDirectTrackListeners;
  nsString mID;
  MediaStreamTrackState mReadyState;
  bool mEnabled;
  bool mMuted;
  dom::MediaTrackConstraints mConstraints;
};

}  // namespace dom
}  // namespace mozilla

#endif /* MEDIASTREAMTRACK_H_ */
