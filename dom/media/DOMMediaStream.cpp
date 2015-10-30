/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMediaStream.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIScriptError.h"
#include "nsIUUIDGenerator.h"
#include "nsPIDOMWindow.h"
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


DOMMediaStream::TrackPort::TrackPort(MediaInputPort* aInputPort,
                                     MediaStreamTrack* aTrack,
                                     const InputPortOwnership aOwnership)
  : mInputPort(aInputPort)
  , mTrack(aTrack)
  , mOwnership(aOwnership)
{
  // XXX Bug 1124630. nsDOMCameraControl requires adding a track without and
  // input port.
  // MOZ_ASSERT(mInputPort);
  MOZ_ASSERT(mTrack);

  MOZ_COUNT_CTOR(TrackPort);
}

DOMMediaStream::TrackPort::~TrackPort()
{
  MOZ_COUNT_DTOR(TrackPort);

  if (mOwnership == InputPortOwnership::OWNED && mInputPort) {
    mInputPort->Destroy();
    mInputPort = nullptr;
  }
}

void
DOMMediaStream::TrackPort::DestroyInputPort()
{
  if (mInputPort) {
    mInputPort->Destroy();
    mInputPort = nullptr;
  }
}

MediaStream*
DOMMediaStream::TrackPort::GetSource() const
{
  return mInputPort ? mInputPort->GetSource() : nullptr;
}

TrackID
DOMMediaStream::TrackPort::GetSourceTrackId() const
{
  return mInputPort ? mInputPort->GetSourceTrackId() : TRACK_INVALID;
}

void
DOMMediaStream::TrackPort::BlockTrackId(TrackID aTrackId)
{
  if (mInputPort) {
    mInputPort->BlockTrackId(aTrackId);
  }
}

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

    RefPtr<MediaStreamTrack> track =
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

    RefPtr<MediaStreamTrack> track =
      mStream->FindPlaybackDOMTrack(aInputStream, aInputTrackID);
    if (!track) {
      LOG(LogLevel::Debug, ("DOMMediaStream %p Not a playback track.", mStream));
      return;
    }

    LOG(LogLevel::Debug, ("DOMMediaStream %p Playback track; notifying stream listeners.",
                           mStream));
    mStream->NotifyTrackRemoved(track);

    RefPtr<TrackPort> endedPort = mStream->FindPlaybackTrackPort(*track);
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

/* static */ already_AddRefed<DOMMediaStream>
DOMMediaStream::Constructor(const GlobalObject& aGlobal,
                            ErrorResult& aRv)
{
  Sequence<OwningNonNull<MediaStreamTrack>> emptyTrackSeq;
  return Constructor(aGlobal, emptyTrackSeq, aRv);
}

