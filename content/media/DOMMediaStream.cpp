/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMediaStream.h"
#include "nsContentUtils.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/LocalMediaStreamBinding.h"
#include "mozilla/dom/AudioNode.h"
#include "MediaStreamGraph.h"
#include "AudioStreamTrack.h"
#include "VideoStreamTrack.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMMediaStream)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMediaStream)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMMediaStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMMediaStream)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTracks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsumersToKeepAlive)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMMediaStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsumersToKeepAlive)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(DOMMediaStream)

NS_IMPL_ISUPPORTS_INHERITED(DOMLocalMediaStream, DOMMediaStream,
                            nsIDOMLocalMediaStream)

NS_IMPL_CYCLE_COLLECTION_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream,
                                   mStreamNode)

NS_IMPL_ADDREF_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMAudioNodeMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

class DOMMediaStream::StreamListener : public MediaStreamListener {
public:
  StreamListener(DOMMediaStream* aStream)
    : mStream(aStream)
  {}

  // Main thread only
  void Forget() { mStream = nullptr; }
  DOMMediaStream* GetStream() { return mStream; }

  class TrackChange : public nsRunnable {
  public:
    TrackChange(StreamListener* aListener,
                TrackID aID, TrackTicks aTrackOffset,
                uint32_t aEvents, MediaSegment::Type aType)
      : mListener(aListener), mID(aID), mEvents(aEvents), mType(aType)
    {
    }

    NS_IMETHOD Run()
    {
      NS_ASSERTION(NS_IsMainThread(), "main thread only");

      DOMMediaStream* stream = mListener->GetStream();
      if (!stream) {
        return NS_OK;
      }

      nsRefPtr<MediaStreamTrack> track;
      if (mEvents & MediaStreamListener::TRACK_EVENT_CREATED) {
        track = stream->CreateDOMTrack(mID, mType);
      } else {
        track = stream->GetDOMTrackFor(mID);
      }
      if (mEvents & MediaStreamListener::TRACK_EVENT_ENDED) {
        track->NotifyEnded();
      }
      return NS_OK;
    }

    StreamTime mEndTime;
    nsRefPtr<StreamListener> mListener;
    TrackID mID;
    uint32_t mEvents;
    MediaSegment::Type mType;
  };

  /**
   * Notify that changes to one of the stream tracks have been queued.
   * aTrackEvents can be any combination of TRACK_EVENT_CREATED and
   * TRACK_EVENT_ENDED. aQueuedMedia is the data being added to the track
   * at aTrackOffset (relative to the start of the stream).
   * aQueuedMedia can be null if there is no output.
   */
  virtual void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                        TrackRate aTrackRate,
                                        TrackTicks aTrackOffset,
                                        uint32_t aTrackEvents,
                                        const MediaSegment& aQueuedMedia) MOZ_OVERRIDE
  {
    if (aTrackEvents & (TRACK_EVENT_CREATED | TRACK_EVENT_ENDED)) {
      nsRefPtr<TrackChange> runnable =
        new TrackChange(this, aID, aTrackOffset, aTrackEvents,
                        aQueuedMedia.GetType());
      NS_DispatchToMainThread(runnable);
    }
  }

private:
  // These fields may only be accessed on the main thread
  DOMMediaStream* mStream;
};

DOMMediaStream::DOMMediaStream()
  : mLogicalStreamStartTime(0),
    mStream(nullptr), mHintContents(0), mTrackTypesAvailable(0),
    mNotifiedOfMediaStreamGraphShutdown(false)
{
  SetIsDOMBinding();
}

DOMMediaStream::~DOMMediaStream()
{
  Destroy();
}

void
DOMMediaStream::Destroy()
{
  if (mListener) {
    mListener->Forget();
    mListener = nullptr;
  }
  if (mStream) {
    mStream->Destroy();
    mStream = nullptr;
  }
}

JSObject*
DOMMediaStream::WrapObject(JSContext* aCx)
{
  return dom::MediaStreamBinding::Wrap(aCx, this);
}

double
DOMMediaStream::CurrentTime()
{
  if (!mStream) {
    return 0.0;
  }
  return MediaTimeToSeconds(mStream->GetCurrentTime() - mLogicalStreamStartTime);
}

void
DOMMediaStream::GetAudioTracks(nsTArray<nsRefPtr<AudioStreamTrack> >& aTracks)
{
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    AudioStreamTrack* t = mTracks[i]->AsAudioStreamTrack();
    if (t) {
      aTracks.AppendElement(t);
    }
  }
}

void
DOMMediaStream::GetVideoTracks(nsTArray<nsRefPtr<VideoStreamTrack> >& aTracks)
{
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    VideoStreamTrack* t = mTracks[i]->AsVideoStreamTrack();
    if (t) {
      aTracks.AppendElement(t);
    }
  }
}

bool
DOMMediaStream::IsFinished()
{
  return !mStream || mStream->IsFinished();
}

void
DOMMediaStream::InitSourceStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents)
{
  mWindow = aWindow;
  SetHintContents(aHintContents);
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  InitStreamCommon(gm->CreateSourceStream(this));
}

void
DOMMediaStream::InitTrackUnionStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents)
{
  mWindow = aWindow;
  SetHintContents(aHintContents);
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  InitStreamCommon(gm->CreateTrackUnionStream(this));
}

void
DOMMediaStream::InitStreamCommon(MediaStream* aStream)
{
  mStream = aStream;

  // Setup track listener
  mListener = new StreamListener(this);
  aStream->AddListener(mListener);
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateSourceStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents)
{
  nsRefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitSourceStream(aWindow, aHintContents);
  return stream.forget();
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateTrackUnionStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents)
{
  nsRefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitTrackUnionStream(aWindow, aHintContents);
  return stream.forget();
}

