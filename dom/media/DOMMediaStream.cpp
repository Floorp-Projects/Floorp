/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMediaStream.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIUUIDGenerator.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/LocalMediaStreamBinding.h"
#include "mozilla/dom/AudioNode.h"
#include "AudioChannelAgent.h"
#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/AudioTrackList.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/VideoTrackList.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "MediaStreamGraph.h"
#include "AudioStreamTrack.h"
#include "VideoStreamTrack.h"
#include "Layers.h"

#ifdef LOG
#undef LOG
#endif

static PRLogModuleInfo* gMediaStreamLog;
#define LOG(type, msg) MOZ_LOG(gMediaStreamLog, type, msg)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

const TrackID TRACK_VIDEO_PRIMARY = 1;

/**
 * TrackPort is a representation of a MediaStreamTrack-MediaInputPort pair
 * that make up a link between the Owned stream and the Playback stream.
 *
 * Semantically, the track is the identifier/key and the port the value of this
 * connection.
 *
 * The input port can be shared between several TrackPorts. This is the case
 * for DOMMediaStream's mPlaybackPort which forwards all tracks in its
 * mOwnedStream automatically.
 *
 * If the MediaStreamTrack is owned by another DOMMediaStream (called A) than
 * the one owning the TrackPort (called B), the input port (locked to the
 * MediaStreamTrack's TrackID) connects A's mOwnedStream to B's mPlaybackStream.
 *
 * A TrackPort may never leave the DOMMediaStream it was created in. Internal
 * use only.
 */
class DOMMediaStream::TrackPort
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TrackPort)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(TrackPort)

  enum class InputPortOwnership {
    OWNED = 1,
    EXTERNAL
  };

  TrackPort(MediaInputPort* aInputPort,
            MediaStreamTrack* aTrack,
            const InputPortOwnership aOwnership)
    : mInputPort(aInputPort)
    , mTrack(aTrack)
    , mOwnership(aOwnership)
  {
    MOZ_ASSERT(mInputPort);
    MOZ_ASSERT(mTrack);

    MOZ_COUNT_CTOR(TrackPort);
  }

protected:
  virtual ~TrackPort()
  {
    MOZ_COUNT_DTOR(TrackPort);

    if (mOwnership == InputPortOwnership::OWNED && mInputPort) {
      mInputPort->Destroy();
      mInputPort = nullptr;
    }
  }

public:
  void DestroyInputPort()
  {
    if (mInputPort) {
      mInputPort->Destroy();
      mInputPort = nullptr;
    }
  }

  /**
   * Returns the source stream of the input port.
   */
  MediaStream* GetSource() const { return mInputPort ? mInputPort->GetSource()
                                                     : nullptr; }

  /**
   * Returns the track ID this track is locked to in the source stream of the
   * input port.
   */
  TrackID GetSourceTrackId() const { return mInputPort ? mInputPort->GetSourceTrackId()
                                                       : TRACK_INVALID; }

  MediaInputPort* GetInputPort() const { return mInputPort; }
  MediaStreamTrack* GetTrack() const { return mTrack; }

private:
  nsRefPtr<MediaInputPort> mInputPort;
  nsRefPtr<MediaStreamTrack> mTrack;

  // Defines if we've been given ownership of the input port or if it's owned
  // externally. The owner is responsible for destroying the port.
  const InputPortOwnership mOwnership;
};

NS_IMPL_CYCLE_COLLECTION(DOMMediaStream::TrackPort, mTrack)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMMediaStream::TrackPort, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMMediaStream::TrackPort, Release)

/**
 * Listener registered on the Owned stream to detect added and ended owned
 * tracks for keeping the list of MediaStreamTracks in sync with the tracks
 * added and ended directly at the source.
 */
class DOMMediaStream::OwnedStreamListener : public MediaStreamListener {
public:
  explicit OwnedStreamListener(DOMMediaStream* aStream)
    : mStream(aStream)
  {}

