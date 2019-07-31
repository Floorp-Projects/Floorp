/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDOMMEDIASTREAM_H_
#define NSDOMMEDIASTREAM_H_

#include "ImageContainer.h"

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "StreamTracks.h"
#include "nsIDOMWindow.h"
#include "nsIPrincipal.h"
#include "MediaTrackConstraints.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RelativeTimeline.h"

namespace mozilla {

class AbstractThread;
class DOMMediaStream;
class MediaStream;
class MediaInputPort;
class ProcessedMediaStream;

enum class BlockingMode;

namespace dom {
class HTMLCanvasElement;
class MediaStreamTrack;
class MediaStreamTrackSource;
class AudioStreamTrack;
class VideoStreamTrack;
}  // namespace dom

namespace layers {
class ImageContainer;
class OverlayImage;
}  // namespace layers

#define NS_DOMMEDIASTREAM_IID                        \
  {                                                  \
    0x8cb65468, 0x66c0, 0x444e, {                    \
      0x89, 0x9f, 0x89, 0x1d, 0x9e, 0xd2, 0xbe, 0x7c \
    }                                                \
  }

/**
 * DOMMediaStream is the implementation of the js-exposed MediaStream interface.
 *
 * This is a thin main-thread class grouping MediaStreamTracks together.
 */
class DOMMediaStream : public DOMEventTargetHelper,
                       public RelativeTimeline,
                       public SupportsWeakPtr<DOMMediaStream> {
  typedef dom::MediaStreamTrack MediaStreamTrack;
  typedef dom::AudioStreamTrack AudioStreamTrack;
  typedef dom::VideoStreamTrack VideoStreamTrack;
  typedef dom::MediaStreamTrackSource MediaStreamTrackSource;

 public:
  typedef dom::MediaTrackConstraints MediaTrackConstraints;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(DOMMediaStream)

  class TrackListener {
   public:
    virtual ~TrackListener() {}

    /**
     * Called when the DOMMediaStream has a live track added, either by
     * script (addTrack()) or the source creating one.
     */
    virtual void NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack){};

    /**
     * Called when the DOMMediaStream removes a live track from playback, either
     * by script (removeTrack(), track.stop()) or the source ending it.
     */
    virtual void NotifyTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack){};

    /**
     * Called when the DOMMediaStream has become active.
     */
    virtual void NotifyActive(){};

