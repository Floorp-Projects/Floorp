/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMediaStream.h"
#include "nsContentUtils.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/LocalMediaStreamBinding.h"
#include "mozilla/dom/AudioNode.h"
#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/AudioTrackList.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/VideoTrackList.h"
#include "MediaStreamGraph.h"
#include "AudioStreamTrack.h"
#include "VideoStreamTrack.h"
#include "MediaEngine.h"

using namespace mozilla;
using namespace mozilla::dom;

class DOMMediaStream::StreamListener : public MediaStreamListener {
public:
  explicit StreamListener(DOMMediaStream* aStream)
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
        track = stream->BindDOMTrack(mID, mType);
        if (!track) {
          stream->CreateDOMTrack(mID, mType);
          track = stream->BindDOMTrack(mID, mType);
          stream->NotifyMediaStreamTrackCreated(track);
        }
      } else {
        track = stream->GetDOMTrackFor(mID);
      }
      if (mEvents & MediaStreamListener::TRACK_EVENT_ENDED) {
        if (track) {
          track->NotifyEnded();
          stream->NotifyMediaStreamTrackEnded(track);
        } else {
          NS_ERROR("track ended but not found");
        }
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

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMMediaStream,
                                                DOMEventTargetHelper)
  if (tmp->mListener) {
    // Make sure |mListener| cannot call back after |mTracks| is collected
    tmp->mListener->Forget();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTracks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsumersToKeepAlive)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMMediaStream,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsumersToKeepAlive)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ISUPPORTS_INHERITED0(DOMLocalMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMMediaStream)
  NS_INTERFACE_MAP_ENTRY(DOMMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(DOMLocalMediaStream, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(DOMLocalMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN(DOMLocalMediaStream)
  NS_INTERFACE_MAP_ENTRY(DOMLocalMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream,
                                   mStreamNode)

NS_IMPL_ADDREF_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMAudioNodeMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

DOMMediaStream::DOMMediaStream()
  : mLogicalStreamStartTime(0),
    mStream(nullptr), mHintContents(0), mTrackTypesAvailable(0),
    mNotifiedOfMediaStreamGraphShutdown(false)
{
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
  return mStream->
    StreamTimeToSeconds(mStream->GetCurrentTime() - mLogicalStreamStartTime);
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

void
DOMMediaStream::GetTracks(nsTArray<nsRefPtr<MediaStreamTrack> >& aTracks)
{
  aTracks.AppendElements(mTracks);
}

bool
DOMMediaStream::HasTrack(const MediaStreamTrack& aTrack) const
{
  return mTracks.Contains(&aTrack);
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
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance(aHintContents);
  InitStreamCommon(gm->CreateSourceStream(this));
}

void
DOMMediaStream::InitTrackUnionStream(nsIDOMWindow* aWindow, TrackTypeHints aHintContents)
{
  mWindow = aWindow;
  SetHintContents(aHintContents);
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance(aHintContents);
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

void
DOMMediaStream::StopTrack(TrackID aTrackID)
{
  if (mStream && mStream->AsSourceStream()) {
    mStream->AsSourceStream()->EndTrack(aTrackID);
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

void
DOMMediaStream::SetHintContents(TrackTypeHints aHintContents)
{
  mHintContents = aHintContents;
  if (aHintContents & HINT_CONTENTS_AUDIO) {
    CreateDOMTrack(kAudioTrack, MediaSegment::AUDIO);
  }
  if (aHintContents & HINT_CONTENTS_VIDEO) {
    CreateDOMTrack(kVideoTrack, MediaSegment::VIDEO);
  }
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

  return track;
}

MediaStreamTrack*
DOMMediaStream::BindDOMTrack(TrackID aTrackID, MediaSegment::Type aType)
{
  MediaStreamTrack* track = nullptr;
  switch (aType) {
  case MediaSegment::AUDIO: {
    for (size_t i = 0; i < mTracks.Length(); ++i) {
      track = mTracks[i]->AsAudioStreamTrack();
      if (track) {
        track->BindTrackID(aTrackID);
        MOZ_ASSERT(mTrackTypesAvailable & HINT_CONTENTS_AUDIO);
        break;
      }
    }
    break;
  }
  case MediaSegment::VIDEO: {
    for (size_t i = 0; i < mTracks.Length(); ++i) {
      track = mTracks[i]->AsVideoStreamTrack();
      if (track) {
        track->BindTrackID(aTrackID);
        MOZ_ASSERT(mTrackTypesAvailable & HINT_CONTENTS_VIDEO);
        break;
      }
    }
    break;
  }
  default:
    MOZ_CRASH("Unhandled track type");
  }
  if (track) {
    CheckTracksAvailable();
  }
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

already_AddRefed<AudioTrack>
DOMMediaStream::CreateAudioTrack(AudioStreamTrack* aStreamTrack)
{
  nsAutoString id;
  nsAutoString label;
  aStreamTrack->GetId(id);
  aStreamTrack->GetLabel(label);

  return MediaTrackList::CreateAudioTrack(id, NS_LITERAL_STRING("main"),
                                          label, EmptyString(),
                                          aStreamTrack->Enabled());
}

already_AddRefed<VideoTrack>
DOMMediaStream::CreateVideoTrack(VideoStreamTrack* aStreamTrack)
{
  nsAutoString id;
  nsAutoString label;
  aStreamTrack->GetId(id);
  aStreamTrack->GetLabel(label);

  return MediaTrackList::CreateVideoTrack(id, NS_LITERAL_STRING("main"),
                                          label, EmptyString());
}

void
DOMMediaStream::ConstructMediaTracks(AudioTrackList* aAudioTrackList,
                                     VideoTrackList* aVideoTrackList)
{
  if (mHintContents & DOMMediaStream::HINT_CONTENTS_AUDIO) {
    MediaTrackListListener listener(aAudioTrackList);
    mMediaTrackListListeners.AppendElement(listener);
  }
  if (mHintContents & DOMMediaStream::HINT_CONTENTS_VIDEO) {
    MediaTrackListListener listener(aVideoTrackList);
    mMediaTrackListListeners.AppendElement(listener);
  }

  int firstEnabledVideo = -1;
  for (uint32_t i = 0; i < mTracks.Length(); ++i) {
    if (AudioStreamTrack* t = mTracks[i]->AsAudioStreamTrack()) {
      nsRefPtr<AudioTrack> track = CreateAudioTrack(t);
      aAudioTrackList->AddTrack(track);
    } else if (VideoStreamTrack* t = mTracks[i]->AsVideoStreamTrack()) {
      nsRefPtr<VideoTrack> track = CreateVideoTrack(t);
      aVideoTrackList->AddTrack(track);
      firstEnabledVideo = (t->Enabled() && firstEnabledVideo < 0)
                          ? (aVideoTrackList->Length() - 1)
                          : firstEnabledVideo;
    }
  }

  if (aVideoTrackList->Length() > 0) {
    // If media resource does not indicate a particular set of video tracks to
    // enable, the one that is listed first in the element's videoTracks object
    // must be selected.
    int index = firstEnabledVideo >= 0 ? firstEnabledVideo : 0;
    (*aVideoTrackList)[index]->SetEnabledInternal(true, MediaTrack::FIRE_NO_EVENTS);
  }
}

void
DOMMediaStream::DisconnectTrackListListeners(const AudioTrackList* aAudioTrackList,
                                             const VideoTrackList* aVideoTrackList)
{
  for (auto i = mMediaTrackListListeners.Length(); i > 0; ) { // unsigned!
    --i; // 0 ... Length()-1 range
    if (mMediaTrackListListeners[i].mMediaTrackList == aAudioTrackList ||
        mMediaTrackListListeners[i].mMediaTrackList == aVideoTrackList) {
      mMediaTrackListListeners.RemoveElementAt(i);
    }
  }
}

void
DOMMediaStream::NotifyMediaStreamTrackCreated(MediaStreamTrack* aTrack)
{
  for (uint32_t i = 0; i < mMediaTrackListListeners.Length(); ++i) {
    if (AudioStreamTrack* t = aTrack->AsAudioStreamTrack()) {
      nsRefPtr<AudioTrack> track = CreateAudioTrack(t);
      mMediaTrackListListeners[i].NotifyMediaTrackCreated(track);
    } else if (VideoStreamTrack* t = aTrack->AsVideoStreamTrack()) {
      nsRefPtr<VideoTrack> track = CreateVideoTrack(t);
      mMediaTrackListListeners[i].NotifyMediaTrackCreated(track);
    }
  }
}

void
DOMMediaStream::NotifyMediaStreamTrackEnded(MediaStreamTrack* aTrack)
{
  nsAutoString id;
  aTrack->GetId(id);
  for (uint32_t i = 0; i < mMediaTrackListListeners.Length(); ++i) {
    mMediaTrackListListeners[i].NotifyMediaTrackEnded(id);
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

DOMAudioNodeMediaStream::~DOMAudioNodeMediaStream()
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