  void Forget() { mStream = nullptr; }

  void DoNotifyTrackCreated(TrackID aTrackId, MediaSegment::Type aType)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mStream) {
      return;
    }

    MediaStreamTrack* track = mStream->FindOwnedDOMTrack(
      mStream->GetOwnedStream(), aTrackId);
    if (track) {
      // This track has already been manually created. Abort.
      return;
    }

    NS_WARN_IF_FALSE(!mStream->mTracks.IsEmpty(),
                     "A new track was detected on the input stream; creating a corresponding MediaStreamTrack. "
                     "Initial tracks should be added manually to immediately and synchronously be available to JS.");
    mStream->CreateOwnDOMTrack(aTrackId, aType);
  }

  void DoNotifyTrackEnded(TrackID aTrackId)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mStream) {
      return;
    }

    nsRefPtr<MediaStreamTrack> track =
      mStream->FindOwnedDOMTrack(mStream->GetOwnedStream(), aTrackId);
    NS_ASSERTION(track, "Owned MediaStreamTracks must be known by the DOMMediaStream");
    if (track) {
      LOG(LogLevel::Debug, ("DOMMediaStream %p MediaStreamTrack %p ended at the source. Marking it ended.",
                            mStream, track.get()));
      track->NotifyEnded();
    }
  }

  void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                StreamTime aTrackOffset, uint32_t aTrackEvents,
                                const MediaSegment& aQueuedMedia,
                                MediaStream* aInputStream,
                                TrackID aInputTrackID) override
  {
    if (aTrackEvents & TRACK_EVENT_CREATED) {
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethodWithArgs<TrackID, MediaSegment::Type>(
          this, &OwnedStreamListener::DoNotifyTrackCreated,
          aID, aQueuedMedia.GetType());
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(runnable.forget());
    } else if (aTrackEvents & TRACK_EVENT_ENDED) {
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethodWithArgs<TrackID>(
          this, &OwnedStreamListener::DoNotifyTrackEnded, aID);
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(runnable.forget());
    }
  }

private:
  // These fields may only be accessed on the main thread
  DOMMediaStream* mStream;
};

/**
 * Listener registered on the Playback stream to detect when tracks end and when
 * all new tracks this iteration have been created - for when several tracks are
 * queued by the source and committed all at once.
 */
class DOMMediaStream::PlaybackStreamListener : public MediaStreamListener {
public:
  explicit PlaybackStreamListener(DOMMediaStream* aStream)
    : mStream(aStream)
  {}

  void Forget()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mStream = nullptr;
  }

  void DoNotifyTrackEnded(MediaStream* aInputStream,
                          TrackID aInputTrackID)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mStream) {
      return;
    }

    LOG(LogLevel::Debug, ("DOMMediaStream %p Track %u of stream %p ended",
                          mStream, aInputTrackID, aInputStream));

    nsRefPtr<MediaStreamTrack> track =
      mStream->FindPlaybackDOMTrack(aInputStream, aInputTrackID);
    if (!track) {
      LOG(LogLevel::Debug, ("DOMMediaStream %p Not a playback track.", mStream));
      return;
    }

    LOG(LogLevel::Debug, ("DOMMediaStream %p Playback track; notifying stream listeners.",
                           mStream));
    mStream->NotifyTrackRemoved(track);

    nsRefPtr<TrackPort> endedPort = mStream->FindPlaybackTrackPort(*track);
    NS_ASSERTION(endedPort, "Playback track should have a TrackPort");
    if (endedPort &&
        endedPort->GetSourceTrackId() != TRACK_ANY &&
        endedPort->GetSourceTrackId() != TRACK_INVALID &&
        endedPort->GetSourceTrackId() != TRACK_NONE) {
      // If a track connected to a locked-track input port ends, we destroy the
      // port to allow our playback stream to finish.
      // XXX (bug 1208316) This should not be necessary when MediaStreams don't
      // finish but instead become inactive.
      endedPort->DestroyInputPort();
    }
  }

  void DoNotifyFinishedTrackCreation()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mStream) {
      return;
    }

    mStream->NotifyTracksCreated();
  }

  // The methods below are called on the MediaStreamGraph thread.

  void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                StreamTime aTrackOffset, uint32_t aTrackEvents,
                                const MediaSegment& aQueuedMedia,
                                MediaStream* aInputStream,
                                TrackID aInputTrackID) override
  {
    if (aTrackEvents & TRACK_EVENT_ENDED) {
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethodWithArgs<StorensRefPtrPassByPtr<MediaStream>, TrackID>(
          this, &PlaybackStreamListener::DoNotifyTrackEnded, aInputStream, aInputTrackID);
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(runnable.forget());
    }
  }

  void NotifyFinishedTrackCreation(MediaStreamGraph* aGraph) override
  {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(this, &PlaybackStreamListener::DoNotifyFinishedTrackCreation);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(runnable.forget());
  }

