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
   * Create an nsDOMMediaStream whose underlying stream is a SourceMediaStream.
   */
  static already_AddRefed<DOMMediaStream>
  CreateSourceStream(nsIDOMWindow* aWindow, uint32_t aHintContents);

  // Hints to tell the SDP generator about whether this
  // MediaStream probably has audio and/or video
  enum {
    HINT_CONTENTS_AUDIO = 0x00000001U,
    HINT_CONTENTS_VIDEO = 0x00000002U
  };
  uint32_t GetHintContents() const { return mHintContents; }
  void SetHintContents(uint32_t aHintContents) { mHintContents = aHintContents; }

  /**
   * Create an nsDOMMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMMediaStream>
  CreateTrackUnionStream(nsIDOMWindow* aWindow, uint32_t aHintContents = 0);

  // Notifications from StreamListener
  MediaStreamTrack* CreateDOMTrack(TrackID aTrackID, MediaSegment::Type aType);
  MediaStreamTrack* GetDOMTrackFor(TrackID aTrackID);

protected:
  void InitSourceStream(nsIDOMWindow* aWindow, uint32_t aHintContents);
  void InitTrackUnionStream(nsIDOMWindow* aWindow, uint32_t aHintContents);
  void InitStreamCommon(MediaStream* aStream);

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

  // tells the SDP generator about whether this
  // MediaStream probably has audio and/or video
  uint32_t mHintContents;
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
  CreateSourceStream(nsIDOMWindow* aWindow, uint32_t aHintContents);

  /**
   * Create an nsDOMLocalMediaStream whose underlying stream is a TrackUnionStream.
   */
  static already_AddRefed<DOMLocalMediaStream>
  CreateTrackUnionStream(nsIDOMWindow* aWindow, uint32_t aHintContents = 0);
};

}

#endif /* NSDOMMEDIASTREAM_H_ */
