/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDOMMEDIASTREAM_H_
#define NSDOMMEDIASTREAM_H_

#include "ImageContainer.h"

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "StreamBuffer.h"
#include "nsIDOMWindow.h"
#include "nsIPrincipal.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/CORSMode.h"

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with NS_DECL_NSIDOMMEDIASTREAM, containing
// currentTime getter.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
// X11 has a #define for CurrentTime. Unbelievable :-(.
// See dom/media/webaudio/AudioContext.h for more fun!
#ifdef CurrentTime
#undef CurrentTime
#endif

namespace mozilla {

class DOMHwMediaStream;
class DOMLocalMediaStream;
class MediaStream;
class MediaEngineSource;
class MediaStreamGraph;

namespace dom {
class AudioNode;
class HTMLCanvasElement;
class MediaStreamTrack;
class AudioStreamTrack;
class VideoStreamTrack;
class AudioTrack;
class VideoTrack;
class AudioTrackList;
class VideoTrackList;
class MediaTrackListListener;
} // namespace dom

namespace layers {
class ImageContainer;
class OverlayImage;
} // namespace layers

class MediaStreamDirectListener;

#define NS_DOMMEDIASTREAM_IID \
{ 0x8cb65468, 0x66c0, 0x444e, \
  { 0x89, 0x9f, 0x89, 0x1d, 0x9e, 0xd2, 0xbe, 0x7c } }

/**
 * DOM wrapper for MediaStreams.
 */
class DOMMediaStream : public DOMEventTargetHelper
{
  friend class DOMLocalMediaStream;
  typedef dom::MediaStreamTrack MediaStreamTrack;
  typedef dom::AudioStreamTrack AudioStreamTrack;
  typedef dom::VideoStreamTrack VideoStreamTrack;
  typedef dom::AudioTrack AudioTrack;
  typedef dom::VideoTrack VideoTrack;
  typedef dom::AudioTrackList AudioTrackList;
  typedef dom::VideoTrackList VideoTrackList;
  typedef dom::MediaTrackListListener MediaTrackListListener;

public:
  typedef uint8_t TrackTypeHints;

  DOMMediaStream();