private:
  // These fields may only be accessed on the main thread
  DOMMediaStream* mStream;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMMediaStream,
                                                DOMEventTargetHelper)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwnedTracks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTracks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsumersToKeepAlive)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMMediaStream,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwnedTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsumersToKeepAlive)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(DOMMediaStream, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DOMMediaStream, DOMEventTargetHelper)

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
  : mLogicalStreamStartTime(0), mInputStream(nullptr), mOwnedStream(nullptr),
    mPlaybackStream(nullptr), mOwnedPort(nullptr), mPlaybackPort(nullptr),
    mTracksCreated(false), mNotifiedOfMediaStreamGraphShutdown(false),
    mCORSMode(CORS_NONE)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);

  if (!gMediaStreamLog) {
    gMediaStreamLog = PR_NewLogModule("MediaStream");
  }

  if (NS_SUCCEEDED(rv) && uuidgen) {
    nsID uuid;
    memset(&uuid, 0, sizeof(uuid));
    rv = uuidgen->GenerateUUIDInPlace(&uuid);
    if (NS_SUCCEEDED(rv)) {
      char buffer[NSID_LENGTH];
      uuid.ToProvidedString(buffer);
      mID = NS_ConvertASCIItoUTF16(buffer);
    }
  }
}

DOMMediaStream::~DOMMediaStream()
{
  Destroy();
}

void
DOMMediaStream::Destroy()
{
  LOG(LogLevel::Debug, ("DOMMediaStream %p Being destroyed.", this));
  if (mOwnedListener) {
    mOwnedListener->Forget();
    mOwnedListener = nullptr;
  }
  if (mPlaybackListener) {
    mPlaybackListener->Forget();
    mPlaybackListener = nullptr;
  }
  if (mPlaybackPort) {
    mPlaybackPort->Destroy();
    mPlaybackPort = nullptr;
  }
  if (mOwnedPort) {
    mOwnedPort->Destroy();
    mOwnedPort = nullptr;
  }
  if (mPlaybackStream) {
    mPlaybackStream->Destroy();
    mPlaybackStream = nullptr;
  }
  if (mOwnedStream) {
    mOwnedStream->Destroy();
    mOwnedStream = nullptr;
  }
  if (mInputStream) {
    mInputStream->Destroy();
    mInputStream = nullptr;
  }
}

JSObject*
DOMMediaStream::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::MediaStreamBinding::Wrap(aCx, this, aGivenProto);
}

double
DOMMediaStream::CurrentTime()
{
  if (!mPlaybackStream) {
    return 0.0;
  }
  return mPlaybackStream->
    StreamTimeToSeconds(mPlaybackStream->GetCurrentTime() - mLogicalStreamStartTime);
}

void
DOMMediaStream::GetId(nsAString& aID) const
{
  aID = mID;
}

void
DOMMediaStream::GetAudioTracks(nsTArray<nsRefPtr<AudioStreamTrack> >& aTracks)
{
  for (const nsRefPtr<TrackPort>& info : mTracks) {
    AudioStreamTrack* t = info->GetTrack()->AsAudioStreamTrack();
    if (t) {
      aTracks.AppendElement(t);
    }
  }
}