void
DOMMediaStream::SetTrackEnabled(TrackID aTrackID, bool aEnabled)
{
  if (mStream) {
    mStream->SetTrackEnabled(aTrackID, aEnabled);
  }
}

bool
DOMMediaStream::CombineWithPrincipal(nsIPrincipal* aPrincipal)
{
  bool changed =
    nsContentUtils::CombineResourcePrincipals(&mPrincipal, aPrincipal);
  if (changed) {
    NotifyPrincipalChanged();
  }
  return changed;
}

void
DOMMediaStream::SetPrincipal(nsIPrincipal* aPrincipal)
{
  mPrincipal = aPrincipal;
  NotifyPrincipalChanged();
}

void
DOMMediaStream::NotifyPrincipalChanged()
{
  for (uint32_t i = 0; i < mPrincipalChangeObservers.Length(); ++i) {
    mPrincipalChangeObservers[i]->PrincipalChanged(this);
  }
}


bool
DOMMediaStream::AddPrincipalChangeObserver(PrincipalChangeObserver* aObserver)
{
  return mPrincipalChangeObservers.AppendElement(aObserver) != nullptr;
}

bool
DOMMediaStream::RemovePrincipalChangeObserver(PrincipalChangeObserver* aObserver)
{
  return mPrincipalChangeObservers.RemoveElement(aObserver);
}

MediaStreamTrack*
DOMMediaStream::CreateDOMTrack(TrackID aTrackID, MediaSegment::Type aType)
{
  MediaStreamTrack* track;
  switch (aType) {
  case MediaSegment::AUDIO:
    track = new AudioStreamTrack(this, aTrackID);
    mTrackTypesAvailable |= HINT_CONTENTS_AUDIO;
    break;
  case MediaSegment::VIDEO:
    track = new VideoStreamTrack(this, aTrackID);
    mTrackTypesAvailable |= HINT_CONTENTS_VIDEO;
    break;
  default:
    MOZ_CRASH("Unhandled track type");
  }
  mTracks.AppendElement(track);

  CheckTracksAvailable();

  return track;
}

MediaStreamTrack*
DOMMediaStream::GetDOMTrackFor(TrackID aTrackID)
{
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    MediaStreamTrack* t = mTracks[i];
    // We may add streams to our track list that are actually owned by
    // a different DOMMediaStream. Ignore those.
    if (t->GetTrackID() == aTrackID && t->GetStream() == this) {
      return t;
    }
  }
  return nullptr;
}

void
DOMMediaStream::NotifyMediaStreamGraphShutdown()
{
  // No more tracks will ever be added, so just clear these callbacks now
  // to prevent leaks.
  mNotifiedOfMediaStreamGraphShutdown = true;
  mRunOnTracksAvailable.Clear();

  mConsumersToKeepAlive.Clear();
}

void
DOMMediaStream::NotifyStreamStateChanged()
{
  if (IsFinished()) {
    mConsumersToKeepAlive.Clear();
  }
}

void
DOMMediaStream::OnTracksAvailable(OnTracksAvailableCallback* aRunnable)
{
  if (mNotifiedOfMediaStreamGraphShutdown) {
    // No more tracks will ever be added, so just delete the callback now.
    delete aRunnable;
    return;
  }
  mRunOnTracksAvailable.AppendElement(aRunnable);
  CheckTracksAvailable();
}

void
DOMMediaStream::CheckTracksAvailable()
{
  if (mTrackTypesAvailable == 0) {
    return;
  }
  nsTArray<nsAutoPtr<OnTracksAvailableCallback> > callbacks;
  callbacks.SwapElements(mRunOnTracksAvailable);

  for (uint32_t i = 0; i < callbacks.Length(); ++i) {
    OnTracksAvailableCallback* cb = callbacks[i];
    if (~mTrackTypesAvailable & cb->GetExpectedTracks()) {
      // Some expected tracks not available yet. Try this callback again later.
      *mRunOnTracksAvailable.AppendElement() = callbacks[i].forget();
      continue;
    }
    cb->NotifyTracksAvailable(this);
  }
}

DOMLocalMediaStream::~DOMLocalMediaStream()
{
  if (mStream) {
    // Make sure Listeners of this stream know it's going away
    Stop();
  }
}

JSObject*
DOMLocalMediaStream::WrapObject(JSContext* aCx)
{
  return dom::LocalMediaStreamBinding::Wrap(aCx, this);
}

void
DOMLocalMediaStream::Stop()
{
  if (mStream && mStream->AsSourceStream()) {
    mStream->AsSourceStream()->EndAllTrackAndFinish();
  }
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateSourceStream(nsIDOMWindow* aWindow,
                                        TrackTypeHints aHintContents)
{
  nsRefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
  stream->InitSourceStream(aWindow, aHintContents);
  return stream.forget();
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateTrackUnionStream(nsIDOMWindow* aWindow,
                                            TrackTypeHints aHintContents)
{
  nsRefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
  stream->InitTrackUnionStream(aWindow, aHintContents);
  return stream.forget();
}

DOMAudioNodeMediaStream::DOMAudioNodeMediaStream(AudioNode* aNode)
: mStreamNode(aNode)
{
}

already_AddRefed<DOMAudioNodeMediaStream>
DOMAudioNodeMediaStream::CreateTrackUnionStream(nsIDOMWindow* aWindow,
                                                AudioNode* aNode,
                                                TrackTypeHints aHintContents)
{
  nsRefPtr<DOMAudioNodeMediaStream> stream = new DOMAudioNodeMediaStream(aNode);
  stream->InitTrackUnionStream(aWindow, aHintContents);
  return stream.forget();
}