/* static */ already_AddRefed<DOMMediaStream>
DOMMediaStream::Constructor(const GlobalObject& aGlobal,
                            const DOMMediaStream& aStream,
                            ErrorResult& aRv)
{
  nsTArray<RefPtr<MediaStreamTrack>> tracks;
  aStream.GetTracks(tracks);

  Sequence<OwningNonNull<MediaStreamTrack>> nonNullTrackSeq;
  if (!nonNullTrackSeq.SetLength(tracks.Length(), fallible)) {
    MOZ_ASSERT(false);
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  for (size_t i = 0; i < tracks.Length(); ++i) {
    nonNullTrackSeq[i] = tracks[i];
  }

  return Constructor(aGlobal, nonNullTrackSeq, aRv);
}

/* static */ already_AddRefed<DOMMediaStream>
DOMMediaStream::Constructor(const GlobalObject& aGlobal,
                            const Sequence<OwningNonNull<MediaStreamTrack>>& aTracks,
                            ErrorResult& aRv)
{
  nsCOMPtr<nsIDOMWindow> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMMediaStream> newStream = new DOMMediaStream();
  newStream->mWindow = ownerWindow;

  for (MediaStreamTrack& track : aTracks) {
    if (!newStream->GetPlaybackStream()) {
      MOZ_RELEASE_ASSERT(track.GetStream());
      MOZ_RELEASE_ASSERT(track.GetStream()->GetPlaybackStream());
      MOZ_RELEASE_ASSERT(track.GetStream()->GetPlaybackStream()->Graph());
      MediaStreamGraph* graph = track.GetStream()->GetPlaybackStream()->Graph();
      newStream->InitPlaybackStreamCommon(graph);
    }
    newStream->AddTrack(track);
  }

  if (!newStream->GetPlaybackStream()) {
    MOZ_ASSERT(aTracks.IsEmpty());
    MediaStreamGraph* graph =
      MediaStreamGraph::GetInstance(MediaStreamGraph::SYSTEM_THREAD_DRIVER,
                                    AudioChannel::Normal);
    newStream->InitPlaybackStreamCommon(graph);
  }

  return newStream.forget();
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
DOMMediaStream::GetAudioTracks(nsTArray<RefPtr<AudioStreamTrack> >& aTracks) const
{
  for (const RefPtr<TrackPort>& info : mTracks) {
    AudioStreamTrack* t = info->GetTrack()->AsAudioStreamTrack();
    if (t) {
      aTracks.AppendElement(t);
    }
  }
}

void
DOMMediaStream::GetVideoTracks(nsTArray<RefPtr<VideoStreamTrack> >& aTracks) const
{
  for (const RefPtr<TrackPort>& info : mTracks) {
    VideoStreamTrack* t = info->GetTrack()->AsVideoStreamTrack();
    if (t) {
      aTracks.AppendElement(t);
    }
  }
}

void
DOMMediaStream::GetTracks(nsTArray<RefPtr<MediaStreamTrack> >& aTracks) const
{
  for (const RefPtr<TrackPort>& info : mTracks) {
    aTracks.AppendElement(info->GetTrack());
  }
}

void
DOMMediaStream::AddTrack(MediaStreamTrack& aTrack)
{
  MOZ_RELEASE_ASSERT(mPlaybackStream);

  RefPtr<ProcessedMediaStream> dest = mPlaybackStream->AsProcessedStream();
  MOZ_ASSERT(dest);
  if (!dest) {
    return;
  }

  LOG(LogLevel::Info, ("DOMMediaStream %p Adding track %p (from stream %p with ID %d)",
                       this, &aTrack, aTrack.GetStream(), aTrack.GetTrackID()));

  if (mPlaybackStream->Graph() !=
      aTrack.GetStream()->mPlaybackStream->Graph()) {
    NS_ASSERTION(false, "Cannot combine tracks from different MediaStreamGraphs");
    LOG(LogLevel::Error, ("DOMMediaStream %p Own MSG %p != aTrack's MSG %p",
                         this, mPlaybackStream->Graph(),
                         aTrack.GetStream()->mPlaybackStream->Graph()));

    nsAutoString trackId;
    aTrack.GetId(trackId);
    const char16_t* params[] = { trackId.get() };
    nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(GetParentObject());
    nsIDocument* document = pWindow ? pWindow->GetExtantDoc() : nullptr;
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("Media"),
                                    document,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "MediaStreamAddTrackDifferentAudioChannel",
                                    params, ArrayLength(params));
    return;
  }

  if (HasTrack(aTrack)) {
    LOG(LogLevel::Debug, ("DOMMediaStream %p already contains track %p", this, &aTrack));
    return;
  }

  RefPtr<DOMMediaStream> addedDOMStream = aTrack.GetStream();
  MOZ_RELEASE_ASSERT(addedDOMStream);

  RefPtr<MediaStream> owningStream = addedDOMStream->GetOwnedStream();
  MOZ_RELEASE_ASSERT(owningStream);

  CombineWithPrincipal(addedDOMStream->mPrincipal);

  // Hook up the underlying track with our underlying playback stream.
  RefPtr<MediaInputPort> inputPort =
    GetPlaybackStream()->AllocateInputPort(owningStream, aTrack.GetTrackID());
  RefPtr<TrackPort> trackPort =
    new TrackPort(inputPort, &aTrack, TrackPort::InputPortOwnership::OWNED);
  mTracks.AppendElement(trackPort.forget());
  NotifyTrackAdded(&aTrack);

  LOG(LogLevel::Debug, ("DOMMediaStream %p Added track %p", this, &aTrack));
}

void
DOMMediaStream::RemoveTrack(MediaStreamTrack& aTrack)
{
  LOG(LogLevel::Info, ("DOMMediaStream %p Removing track %p (from stream %p with ID %d)",
                       this, &aTrack, aTrack.GetStream(), aTrack.GetTrackID()));

  RefPtr<TrackPort> toRemove = FindPlaybackTrackPort(aTrack);
  if (!toRemove) {
    LOG(LogLevel::Debug, ("DOMMediaStream %p does not contain track %p", this, &aTrack));
    return;
  }

  // If the track comes from a TRACK_ANY input port (i.e., mOwnedPort), we need
  // to block it in the port. Doing this for a locked track is still OK as it
  // will first block the track, then destroy the port. Both cause the track to
  // end.
  toRemove->BlockTrackId(aTrack.GetTrackID());

  DebugOnly<bool> removed = mTracks.RemoveElement(toRemove);
  MOZ_ASSERT(removed);
  LOG(LogLevel::Debug, ("DOMMediaStream %p Removed track %p", this, &aTrack));
}

bool
DOMMediaStream::HasTrack(const MediaStreamTrack& aTrack) const
{
  return !!FindPlaybackTrackPort(aTrack);
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
  InitInputStreamCommon(aGraph->CreateSourceStream(nullptr), aGraph);
  InitOwnedStreamCommon(aGraph);
  InitPlaybackStreamCommon(aGraph);
}

void
DOMMediaStream::InitTrackUnionStream(nsIDOMWindow* aWindow,
                                     MediaStreamGraph* aGraph)
{
  mWindow = aWindow;
  InitInputStreamCommon(aGraph->CreateTrackUnionStream(nullptr), aGraph);
  InitOwnedStreamCommon(aGraph);
  InitPlaybackStreamCommon(aGraph);
}

void
DOMMediaStream::InitAudioCaptureStream(nsIDOMWindow* aWindow,
                                       MediaStreamGraph* aGraph)
{
  mWindow = aWindow;

  const TrackID AUDIO_TRACK = 1;

  InitInputStreamCommon(aGraph->CreateAudioCaptureStream(this, AUDIO_TRACK), aGraph);
  InitOwnedStreamCommon(aGraph);
  InitPlaybackStreamCommon(aGraph);
  CreateOwnDOMTrack(AUDIO_TRACK, MediaSegment::AUDIO);
}

void
DOMMediaStream::InitInputStreamCommon(MediaStream* aStream,
                                      MediaStreamGraph* aGraph)
{
  MOZ_ASSERT(!mOwnedStream, "Input stream must be initialized before owned stream");

  mInputStream = aStream;
}

void
DOMMediaStream::InitOwnedStreamCommon(MediaStreamGraph* aGraph)
{
  MOZ_ASSERT(!mPlaybackStream, "Owned stream must be initialized before playback stream");

  // We pass null as the wrapper since it is only used to signal finished
  // streams. This is only needed for the playback stream.
  mOwnedStream = aGraph->CreateTrackUnionStream(nullptr);
  mOwnedStream->SetAutofinish(true);
  if (mInputStream) {
    mOwnedPort = mOwnedStream->AllocateInputPort(mInputStream);
  }

  // Setup track listeners
  mOwnedListener = new OwnedStreamListener(this);
  mOwnedStream->AddListener(mOwnedListener);
}

void
DOMMediaStream::InitPlaybackStreamCommon(MediaStreamGraph* aGraph)
{
  mPlaybackStream = aGraph->CreateTrackUnionStream(this);
  mPlaybackStream->SetAutofinish(true);
  if (mOwnedStream) {
    mPlaybackPort = mPlaybackStream->AllocateInputPort(mOwnedStream);
  }

  mPlaybackListener = new PlaybackStreamListener(this);
  mPlaybackStream->AddListener(mPlaybackListener);

  LOG(LogLevel::Debug, ("DOMMediaStream %p Initiated with mInputStream=%p, mOwnedStream=%p, mPlaybackStream=%p",
                        this, mInputStream, mOwnedStream, mPlaybackStream));
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateSourceStream(nsIDOMWindow* aWindow,
                                   MediaStreamGraph* aGraph)
{
  RefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitSourceStream(aWindow, aGraph);
  return stream.forget();
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateTrackUnionStream(nsIDOMWindow* aWindow,
                                       MediaStreamGraph* aGraph)
{
  RefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitTrackUnionStream(aWindow, aGraph);
  return stream.forget();
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateAudioCaptureStream(nsIDOMWindow* aWindow,
                                         MediaStreamGraph* aGraph)
{
  RefPtr<DOMMediaStream> stream = new DOMMediaStream();
  stream->InitAudioCaptureStream(aWindow, aGraph);
  return stream.forget();
}

void
DOMMediaStream::SetTrackEnabled(TrackID aTrackID, bool aEnabled)
{
  // XXX Bug 1208371 - This enables/disables the track across clones.
  if (mInputStream) {
    mInputStream->SetTrackEnabled(aTrackID, aEnabled);
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
  MOZ_RELEASE_ASSERT(mInputStream);
  MOZ_RELEASE_ASSERT(mOwnedStream);

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

  RefPtr<TrackPort> ownedTrackPort =
    new TrackPort(mOwnedPort, track, TrackPort::InputPortOwnership::EXTERNAL);
  mOwnedTracks.AppendElement(ownedTrackPort.forget());

  RefPtr<TrackPort> playbackTrackPort =
    new TrackPort(mPlaybackPort, track, TrackPort::InputPortOwnership::EXTERNAL);
  mTracks.AppendElement(playbackTrackPort.forget());

  NotifyTrackAdded(track);
  return track;
}

MediaStreamTrack*
DOMMediaStream::FindOwnedDOMTrack(MediaStream* aOwningStream, TrackID aTrackID) const
{
  MOZ_RELEASE_ASSERT(mOwnedStream);

  if (aOwningStream != mOwnedStream) {
    return nullptr;
  }

  for (const RefPtr<TrackPort>& info : mOwnedTracks) {
    if (info->GetTrack()->GetTrackID() == aTrackID) {
      return info->GetTrack();
    }
  }
  return nullptr;
}

MediaStreamTrack*
DOMMediaStream::FindPlaybackDOMTrack(MediaStream* aInputStream, TrackID aInputTrackID) const
{
  MOZ_RELEASE_ASSERT(mPlaybackStream);

  for (const RefPtr<TrackPort>& info : mTracks) {
    if (info->GetInputPort() == mPlaybackPort &&
        aInputStream == mOwnedStream &&
        aInputTrackID == info->GetTrack()->GetTrackID()) {
      // This track is in our owned and playback streams.
      return info->GetTrack();
    }
    if (info->GetInputPort() &&
        info->GetInputPort()->GetSource() == aInputStream &&
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

DOMMediaStream::TrackPort*
DOMMediaStream::FindPlaybackTrackPort(const MediaStreamTrack& aTrack) const
{
  for (const RefPtr<TrackPort>& info : mTracks) {
    if (info->GetTrack() == &aTrack) {
      return info;
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
  mTrackListeners.Clear();
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
DOMMediaStream::NotifyTracksCreated()
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
DOMMediaStream::RegisterTrackListener(TrackListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mNotifiedOfMediaStreamGraphShutdown) {
    // No more tracks will ever be added, so just do nothing.
    return;
  }
  mTrackListeners.AppendElement(aListener);
}

void
DOMMediaStream::UnregisterTrackListener(TrackListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  mTrackListeners.RemoveElement(aListener);
}

void
DOMMediaStream::NotifyTrackAdded(
    const RefPtr<MediaStreamTrack>& aTrack)
{
  MOZ_ASSERT(NS_IsMainThread());

  for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
    const RefPtr<TrackListener>& listener = mTrackListeners[i];
    listener->NotifyTrackAdded(aTrack);
  }
}

void
DOMMediaStream::NotifyTrackRemoved(
    const RefPtr<MediaStreamTrack>& aTrack)
{
  MOZ_ASSERT(NS_IsMainThread());

  for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
    const RefPtr<TrackListener>& listener = mTrackListeners[i];
    listener->NotifyTrackRemoved(aTrack);
  }
}

void
DOMMediaStream::CreateAndAddPlaybackStreamListener(MediaStream* aStream)
{
  MOZ_ASSERT(GetCameraStream(), "I'm a hack. Only DOMCameraControl may use me.");
  mPlaybackListener = new PlaybackStreamListener(this);
  aStream->AddListener(mPlaybackListener);
}

DOMLocalMediaStream::~DOMLocalMediaStream()
{
  if (mInputStream) {
    // Make sure Listeners of this stream know it's going away
    StopImpl();
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
  nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(GetParentObject());
  nsIDocument* document = pWindow ? pWindow->GetExtantDoc() : nullptr;
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Media"),
                                  document,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  "MediaStreamStopDeprecatedWarning");

  StopImpl();
}

void
DOMLocalMediaStream::StopImpl()
{
  if (mInputStream && mInputStream->AsSourceStream()) {
    mInputStream->AsSourceStream()->EndAllTrackAndFinish();
  }
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateSourceStream(nsIDOMWindow* aWindow,
                                        MediaStreamGraph* aGraph)
{
  RefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
  stream->InitSourceStream(aWindow, aGraph);
  return stream.forget();
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateTrackUnionStream(nsIDOMWindow* aWindow,
                                            MediaStreamGraph* aGraph)
{
  RefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
  stream->InitTrackUnionStream(aWindow, aGraph);
  return stream.forget();
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateAudioCaptureStream(nsIDOMWindow* aWindow,
                                              MediaStreamGraph* aGraph)
{
  RefPtr<DOMLocalMediaStream> stream = new DOMLocalMediaStream();
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
  RefPtr<DOMAudioNodeMediaStream> stream = new DOMAudioNodeMediaStream(aNode);
  stream->InitTrackUnionStream(aWindow, aGraph);
  return stream.forget();
}

DOMHwMediaStream::DOMHwMediaStream()
{
#ifdef MOZ_WIDGET_GONK
  mImageContainer = LayerManager::CreateImageContainer(ImageContainer::ASYNCHRONOUS_OVERLAY);
  RefPtr<Image> img = mImageContainer->CreateImage(ImageFormat::OVERLAY_IMAGE);
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
  RefPtr<DOMHwMediaStream> stream = new DOMHwMediaStream();

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

    RefPtr<Image> image = static_cast<Image*>(mOverlayImage.get());
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
  RefPtr<Image> image = static_cast<Image*>(mOverlayImage.get());
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