void
DOMMediaStream::GetVideoTracks(nsTArray<nsRefPtr<VideoStreamTrack> >& aTracks)
{
  for (const nsRefPtr<TrackPort>& info : mTracks) {
    VideoStreamTrack* t = info->GetTrack()->AsVideoStreamTrack();
    if (t) {
      aTracks.AppendElement(t);
    }
  }
}

void
DOMMediaStream::GetTracks(nsTArray<nsRefPtr<MediaStreamTrack> >& aTracks)
{
  for (const nsRefPtr<TrackPort>& info : mTracks) {
    aTracks.AppendElement(info->GetTrack());
  }
}

bool
DOMMediaStream::HasTrack(const MediaStreamTrack& aTrack) const
{
  return !!FindPlaybackDOMTrack(aTrack.GetStream()->GetOwnedStream(), aTrack.GetTrackID());
}

bool
DOMMediaStream::OwnsTrack(const MediaStreamTrack& aTrack) const
{
  return (aTrack.GetStream() == this) && HasTrack(aTrack);
}

bool
DOMMediaStream::IsFinished()
{
  return !mPlaybackStream || mPlaybackStream->IsFinished();
}

void
DOMMediaStream::InitSourceStream(nsIDOMWindow* aWindow,
                                 MediaStreamGraph* aGraph)
{
  mWindow = aWindow;
  InitStreamCommon(aGraph->CreateSourceStream(nullptr), aGraph);
}

void
DOMMediaStream::InitTrackUnionStream(nsIDOMWindow* aWindow,
                                     MediaStreamGraph* aGraph)
{
  mWindow = aWindow;
  InitStreamCommon(aGraph->CreateTrackUnionStream(nullptr), aGraph);
}

void
DOMMediaStream::InitAudioCaptureStream(nsIDOMWindow* aWindow,
                                       MediaStreamGraph* aGraph)
{
  mWindow = aWindow;

  const TrackID AUDIO_TRACK = 1;

  InitStreamCommon(aGraph->CreateAudioCaptureStream(this, AUDIO_TRACK), aGraph);
  CreateOwnDOMTrack(AUDIO_TRACK, MediaSegment::AUDIO);
}

