/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDOMMEDIASTREAM_H_
#define NSDOMMEDIASTREAM_H_

#include "nsIDOMMediaStream.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "StreamBuffer.h"
#include "nsIDOMWindow.h"
#include "nsIPrincipal.h"
#include "mozilla/PeerIdentity.h"

class nsXPCClassInfo;

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with NS_DECL_NSIDOMMEDIASTREAM, containing
// currentTime getter.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
// X11 has a #define for CurrentTime. Unbelievable :-(.
// See content/media/webaudio/AudioContext.h for more fun!
#ifdef CurrentTime
#undef CurrentTime
#endif

namespace mozilla {

class MediaStream;

namespace dom {
class AudioNode;
class MediaStreamTrack;
class AudioStreamTrack;
class VideoStreamTrack;
}

class MediaStreamDirectListener;

/**
 * DOM wrapper for MediaStreams.
 */
class DOMMediaStream : public nsIDOMMediaStream,
                       public nsWrapperCache
{
  friend class DOMLocalMediaStream;
  typedef dom::MediaStreamTrack MediaStreamTrack;
  typedef dom::AudioStreamTrack AudioStreamTrack;
  typedef dom::VideoStreamTrack VideoStreamTrack;

public:
  typedef uint8_t TrackTypeHints;

  DOMMediaStream();
  virtual ~DOMMediaStream();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMMediaStream)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  nsIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  double CurrentTime();

  void GetAudioTracks(nsTArray<nsRefPtr<AudioStreamTrack> >& aTracks);
  void GetVideoTracks(nsTArray<nsRefPtr<VideoStreamTrack> >& aTracks);

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

  bool IsFinished();
  /**
   * Returns a principal indicating who may access this stream. The stream contents
   * can only be accessed by principals subsuming this principal.
   */
  nsIPrincipal* GetPrincipal() { return mPrincipal; }

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
   * Called when the main-thread state of the MediaStream changed.
   */
  void NotifyStreamStateChanged();

  // Indicate what track types we eventually expect to add to this stream
  enum {
    HINT_CONTENTS_AUDIO = 1 << 0,
    HINT_CONTENTS_VIDEO = 1 << 1
  };
  TrackTypeHints GetHintContents() const { return mHintContents; }
  void SetHintContents(TrackTypeHints aHintContents) { mHintContents = aHintContents; }

  /**
   * Create an nsDOMMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<DOMMediaStream>
  CreateSourceStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents);

  /**
   * Create an nsDOMMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMMediaStream>
  CreateTrackUnionStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents = 0);

  void SetLogicalStreamStartTime(StreamTime aTime)
  {
    mLogicalStreamStartTime = aTime;
  }

  // Notifications from StreamListener.
  // CreateDOMTrack should only be called when it's safe to run script.
  MediaStreamTrack* CreateDOMTrack(TrackID aTrackID, MediaSegment::Type aType);
  MediaStreamTrack* GetDOMTrackFor(TrackID aTrackID);

  class OnTracksAvailableCallback {
  public:
    OnTracksAvailableCallback(uint8_t aExpectedTracks = 0)
      : mExpectedTracks(aExpectedTracks) {}
    virtual ~OnTracksAvailableCallback() {}
    virtual void NotifyTracksAvailable(DOMMediaStream* aStream) = 0;
    TrackTypeHints GetExpectedTracks() { return mExpectedTracks; }
    void SetExpectedTracks(TrackTypeHints aExpectedTracks) { mExpectedTracks = aExpectedTracks; }
  private:
    TrackTypeHints mExpectedTracks;
  };
  // When one track of the appropriate type has been added for each bit set
  // in aCallback->GetExpectedTracks(), run aCallback->NotifyTracksAvailable.
  // It is allowed to do anything, including run script.
  // aCallback may run immediately during this call if tracks are already
  // available!
  // We only care about track additions, we'll fire the notification even if
  // some of the tracks have been removed.
  // Takes ownership of aCallback.
  // If GetExpectedTracks() returns 0, the callback will be fired as soon as there are any tracks.
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

protected:
  void Destroy();
  void InitSourceStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents);
  void InitTrackUnionStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents);
  void InitStreamCommon(MediaStream* aStream);

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

  // Keep these alive until the stream finishes
  nsTArray<nsCOMPtr<nsISupports> > mConsumersToKeepAlive;

  // Indicate what track types we eventually expect to add to this stream
  uint8_t mHintContents;
  // Indicate what track types have been added to this stream
  uint8_t mTrackTypesAvailable;
  bool mNotifiedOfMediaStreamGraphShutdown;

private:
  void NotifyPrincipalChanged();

  // Principal identifying who may access the contents of this stream.
  // If null, this stream can be used by anyone because it has no content yet.
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsTArray<PrincipalChangeObserver*> mPrincipalChangeObservers;
  // this is used in gUM and WebRTC to identify peers that this stream
  // is allowed to be sent to
  nsAutoPtr<PeerIdentity> mPeerIdentity;
};

class DOMLocalMediaStream : public DOMMediaStream,
                            public nsIDOMLocalMediaStream
{
public:
  DOMLocalMediaStream() {}
  virtual ~DOMLocalMediaStream();

  NS_DECL_ISUPPORTS_INHERITED

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual void Stop();

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<DOMLocalMediaStream>
  CreateSourceStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents);

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMLocalMediaStream>
  CreateTrackUnionStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents = 0);
};

class DOMAudioNodeMediaStream : public DOMMediaStream
{
  typedef dom::AudioNode AudioNode;
public:
  DOMAudioNodeMediaStream(AudioNode* aNode);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream)

  /**
   * Create a DOMAudioNodeMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMAudioNodeMediaStream>
  CreateTrackUnionStream(nsIDOMWindow* aWindow,
                         AudioNode* aNode,
                         TrackTypeHints aHintContents = 0);

private:
  // If this object wraps a stream owned by an AudioNode, we need to ensure that
  // the node isn't cycle-collected too early.
  nsRefPtr<AudioNode> mStreamNode;
};

}

#endif /* NSDOMMEDIASTREAM_H_ */