  NS_DECL_ISUPPORTS_INHERITED
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DOMMediaStream,
                                           DOMEventTargetHelper)
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOMMEDIASTREAM_IID)

  nsIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  double CurrentTime();

  void GetId(nsAString& aID) const;

  void GetAudioTracks(nsTArray<nsRefPtr<AudioStreamTrack> >& aTracks);
  void GetVideoTracks(nsTArray<nsRefPtr<VideoStreamTrack> >& aTracks);
  void GetTracks(nsTArray<nsRefPtr<MediaStreamTrack> >& aTracks);
  bool HasTrack(const MediaStreamTrack& aTrack) const;

  MediaStream* GetStream() const { return mStream; }

  /**
   * Overridden in DOMLocalMediaStreams to allow getUserMedia to pass
   * data directly to RTCPeerConnection without going through graph queuing.
   * Returns a bool to let us know if direct data will be delivered.
   */
  virtual bool AddDirectListener(MediaStreamDirectListener *aListener) { return false; }
  virtual void RemoveDirectListener(MediaStreamDirectListener *aListener) {}

  /**
   * Overridden in DOMLocalMediaStreams to allow getUserMedia to disable
   * media at the SourceMediaStream.
   */
  virtual void SetTrackEnabled(TrackID aTrackID, bool aEnabled);

  virtual void StopTrack(TrackID aTrackID);

  virtual DOMLocalMediaStream* AsDOMLocalMediaStream() { return nullptr; }
  virtual DOMHwMediaStream* AsDOMHwMediaStream() { return nullptr; }

  bool IsFinished();
  /**
   * Returns a principal indicating who may access this stream. The stream contents
   * can only be accessed by principals subsuming this principal.
   */
  nsIPrincipal* GetPrincipal() { return mPrincipal; }
  mozilla::CORSMode GetCORSMode();
  void SetCORSMode(mozilla::CORSMode aCORSMode);

  /**
   * These are used in WebRTC.  A peerIdentity constrained MediaStream cannot be sent
   * across the network to anything other than a peer with the provided identity.
   * If this is set, then mPrincipal should be an instance of nsNullPrincipal.
   */
  PeerIdentity* GetPeerIdentity() const { return mPeerIdentity; }
  void SetPeerIdentity(PeerIdentity* aPeerIdentity)
  {
    mPeerIdentity = aPeerIdentity;
  }

  /**
   * Indicate that data will be contributed to this stream from origin aPrincipal.
   * If aPrincipal is null, this is ignored. Otherwise, from now on the contents
   * of this stream can only be accessed by principals that subsume aPrincipal.
   * Returns true if the stream's principal changed.
   */
  bool CombineWithPrincipal(nsIPrincipal* aPrincipal);

  /**
   * This is used in WebRTC to move from a protected state (nsNullPrincipal) to
   * one where the stream is accessible to script.  Don't call this.
   * CombineWithPrincipal is almost certainly more appropriate.
   */
  void SetPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Used to learn about dynamic changes in principal occur.
   * Operations relating to these observers must be confined to the main thread.
   */
  class PrincipalChangeObserver
  {
  public:
    virtual void PrincipalChanged(DOMMediaStream* aMediaStream) = 0;
  };
  bool AddPrincipalChangeObserver(PrincipalChangeObserver* aObserver);
  bool RemovePrincipalChangeObserver(PrincipalChangeObserver* aObserver);

  /**
   * Called when this stream's MediaStreamGraph has been shut down. Normally
   * MSGs are only shut down when all streams have been removed, so this
   * will only be called during a forced shutdown due to application exit.
   */
  void NotifyMediaStreamGraphShutdown();
  /**
   * Called when the main-thread state of the MediaStream goes to finished.
   */
  void NotifyStreamFinished();

  // Webrtc allows the remote side to name a stream whatever it wants, and we
  // need to surface this to content.
  void AssignId(const nsAString& aID) { mID = aID; }

  /**
   * Create an nsDOMMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<DOMMediaStream> CreateSourceStream(nsIDOMWindow* aWindow,
                                                             MediaStreamGraph* aGraph);

  /**
   * Create an nsDOMMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMMediaStream> CreateTrackUnionStream(nsIDOMWindow* aWindow,
                                                                 MediaStreamGraph* aGraph);

  /**
   * Create an nsDOMMediaStream whose underlying stream is an
   * AudioCaptureStream
   */
  static already_AddRefed<DOMMediaStream> CreateAudioCaptureStream(
    nsIDOMWindow* aWindow, MediaStreamGraph* aGraph);

  void SetLogicalStreamStartTime(StreamTime aTime)
  {
    mLogicalStreamStartTime = aTime;
  }

  // Notifications from StreamListener.
  // BindDOMTrack should only be called when it's safe to run script.
  MediaStreamTrack* BindDOMTrack(TrackID aTrackID, MediaSegment::Type aType);
  MediaStreamTrack* CreateDOMTrack(TrackID aTrackID, MediaSegment::Type aType);
  MediaStreamTrack* GetDOMTrackFor(TrackID aTrackID);

  class OnTracksAvailableCallback {
  public:
    virtual ~OnTracksAvailableCallback() {}
    virtual void NotifyTracksAvailable(DOMMediaStream* aStream) = 0;
  };
  // When the initial set of tracks has been added, run
  // aCallback->NotifyTracksAvailable.
  // It is allowed to do anything, including run script.
  // aCallback may run immediately during this call if tracks are already
  // available!
  // We only care about track additions, we'll fire the notification even if
  // some of the tracks have been removed.
  // Takes ownership of aCallback.
  void OnTracksAvailable(OnTracksAvailableCallback* aCallback);

  /**
   * Add an nsISupports object that this stream will keep alive as long as
   * the stream is not finished.
   */
  void AddConsumerToKeepAlive(nsISupports* aConsumer)
  {
    if (!IsFinished() && !mNotifiedOfMediaStreamGraphShutdown) {
      mConsumersToKeepAlive.AppendElement(aConsumer);
    }
  }

  /**
   * If loading and playing a MediaStream in a media element, for each
   * MediaStreamTrack in the MediaStream, create a corresponding AudioTrack or
   * VideoTrack during the phase of resource fetching.
   */
  void ConstructMediaTracks(AudioTrackList* aAudioTrackList,
                            VideoTrackList* aVideoTrackList);

  /**
   * MUST call this before the AudioTrackList or VideoTrackList go away
   */
  void DisconnectTrackListListeners(const AudioTrackList* aAudioTrackList,
                                    const VideoTrackList* aVideoTrackList);

  virtual void NotifyMediaStreamTrackCreated(MediaStreamTrack* aTrack);

  virtual void NotifyMediaStreamTrackEnded(MediaStreamTrack* aTrack);