void
DOMMediaStream::InitStreamCommon(MediaStream* aStream,
                                 MediaStreamGraph* aGraph)
{
  mInputStream = aStream;

  // We pass null as the wrapper since it is only used to signal finished
  // streams. This is only needed for the playback stream.
  mOwnedStream = aGraph->CreateTrackUnionStream(nullptr);
  mOwnedStream->SetAutofinish(true);
  mOwnedPort = mOwnedStream->AllocateInputPort(mInputStream);

  mPlaybackStream = aGraph->CreateTrackUnionStream(this);
  mPlaybackStream->SetAutofinish(true);
  mPlaybackPort = mPlaybackStream->AllocateInputPort(mOwnedStream);

  LOG(LogLevel::Debug, ("DOMMediaStream %p Initiated with mInputStream=%p, mOwnedStream=%p, mPlaybackStream=%p",
                        this, mInputStream, mOwnedStream, mPlaybackStream));

  // Setup track listeners
  mOwnedListener = new OwnedStreamListener(this);
  mOwnedStream->AddListener(mOwnedListener);
  mPlaybackListener = new PlaybackStreamListener(this);
  mPlaybackStream->AddListener(mPlaybackListener);
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateSourceStream(nsIDOMWindow* aWindow,
                                   MediaStreamGraph* aGraph)
{
  nsRefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitSourceStream(aWindow, aGraph);
  return stream.forget();
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateTrackUnionStream(nsIDOMWindow* aWindow,
                                       MediaStreamGraph* aGraph)
{
  nsRefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitTrackUnionStream(aWindow, aGraph);
  return stream.forget();
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateAudioCaptureStream(nsIDOMWindow* aWindow,
                                         MediaStreamGraph* aGraph)
{
  nsRefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitAudioCaptureStream(aWindow, aGraph);
  return stream.forget();
}

void
DOMMediaStream::SetTrackEnabled(TrackID aTrackID, bool aEnabled)
{
  if (mOwnedStream) {
    mOwnedStream->SetTrackEnabled(aTrackID, aEnabled);
  }
}

void
DOMMediaStream::StopTrack(TrackID aTrackID)
{
  if (mInputStream && mInputStream->AsSourceStream()) {
    mInputStream->AsSourceStream()->EndTrack(aTrackID);
  }
}

already_AddRefed<Promise>
DOMMediaStream::ApplyConstraintsToTrack(TrackID aTrackID,
                                        const MediaTrackConstraints& aConstraints,
                                        ErrorResult &aRv)
{
  return nullptr;
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
DOMMediaStream::SetCORSMode(CORSMode aCORSMode)
{
  MOZ_ASSERT(NS_IsMainThread());
  mCORSMode = aCORSMode;
}

CORSMode
DOMMediaStream::GetCORSMode()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mCORSMode;
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
DOMMediaStream::CreateOwnDOMTrack(TrackID aTrackID, MediaSegment::Type aType)
{
  MOZ_ASSERT(FindOwnedDOMTrack(GetOwnedStream(), aTrackID) == nullptr);

  MediaStreamTrack* track;
  switch (aType) {
  case MediaSegment::AUDIO:
    track = new AudioStreamTrack(this, aTrackID);
    break;
  case MediaSegment::VIDEO:
    track = new VideoStreamTrack(this, aTrackID);
    break;
  default:
    MOZ_CRASH("Unhandled track type");
  }

  LOG(LogLevel::Debug, ("DOMMediaStream %p Created new track %p with ID %u", this, track, aTrackID));

  nsRefPtr<TrackPort> ownedTrackPort =
    new TrackPort(mOwnedPort, track, TrackPort::InputPortOwnership::EXTERNAL);
  mOwnedTracks.AppendElement(ownedTrackPort.forget());

  nsRefPtr<TrackPort> playbackTrackPort =
    new TrackPort(mPlaybackPort, track, TrackPort::InputPortOwnership::EXTERNAL);
  mTracks.AppendElement(playbackTrackPort.forget());

  NotifyMediaStreamTrackCreated(track);
  return track;
}

MediaStreamTrack*
DOMMediaStream::FindOwnedDOMTrack(MediaStream* aOwningStream, TrackID aTrackID) const
{
  if (aOwningStream != mOwnedStream) {
    return nullptr;
  }

  for (const nsRefPtr<TrackPort>& info : mOwnedTracks) {
    if (info->GetTrack()->GetTrackID() == aTrackID) {
      return info->GetTrack();
    }
  }
  return nullptr;
}

MediaStreamTrack*
DOMMediaStream::FindPlaybackDOMTrack(MediaStream* aInputStream, TrackID aInputTrackID) const
{
  for (const nsRefPtr<TrackPort>& info : mTracks) {
    if (info->GetInputPort() == mPlaybackPort &&
        aInputStream == mOwnedStream &&
        aInputTrackID == info->GetTrack()->GetTrackID()) {
      // This track is in our owned and playback streams.
      return info->GetTrack();
    }
    if (info->GetInputPort()->GetSource() == aInputStream &&
        info->GetSourceTrackId() == aInputTrackID) {
      // This track is owned externally but in our playback stream.
      MOZ_ASSERT(aInputTrackID != TRACK_NONE);
      MOZ_ASSERT(aInputTrackID != TRACK_INVALID);
      MOZ_ASSERT(aInputTrackID != TRACK_ANY);
      return info->GetTrack();
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
DOMMediaStream::NotifyStreamFinished()
{
  MOZ_ASSERT(IsFinished());
  mConsumersToKeepAlive.Clear();
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
DOMMediaStream::TracksCreated()
{
  mTracksCreated = true;
  CheckTracksAvailable();
}

void
DOMMediaStream::CheckTracksAvailable()
{
  if (!mTracksCreated) {
    return;
  }
  nsTArray<nsAutoPtr<OnTracksAvailableCallback> > callbacks;
  callbacks.SwapElements(mRunOnTracksAvailable);

  for (uint32_t i = 0; i < callbacks.Length(); ++i) {
    callbacks[i]->NotifyTracksAvailable(this);
  }
}

void
DOMMediaStream::CreateAndAddPlaybackStreamListener(MediaStream* aStream)
{
  MOZ_ASSERT(GetCameraStream(), "I'm a hack. Only DOMCameraControl may use me.");
  mPlaybackListener = new PlaybackStreamListener(this);
  aStream->AddListener(mPlaybackListener);
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
  MediaTrackListListener audioListener(aAudioTrackList);
  mMediaTrackListListeners.AppendElement(audioListener);
  MediaTrackListListener videoListener(aVideoTrackList);
  mMediaTrackListListeners.AppendElement(videoListener);

  int firstEnabledVideo = -1;
  for (const nsRefPtr<TrackPort>& info : mTracks) {
    if (AudioStreamTrack* t = info->GetTrack()->AsAudioStreamTrack()) {
      nsRefPtr<AudioTrack> track = CreateAudioTrack(t);
      aAudioTrackList->AddTrack(track);
    } else if (VideoStreamTrack* t = info->GetTrack()->AsVideoStreamTrack()) {
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
  MOZ_ASSERT(aTrack);

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
  MOZ_ASSERT(aTrack);

  nsAutoString id;
  aTrack->GetId(id);
  for (uint32_t i = 0; i < mMediaTrackListListeners.Length(); ++i) {
    mMediaTrackListListeners[i].NotifyMediaTrackEnded(id);
  }
}

DOMLocalMediaStream::~DOMLocalMediaStream()
{
  if (mInputStream) {
    // Make sure Listeners of this stream know it's going away
    Stop();
  }
}

JSObject*
DOMLocalMediaStream::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::LocalMediaStreamBinding::Wrap(aCx, this, aGivenProto);
}

void
DOMLocalMediaStream::Stop()
{
  if (mInputStream && mInputStream->AsSourceStream()) {
    mInputStream->AsSourceStream()->EndAllTrackAndFinish();
  }
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateSourceStream(nsIDOMWindow* aWindow,
                                        MediaStreamGraph* aGraph)
{
  nsRefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
  stream->InitSourceStream(aWindow, aGraph);
  return stream.forget();
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateTrackUnionStream(nsIDOMWindow* aWindow,
                                            MediaStreamGraph* aGraph)
{
  nsRefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
  stream->InitTrackUnionStream(aWindow, aGraph);
  return stream.forget();
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateAudioCaptureStream(nsIDOMWindow* aWindow,
                                              MediaStreamGraph* aGraph)
{
  nsRefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
  stream->InitAudioCaptureStream(aWindow, aGraph);
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
                                                MediaStreamGraph* aGraph)
{
  nsRefPtr<DOMAudioNodeMediaStream> stream = new DOMAudioNodeMediaStream(aNode);
  stream->InitTrackUnionStream(aWindow, aGraph);
  return stream.forget();
}

DOMHwMediaStream::DOMHwMediaStream()
{
#ifdef MOZ_WIDGET_GONK
  mImageContainer = LayerManager::CreateImageContainer(ImageContainer::ASYNCHRONOUS_OVERLAY);
  nsRefPtr<Image> img = mImageContainer->CreateImage(ImageFormat::OVERLAY_IMAGE);
  mOverlayImage = static_cast<layers::OverlayImage*>(img.get());
  nsAutoTArray<ImageContainer::NonOwningImage,1> images;
  images.AppendElement(ImageContainer::NonOwningImage(img));
  mImageContainer->SetCurrentImages(images);
#endif
}

DOMHwMediaStream::~DOMHwMediaStream()
{
}

already_AddRefed<DOMHwMediaStream>
DOMHwMediaStream::CreateHwStream(nsIDOMWindow* aWindow)
{
  nsRefPtr<DOMHwMediaStream> stream = new DOMHwMediaStream();

  MediaStreamGraph* graph =
    MediaStreamGraph::GetInstance(MediaStreamGraph::SYSTEM_THREAD_DRIVER,
                                  AudioChannel::Normal);
  stream->InitSourceStream(aWindow, graph);
  stream->Init(stream->GetInputStream());

  return stream.forget();
}

void
DOMHwMediaStream::Init(MediaStream* stream)
{
  SourceMediaStream* srcStream = stream->AsSourceStream();

  if (srcStream) {
    VideoSegment segment;
#ifdef MOZ_WIDGET_GONK
    const StreamTime delta = STREAM_TIME_MAX; // Because MediaStreamGraph will run out frames in non-autoplay mode,
                                              // we must give it bigger frame length to cover this situation.
    mImageData.mOverlayId = DEFAULT_IMAGE_ID;
    mImageData.mSize.width = DEFAULT_IMAGE_WIDTH;
    mImageData.mSize.height = DEFAULT_IMAGE_HEIGHT;
    mOverlayImage->SetData(mImageData);

    nsRefPtr<Image> image = static_cast<Image*>(mOverlayImage.get());
    mozilla::gfx::IntSize size = image->GetSize();

    segment.AppendFrame(image.forget(), delta, size);
#endif
    srcStream->AddTrack(TRACK_VIDEO_PRIMARY, 0, new VideoSegment());
    srcStream->AppendToTrack(TRACK_VIDEO_PRIMARY, &segment);
    srcStream->FinishAddTracks();
    srcStream->AdvanceKnownTracksTime(STREAM_TIME_MAX);
  }
}

int32_t
DOMHwMediaStream::RequestOverlayId()
{
#ifdef MOZ_WIDGET_GONK
  return mOverlayImage->GetOverlayId();
#else
  return -1;
#endif
}

void
DOMHwMediaStream::SetImageSize(uint32_t width, uint32_t height)
{
#ifdef MOZ_WIDGET_GONK
  OverlayImage::Data imgData;

  imgData.mOverlayId = mOverlayImage->GetOverlayId();
  imgData.mSize = IntSize(width, height);
  mOverlayImage->SetData(imgData);
#endif

  SourceMediaStream* srcStream = GetInputStream()->AsSourceStream();
  StreamBuffer::Track* track = srcStream->FindTrack(TRACK_VIDEO_PRIMARY);

  if (!track || !track->GetSegment()) {
    return;
  }

#ifdef MOZ_WIDGET_GONK
  // Clear the old segment.
  // Changing the existing content of segment is a Very BAD thing, and this way will
  // confuse consumers of MediaStreams.
  // It is only acceptable for DOMHwMediaStream
  // because DOMHwMediaStream doesn't have consumers of TV streams currently.
  track->GetSegment()->Clear();

  // Change the image size.
  const StreamTime delta = STREAM_TIME_MAX;
  nsRefPtr<Image> image = static_cast<Image*>(mOverlayImage.get());
  mozilla::gfx::IntSize size = image->GetSize();
  VideoSegment segment;

  segment.AppendFrame(image.forget(), delta, size);
  srcStream->AppendToTrack(TRACK_VIDEO_PRIMARY, &segment);
#endif
}
void
DOMHwMediaStream::SetOverlayId(int32_t aOverlayId)
{
#ifdef MOZ_WIDGET_GONK
  OverlayImage::Data imgData;

  imgData.mOverlayId = aOverlayId;
  imgData.mSize = mOverlayImage->GetSize();

  mOverlayImage->SetData(imgData);
#endif
}
