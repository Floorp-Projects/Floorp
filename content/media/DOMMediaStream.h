/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDOMMEDIASTREAM_H_
#define NSDOMMEDIASTREAM_H_

#include "nsIDOMMediaStream.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPrincipal.h"
#include "nsWrapperCache.h"
#include "nsIDOMWindow.h"
#include "StreamBuffer.h"
#include "nsIRunnable.h"

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
class MediaStreamTrack;
class AudioStreamTrack;
class VideoStreamTrack;
}

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
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  double CurrentTime();
  void GetAudioTracks(nsTArray<nsRefPtr<AudioStreamTrack> >& aTracks);
  void GetVideoTracks(nsTArray<nsRefPtr<VideoStreamTrack> >& aTracks);

  MediaStream* GetStream() { return mStream; }
  bool IsFinished();
  /**
   * Returns a principal indicating who may access this stream. The stream contents
   * can only be accessed by principals subsuming this principal.
   */
  nsIPrincipal* GetPrincipal() { return mPrincipal; }

  /**
   * Indicate that data will be contributed to this stream from origin aPrincipal.
   * If aPrincipal is null, this is ignored. Otherwise, from now on the contents
   * of this stream can only be accessed by principals that subsume aPrincipal.
   * Returns true if the stream's principal changed.
   */
  bool CombineWithPrincipal(nsIPrincipal* aPrincipal);

  /**
   * Called when this stream's MediaStreamGraph has been shut down. Normally
   * MSGs are only shut down when all streams have been removed, so this
   * will only be called during a forced shutdown due to application exit.
   */
  void NotifyMediaStreamGraphShutdown();

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
  void OnTracksAvailable(OnTracksAvailableCallback* aCallback);

protected:
  void Destroy();
  void InitSourceStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents);
  void InitTrackUnionStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents);
  void InitStreamCommon(MediaStream* aStream);
  void CheckTracksAvailable();

  class StreamListener;
  friend class StreamListener;

  // We need this to track our parent object.
  nsCOMPtr<nsIDOMWindow> mWindow;

  // MediaStream is owned by the graph, but we tell it when to die, and it won't
  // die until we let it.
  MediaStream* mStream;
  // Principal identifying who may access the contents of this stream.
  // If null, this stream can be used by anyone because it has no content yet.
  nsCOMPtr<nsIPrincipal> mPrincipal;

  nsAutoTArray<nsRefPtr<MediaStreamTrack>,2> mTracks;
  nsRefPtr<StreamListener> mListener;

  nsTArray<nsAutoPtr<OnTracksAvailableCallback> > mRunOnTracksAvailable;

  // Indicate what track types we eventually expect to add to this stream
  uint8_t mHintContents;
  // Indicate what track types have been added to this stream
  uint8_t mTrackTypesAvailable;
  bool mNotifiedOfMediaStreamGraphShutdown;
};

class DOMLocalMediaStream : public DOMMediaStream,
                            public nsIDOMLocalMediaStream
{
public:
  DOMLocalMediaStream() {}
  virtual ~DOMLocalMediaStream();

  NS_DECL_ISUPPORTS_INHERITED

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

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

}

#endif /* NSDOMMEDIASTREAM_H_ */