protected:
  virtual ~DOMMediaStream();

  void Destroy();
  void InitSourceStream(nsIDOMWindow* aWindow,
                        MediaStreamGraph* aGraph);
  void InitTrackUnionStream(nsIDOMWindow* aWindow,
                            MediaStreamGraph* aGraph);
  void InitAudioCaptureStream(nsIDOMWindow* aWindow,
                              MediaStreamGraph* aGraph);
  void InitStreamCommon(MediaStream* aStream);
  already_AddRefed<AudioTrack> CreateAudioTrack(AudioStreamTrack* aStreamTrack);
  already_AddRefed<VideoTrack> CreateVideoTrack(VideoStreamTrack* aStreamTrack);

  // Called when MediaStreamGraph has finished an iteration where tracks were
  // created.
  void TracksCreated();

  void CheckTracksAvailable();

  class StreamListener;
  friend class StreamListener;

  // StreamTime at which the currentTime attribute would return 0.
  StreamTime mLogicalStreamStartTime;

  // We need this to track our parent object.
  nsCOMPtr<nsIDOMWindow> mWindow;

  // MediaStream is owned by the graph, but we tell it when to die, and it won't
  // die until we let it.
  MediaStream* mStream;

  nsAutoTArray<nsRefPtr<MediaStreamTrack>,2> mTracks;
  nsRefPtr<StreamListener> mListener;

  nsTArray<nsAutoPtr<OnTracksAvailableCallback> > mRunOnTracksAvailable;

  // Set to true after MediaStreamGraph has created tracks for mStream.
  bool mTracksCreated;

  nsString mID;

  // Keep these alive until the stream finishes
  nsTArray<nsCOMPtr<nsISupports> > mConsumersToKeepAlive;

  bool mNotifiedOfMediaStreamGraphShutdown;

  // Send notifications to AudioTrackList or VideoTrackList, if this MediaStream
  // is consumed by a HTMLMediaElement.
  nsTArray<MediaTrackListListener> mMediaTrackListListeners;

private:
  void NotifyPrincipalChanged();

  // Principal identifying who may access the contents of this stream.
  // If null, this stream can be used by anyone because it has no content yet.
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsTArray<PrincipalChangeObserver*> mPrincipalChangeObservers;
  // this is used in gUM and WebRTC to identify peers that this stream
  // is allowed to be sent to
  nsAutoPtr<PeerIdentity> mPeerIdentity;
  CORSMode mCORSMode;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMMediaStream,
                              NS_DOMMEDIASTREAM_IID)

#define NS_DOMLOCALMEDIASTREAM_IID \
{ 0xb1437260, 0xec61, 0x4dfa, \
  { 0x92, 0x54, 0x04, 0x44, 0xe2, 0xb5, 0x94, 0x9c } }

class DOMLocalMediaStream : public DOMMediaStream
{
public:
  DOMLocalMediaStream() {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOMLOCALMEDIASTREAM_IID)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual void Stop();

  virtual MediaEngineSource* GetMediaEngine(TrackID aTrackID) { return nullptr; }

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<DOMLocalMediaStream>
  CreateSourceStream(nsIDOMWindow* aWindow,
                     MediaStreamGraph* aGraph);

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMLocalMediaStream>
  CreateTrackUnionStream(nsIDOMWindow* aWindow,
                         MediaStreamGraph* aGraph);

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is an
   * AudioCaptureStream. */
  static already_AddRefed<DOMLocalMediaStream> CreateAudioCaptureStream(
    nsIDOMWindow* aWindow, MediaStreamGraph* aGraph);

protected:
  virtual ~DOMLocalMediaStream();
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMLocalMediaStream,
                              NS_DOMLOCALMEDIASTREAM_IID)

class DOMAudioNodeMediaStream : public DOMMediaStream
{
  typedef dom::AudioNode AudioNode;
public:
  explicit DOMAudioNodeMediaStream(AudioNode* aNode);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream)

  /**
   * Create a DOMAudioNodeMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMAudioNodeMediaStream>
  CreateTrackUnionStream(nsIDOMWindow* aWindow,
                         AudioNode* aNode,
                         MediaStreamGraph* aGraph);

protected:
  ~DOMAudioNodeMediaStream();

private:
  // If this object wraps a stream owned by an AudioNode, we need to ensure that
  // the node isn't cycle-collected too early.
  nsRefPtr<AudioNode> mStreamNode;
};

class DOMHwMediaStream : public DOMLocalMediaStream
{
  typedef mozilla::gfx::IntSize IntSize;
  typedef layers::ImageContainer ImageContainer;
#ifdef MOZ_WIDGET_GONK
  typedef layers::OverlayImage OverlayImage;
  typedef layers::OverlayImage::Data Data;
#endif

public:
  DOMHwMediaStream();

  static already_AddRefed<DOMHwMediaStream> CreateHwStream(nsIDOMWindow* aWindow);
  virtual DOMHwMediaStream* AsDOMHwMediaStream() override { return this; }
  int32_t RequestOverlayId();
  void SetOverlayId(int32_t aOverlayId);
  void SetImageSize(uint32_t width, uint32_t height);

protected:
  ~DOMHwMediaStream();

private:
  void Init(MediaStream* aStream);

#ifdef MOZ_WIDGET_GONK
  nsRefPtr<ImageContainer> mImageContainer;
  const int DEFAULT_IMAGE_ID = 0x01;
  const int DEFAULT_IMAGE_WIDTH = 400;
  const int DEFAULT_IMAGE_HEIGHT = 300;
  nsRefPtr<OverlayImage> mOverlayImage;
  Data mImageData;
#endif
};

} // namespace mozilla

#endif /* NSDOMMEDIASTREAM_H_ */