    /**
     * Called when the DOMMediaStream has become inactive.
     */
    virtual void NotifyInactive(){};
  };

  explicit DOMMediaStream(nsPIDOMWindowInner* aWindow);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DOMMediaStream, DOMEventTargetHelper)
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOMMEDIASTREAM_IID)

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL

  static already_AddRefed<DOMMediaStream> Constructor(
      const dom::GlobalObject& aGlobal, ErrorResult& aRv);

  static already_AddRefed<DOMMediaStream> Constructor(
      const dom::GlobalObject& aGlobal, const DOMMediaStream& aStream,
      ErrorResult& aRv);

  static already_AddRefed<DOMMediaStream> Constructor(
      const dom::GlobalObject& aGlobal,
      const dom::Sequence<OwningNonNull<MediaStreamTrack>>& aTracks,
      ErrorResult& aRv);

  static already_AddRefed<dom::Promise> CountUnderlyingStreams(
      const dom::GlobalObject& aGlobal, ErrorResult& aRv);

  void GetId(nsAString& aID) const;

  void GetAudioTracks(nsTArray<RefPtr<AudioStreamTrack>>& aTracks) const;
  void GetAudioTracks(nsTArray<RefPtr<MediaStreamTrack>>& aTracks) const;
  void GetVideoTracks(nsTArray<RefPtr<VideoStreamTrack>>& aTracks) const;
  void GetVideoTracks(nsTArray<RefPtr<MediaStreamTrack>>& aTracks) const;
  void GetTracks(nsTArray<RefPtr<MediaStreamTrack>>& aTracks) const;
  MediaStreamTrack* GetTrackById(const nsAString& aId) const;
  void AddTrack(MediaStreamTrack& aTrack);
  void RemoveTrack(MediaStreamTrack& aTrack);
  already_AddRefed<DOMMediaStream> Clone();

  bool Active() const;

  IMPL_EVENT_HANDLER(addtrack)
  IMPL_EVENT_HANDLER(removetrack)

  // NON-WebIDL

  /**
   * Returns true if this DOMMediaStream has aTrack in mTracks.
   */
  bool HasTrack(const MediaStreamTrack& aTrack) const;

  /**
   * Returns a principal indicating who may access this stream. The stream
   * contents can only be accessed by principals subsuming this principal.
   */
  already_AddRefed<nsIPrincipal> GetPrincipal();

  // Webrtc allows the remote side to name a stream whatever it wants, and we
  // need to surface this to content.
  void AssignId(const nsAString& aID) { mID = aID; }

  /**
   * Adds a MediaStreamTrack to mTracks and raises "addtrack".
   *
   * Note that "addtrack" is raised synchronously and only has an effect if
   * this MediaStream is already exposed to script. For spec compliance this is
   * to be called from an async task.
   */
  void AddTrackInternal(MediaStreamTrack* aTrack);

  /**
   * Add an nsISupports object that this stream will keep alive as long as
   * the stream itself is alive.
   */
  void AddConsumerToKeepAlive(nsISupports* aConsumer) {
    mConsumersToKeepAlive.AppendElement(aConsumer);
  }

  // Registers a track listener to this MediaStream, for listening to changes
  // to our track set. The caller must call UnregisterTrackListener before
  // being destroyed, so we don't hold on to a dead pointer. Main thread only.
  void RegisterTrackListener(TrackListener* aListener);

  // Unregisters a track listener from this MediaStream. The caller must call
  // UnregisterTrackListener before being destroyed, so we don't hold on to
  // a dead pointer. Main thread only.
  void UnregisterTrackListener(TrackListener* aListener);

  // Tells this MediaStream whether it can go inactive as soon as no tracks
  // are live anymore.
  void SetFinishedOnInactive(bool aFinishedOnInactive);

 protected:
  virtual ~DOMMediaStream();

  void Destroy();

  // Dispatches NotifyActive() to all registered track listeners.
  void NotifyActive();

  // Dispatches NotifyInactive() to all registered track listeners.
  void NotifyInactive();

  // Dispatches NotifyTrackAdded() to all registered track listeners.
  void NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack);

  // Dispatches NotifyTrackRemoved() to all registered track listeners.
  void NotifyTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack);

  // Dispatches "addtrack" or "removetrack".
  nsresult DispatchTrackEvent(const nsAString& aName,
                              const RefPtr<MediaStreamTrack>& aTrack);

  // We need this to track our parent object.
  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  // MediaStreamTracks contained by this DOMMediaStream.
  nsTArray<RefPtr<MediaStreamTrack>> mTracks;

  // Listener tracking when live MediaStreamTracks in mTracks end.
  class PlaybackTrackListener;
  RefPtr<PlaybackTrackListener> mPlaybackTrackListener;

  nsString mID;

  // Keep these alive while the stream is alive.
  nsTArray<nsCOMPtr<nsISupports>> mConsumersToKeepAlive;

  // The track listeners subscribe to changes in this stream's track set.
  nsTArray<TrackListener*> mTrackListeners;

  // True if this stream has live tracks.
  bool mActive;

  // For compatibility with mozCaptureStream, we in some cases do not go
  // inactive until the MediaDecoder lets us. (Remove this in Bug 1302379)
  bool mFinishedOnInactive;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMMediaStream, NS_DOMMEDIASTREAM_IID)

}  // namespace mozilla

#endif /* NSDOMMEDIASTREAM_H_ */
