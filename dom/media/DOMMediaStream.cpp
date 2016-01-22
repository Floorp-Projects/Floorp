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

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with NS_DECL_NSIDOMMEDIASTREAM, containing
// currentTime getter.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#ifdef LOG
#undef LOG
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

static LazyLogModule gMediaStreamLog("MediaStream");
#define LOG(type, msg) MOZ_LOG(gMediaStreamLog, type, msg)

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

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaStreamTrackSourceGetter)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaStreamTrackSourceGetter)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamTrackSourceGetter)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION_0(MediaStreamTrackSourceGetter)

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

  void DoNotifyTrackCreated(TrackID aTrackID, MediaSegment::Type aType,
                            MediaStream* aInputStream, TrackID aInputTrackID)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mStream) {
      return;
    }

    MediaStreamTrack* track =
      mStream->FindOwnedDOMTrack(aInputStream, aInputTrackID);
    if (!track) {
      // Track had not been created on main thread before, create it now.
      NS_WARN_IF_FALSE(!mStream->mTracks.IsEmpty(),
                       "A new track was detected on the input stream; creating "
                       "a corresponding MediaStreamTrack. Initial tracks "
                       "should be added manually to immediately and "
                       "synchronously be available to JS.");
      RefPtr<MediaStreamTrackSource> source;
      if (mStream->mTrackSourceGetter) {
        source = mStream->mTrackSourceGetter->GetMediaStreamTrackSource(aTrackID);
      }
      if (!source) {
        NS_ASSERTION(false, "Dynamic track created without an explicit TrackSource");
        nsPIDOMWindowInner* window = mStream->GetParentObject();
        nsIDocument* doc = window ? window->GetExtantDoc() : nullptr;
        nsIPrincipal* principal = doc ? doc->NodePrincipal() : nullptr;
        source = new BasicUnstoppableTrackSource(principal);
      }
      track = mStream->CreateOwnDOMTrack(aTrackID, aType, nsString(), source);
    }
  }

  void DoNotifyTrackEnded(MediaStream* aInputStream, TrackID aInputTrackID)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mStream) {
      return;
    }

    RefPtr<MediaStreamTrack> track =
      mStream->FindOwnedDOMTrack(aInputStream, aInputTrackID);
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
        NS_NewRunnableMethodWithArgs<TrackID, MediaSegment::Type, MediaStream*, TrackID>(
          this, &OwnedStreamListener::DoNotifyTrackCreated,
          aID, aQueuedMedia.GetType(), aInputStream, aInputTrackID);
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(runnable.forget());
    } else if (aTrackEvents & TRACK_EVENT_ENDED) {
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethodWithArgs<MediaStream*, TrackID>(
          this, &OwnedStreamListener::DoNotifyTrackEnded,
          aInputStream, aInputTrackID);
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
    if (endedPort && IsTrackIDExplicit(endedPort->GetSourceTrackId())) {
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTrackSourceGetter)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMMediaStream,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwnedTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsumersToKeepAlive)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTrackSourceGetter)
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

DOMMediaStream::DOMMediaStream(nsPIDOMWindowInner* aWindow,
                               MediaStreamTrackSourceGetter* aTrackSourceGetter)
  : mLogicalStreamStartTime(0), mWindow(aWindow),
    mInputStream(nullptr), mOwnedStream(nullptr), mPlaybackStream(nullptr),
    mOwnedPort(nullptr), mPlaybackPort(nullptr),
    mTrackSourceGetter(aTrackSourceGetter), mTracksCreated(false),
    mNotifiedOfMediaStreamGraphShutdown(false), mCORSMode(CORS_NONE)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);

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
  for (const RefPtr<TrackPort>& info : mTracks) {
    // We must remove ourselves from each track's principal change observer list
    // before we die. CC may have cleared info->mTrack so guard against it.
    if (info->GetTrack()) {
      info->GetTrack()->RemovePrincipalChangeObserver(this);
    }
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
  nsCOMPtr<nsPIDOMWindowInner> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Streams created from JS cannot have dynamically created tracks.
  MediaStreamTrackSourceGetter* getter = nullptr;
  RefPtr<DOMMediaStream> newStream = new DOMMediaStream(ownerWindow, getter);

  for (MediaStreamTrack& track : aTracks) {
    if (!newStream->GetPlaybackStream()) {
      MOZ_RELEASE_ASSERT(track.Graph());
      newStream->InitPlaybackStreamCommon(track.Graph());
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
                       this, &aTrack, aTrack.mOwningStream.get(), aTrack.mTrackID));

  if (mPlaybackStream->Graph() != aTrack.Graph()) {
    NS_ASSERTION(false, "Cannot combine tracks from different MediaStreamGraphs");
    LOG(LogLevel::Error, ("DOMMediaStream %p Own MSG %p != aTrack's MSG %p",
                         this, mPlaybackStream->Graph(), aTrack.Graph()));

    nsAutoString trackId;
    aTrack.GetId(trackId);
    const char16_t* params[] = { trackId.get() };
    nsCOMPtr<nsPIDOMWindowInner> pWindow = GetParentObject();
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


  // Hook up the underlying track with our underlying playback stream.
  RefPtr<MediaInputPort> inputPort =
    GetPlaybackStream()->AllocateInputPort(aTrack.GetOwnedStream(),
                                           aTrack.mTrackID);
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
                       this, &aTrack, aTrack.mOwningStream.get(), aTrack.mTrackID));

  RefPtr<TrackPort> toRemove = FindPlaybackTrackPort(aTrack);
  if (!toRemove) {
    LOG(LogLevel::Debug, ("DOMMediaStream %p does not contain track %p", this, &aTrack));
    return;
  }

  // If the track comes from a TRACK_ANY input port (i.e., mOwnedPort), we need
  // to block it in the port. Doing this for a locked track is still OK as it
  // will first block the track, then destroy the port. Both cause the track to
  // end.
  toRemove->BlockTrackId(aTrack.mTrackID);

  DebugOnly<bool> removed = mTracks.RemoveElement(toRemove);
  MOZ_ASSERT(removed);

  NotifyTrackRemoved(&aTrack);

  LOG(LogLevel::Debug, ("DOMMediaStream %p Removed track %p", this, &aTrack));
}

MediaStreamTrack*
DOMMediaStream::GetTrackById(const nsString& aId)
{
  for (const RefPtr<TrackPort>& info : mTracks) {
    nsString id;
    info->GetTrack()->GetId(id);
    if (id == aId) {
      return info->GetTrack();
    }
  }
  return nullptr;
}

MediaStreamTrack*
DOMMediaStream::GetOwnedTrackById(const nsString& aId)
{
  for (const RefPtr<TrackPort>& info : mOwnedTracks) {
    nsString id;
    info->GetTrack()->GetId(id);
    if (id == aId) {
      return info->GetTrack();
    }
  }
  return nullptr;
}

bool
DOMMediaStream::HasTrack(const MediaStreamTrack& aTrack) const
{
  return !!FindPlaybackTrackPort(aTrack);
}

bool
DOMMediaStream::OwnsTrack(const MediaStreamTrack& aTrack) const
{
  return !!FindOwnedTrackPort(aTrack);
}

bool
DOMMediaStream::AddDirectListener(MediaStreamDirectListener* aListener)
{
  if (GetInputStream() && GetInputStream()->AsSourceStream()) {
    GetInputStream()->AsSourceStream()->AddDirectListener(aListener);
    return true; // application should ignore NotifyQueuedTrackData
  }
  return false;
}

void
DOMMediaStream::RemoveDirectListener(MediaStreamDirectListener* aListener)
{
  if (GetInputStream() && GetInputStream()->AsSourceStream()) {
    GetInputStream()->AsSourceStream()->RemoveDirectListener(aListener);
  }
}

bool
DOMMediaStream::IsFinished()
{
  return !mPlaybackStream || mPlaybackStream->IsFinished();
}

void
DOMMediaStream::InitSourceStream(MediaStreamGraph* aGraph)
{
  InitInputStreamCommon(aGraph->CreateSourceStream(nullptr), aGraph);
  InitOwnedStreamCommon(aGraph);
  InitPlaybackStreamCommon(aGraph);
}

void
DOMMediaStream::InitTrackUnionStream(MediaStreamGraph* aGraph)
{
  InitInputStreamCommon(aGraph->CreateTrackUnionStream(nullptr), aGraph);
  InitOwnedStreamCommon(aGraph);
  InitPlaybackStreamCommon(aGraph);
}

void
DOMMediaStream::InitAudioCaptureStream(nsIPrincipal* aPrincipal, MediaStreamGraph* aGraph)
{
  const TrackID AUDIO_TRACK = 1;

  RefPtr<BasicUnstoppableTrackSource> audioCaptureSource =
    new BasicUnstoppableTrackSource(aPrincipal, MediaSourceEnum::AudioCapture);

  AudioCaptureStream* audioCaptureStream =
    static_cast<AudioCaptureStream*>(aGraph->CreateAudioCaptureStream(this, AUDIO_TRACK));
  InitInputStreamCommon(audioCaptureStream, aGraph);
  InitOwnedStreamCommon(aGraph);
  InitPlaybackStreamCommon(aGraph);
  CreateOwnDOMTrack(AUDIO_TRACK, MediaSegment::AUDIO, nsString(), audioCaptureSource);
  audioCaptureStream->Start();
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
DOMMediaStream::CreateSourceStream(nsPIDOMWindowInner* aWindow,
                                   MediaStreamGraph* aGraph,
                                   MediaStreamTrackSourceGetter* aTrackSourceGetter)
{
  RefPtr<DOMMediaStream> stream = new DOMMediaStream(aWindow, aTrackSourceGetter);
  stream->InitSourceStream(aGraph);
  return stream.forget();
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateTrackUnionStream(nsPIDOMWindowInner* aWindow,
                                       MediaStreamGraph* aGraph,
                                       MediaStreamTrackSourceGetter* aTrackSourceGetter)
{
  RefPtr<DOMMediaStream> stream = new DOMMediaStream(aWindow, aTrackSourceGetter);
  stream->InitTrackUnionStream(aGraph);
  return stream.forget();
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateAudioCaptureStream(nsPIDOMWindowInner* aWindow,
                                         nsIPrincipal* aPrincipal,
                                         MediaStreamGraph* aGraph)
{
  // Audio capture doesn't create tracks dynamically
  MediaStreamTrackSourceGetter* getter = nullptr;
  RefPtr<DOMMediaStream> stream = new DOMMediaStream(aWindow, getter);
  stream->InitAudioCaptureStream(aPrincipal, aGraph);
  return stream.forget();
}

void
DOMMediaStream::PrincipalChanged(MediaStreamTrack* aTrack)
{
  MOZ_ASSERT(aTrack);
  NS_ASSERTION(HasTrack(*aTrack), "Principal changed for an unknown track");
  LOG(LogLevel::Info, ("DOMMediaStream %p Principal changed for track %p",
                       this, aTrack));
  RecomputePrincipal();
}

void
DOMMediaStream::RecomputePrincipal()
{
  nsCOMPtr<nsIPrincipal> previousPrincipal = mPrincipal.forget();
  for (const RefPtr<TrackPort>& info : mTracks) {
    nsContentUtils::CombineResourcePrincipals(&mPrincipal,
                                              info->GetTrack()->GetPrincipal());
  }
  if (previousPrincipal != mPrincipal) {
    NotifyPrincipalChanged();
  }
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
  if (!mPrincipal) {
    // When all tracks are removed, mPrincipal will change to nullptr.
    LOG(LogLevel::Info, ("DOMMediaStream %p Principal changed to nothing.",
                         this));
  } else {
    LOG(LogLevel::Info, ("DOMMediaStream %p Principal changed. Now: "
                         "null=%d, codebase=%d, expanded=%d, system=%d", this,
                          mPrincipal->GetIsNullPrincipal(),
                          mPrincipal->GetIsCodebasePrincipal(),
                          mPrincipal->GetIsExpandedPrincipal(),
                          mPrincipal->GetIsSystemPrincipal()));
  }

  for (uint32_t i = 0; i < mPrincipalChangeObservers.Length(); ++i) {
    mPrincipalChangeObservers[i]->PrincipalChanged(this);
  }
}


bool
DOMMediaStream::AddPrincipalChangeObserver(
  PrincipalChangeObserver<DOMMediaStream>* aObserver)
{
  return mPrincipalChangeObservers.AppendElement(aObserver) != nullptr;
}

bool
DOMMediaStream::RemovePrincipalChangeObserver(
  PrincipalChangeObserver<DOMMediaStream>* aObserver)
{
  return mPrincipalChangeObservers.RemoveElement(aObserver);
}

MediaStreamTrack*
DOMMediaStream::CreateOwnDOMTrack(TrackID aTrackID, MediaSegment::Type aType,
                                  const nsString& aLabel,
                                  MediaStreamTrackSource* aSource)
{
  MOZ_RELEASE_ASSERT(mInputStream);
  MOZ_RELEASE_ASSERT(mOwnedStream);

  MOZ_ASSERT(FindOwnedDOMTrack(GetInputStream(), aTrackID) == nullptr);

  MediaStreamTrack* track;
  switch (aType) {
  case MediaSegment::AUDIO:
    track = new AudioStreamTrack(this, aTrackID, aTrackID, aLabel, aSource);
    break;
  case MediaSegment::VIDEO:
    track = new VideoStreamTrack(this, aTrackID, aTrackID, aLabel, aSource);
    break;
  default:
    MOZ_CRASH("Unhandled track type");
  }

  LOG(LogLevel::Debug, ("DOMMediaStream %p Created new track %p with ID %u", this, track, aTrackID));

  mOwnedTracks.AppendElement(
    new TrackPort(mOwnedPort, track, TrackPort::InputPortOwnership::EXTERNAL));

  mTracks.AppendElement(
    new TrackPort(mPlaybackPort, track, TrackPort::InputPortOwnership::EXTERNAL));

  NotifyTrackAdded(track);
  return track;
}

already_AddRefed<MediaStreamTrack>
DOMMediaStream::CreateClonedDOMTrack(MediaStreamTrack& aTrack,
                                     TrackID aCloneTrackID)
{
  MOZ_RELEASE_ASSERT(mOwnedStream);
  MOZ_RELEASE_ASSERT(mPlaybackStream);
  MOZ_RELEASE_ASSERT(IsTrackIDExplicit(aCloneTrackID));

  TrackID inputTrackID = aTrack.mInputTrackID;
  MediaStream* inputStream = aTrack.GetInputStream();

  RefPtr<MediaStreamTrack> newTrack = aTrack.CloneInternal(this, aCloneTrackID);

  newTrack->mOriginalTrack =
    aTrack.mOriginalTrack ? aTrack.mOriginalTrack.get() : &aTrack;

  LOG(LogLevel::Debug, ("DOMMediaStream %p Created new track %p cloned from stream %p track %d",
                        this, newTrack.get(), inputStream, inputTrackID));

  RefPtr<MediaInputPort> inputPort =
    mOwnedStream->AllocateInputPort(inputStream, inputTrackID, aCloneTrackID);

  mOwnedTracks.AppendElement(
    new TrackPort(inputPort, newTrack, TrackPort::InputPortOwnership::OWNED));

  mTracks.AppendElement(
    new TrackPort(mPlaybackPort, newTrack, TrackPort::InputPortOwnership::EXTERNAL));

  NotifyTrackAdded(newTrack);

  newTrack->SetEnabled(aTrack.Enabled());

  return newTrack.forget();
}

MediaStreamTrack*
DOMMediaStream::FindOwnedDOMTrack(MediaStream* aInputStream,
                                  TrackID aInputTrackID) const
{
  MOZ_RELEASE_ASSERT(mOwnedStream);

  for (const RefPtr<TrackPort>& info : mOwnedTracks) {
    if (info->GetInputPort() &&
        info->GetInputPort()->GetSource() == aInputStream &&
        info->GetTrack()->mInputTrackID == aInputTrackID) {
      // This track is owned externally but in our playback stream.
      return info->GetTrack();
    }
  }
  return nullptr;
}

DOMMediaStream::TrackPort*
DOMMediaStream::FindOwnedTrackPort(const MediaStreamTrack& aTrack) const
{
  for (const RefPtr<TrackPort>& info : mOwnedTracks) {
    if (info->GetTrack() == &aTrack) {
      return info;
    }
  }
  return nullptr;
}


MediaStreamTrack*
DOMMediaStream::FindPlaybackDOMTrack(MediaStream* aInputStream, TrackID aInputTrackID) const
{
  if (!mPlaybackStream) {
    // One would think we can assert mPlaybackStream here, but track clones have
    // a dummy DOMMediaStream that doesn't have a playback stream, so we can't.
    return nullptr;
  }

  for (const RefPtr<TrackPort>& info : mTracks) {
    if (info->GetInputPort() == mPlaybackPort &&
        aInputStream == mOwnedStream &&
        info->GetTrack()->mInputTrackID == aInputTrackID) {
      // This track is in our owned and playback streams.
      return info->GetTrack();
    }
    if (info->GetInputPort() &&
        info->GetInputPort()->GetSource() == aInputStream &&
        info->GetSourceTrackId() == aInputTrackID) {
      // This track is owned externally but in our playback stream.
      MOZ_ASSERT(IsTrackIDExplicit(aInputTrackID));
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

  RecomputePrincipal();

  aTrack->AddPrincipalChangeObserver(this);

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

  aTrack->RemovePrincipalChangeObserver(this);

  for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
    const RefPtr<TrackListener>& listener = mTrackListeners[i];
    listener->NotifyTrackRemoved(aTrack);
  }

  RecomputePrincipal();
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
  nsCOMPtr<nsPIDOMWindowInner> pWindow = GetParentObject();
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
DOMLocalMediaStream::CreateSourceStream(nsPIDOMWindowInner* aWindow,
                                        MediaStreamGraph* aGraph,
                                        MediaStreamTrackSourceGetter* aTrackSourceGetter)
{
  RefPtr<DOMLocalMediaStream> stream =
    new DOMLocalMediaStream(aWindow, aTrackSourceGetter);
  stream->InitSourceStream(aGraph);
  return stream.forget();
}

already_AddRefed<DOMLocalMediaStream>
DOMLocalMediaStream::CreateTrackUnionStream(nsPIDOMWindowInner* aWindow,
                                            MediaStreamGraph* aGraph,
                                            MediaStreamTrackSourceGetter* aTrackSourceGetter)
{
  RefPtr<DOMLocalMediaStream> stream =
    new DOMLocalMediaStream(aWindow, aTrackSourceGetter);
  stream->InitTrackUnionStream(aGraph);
  return stream.forget();
}

DOMAudioNodeMediaStream::DOMAudioNodeMediaStream(nsPIDOMWindowInner* aWindow, AudioNode* aNode)
  : DOMMediaStream(aWindow, nullptr),
    mStreamNode(aNode)
{
}

DOMAudioNodeMediaStream::~DOMAudioNodeMediaStream()
{
}

already_AddRefed<DOMAudioNodeMediaStream>
DOMAudioNodeMediaStream::CreateTrackUnionStream(nsPIDOMWindowInner* aWindow,
                                                AudioNode* aNode,
                                                MediaStreamGraph* aGraph)
{
  RefPtr<DOMAudioNodeMediaStream> stream = new DOMAudioNodeMediaStream(aWindow, aNode);
  stream->InitTrackUnionStream(aGraph);
  return stream.forget();
}

DOMHwMediaStream::DOMHwMediaStream(nsPIDOMWindowInner* aWindow)
  : DOMLocalMediaStream(aWindow, nullptr)
{
}

DOMHwMediaStream::~DOMHwMediaStream()
{
}

already_AddRefed<DOMHwMediaStream>
DOMHwMediaStream::CreateHwStream(nsPIDOMWindowInner* aWindow,
                                 OverlayImage* aImage)
{
  RefPtr<DOMHwMediaStream> stream = new DOMHwMediaStream(aWindow);

  MediaStreamGraph* graph =
    MediaStreamGraph::GetInstance(MediaStreamGraph::SYSTEM_THREAD_DRIVER,
                                  AudioChannel::Normal);
  stream->InitSourceStream(graph);
  stream->Init(stream->GetInputStream(), aImage);

  return stream.forget();
}

void
DOMHwMediaStream::Init(MediaStream* stream, OverlayImage* aImage)
{
  SourceMediaStream* srcStream = stream->AsSourceStream();

#ifdef MOZ_WIDGET_GONK
  if (aImage) {
    mOverlayImage = aImage;
  } else {
    Data imageData;
    imageData.mOverlayId = DEFAULT_IMAGE_ID;
    imageData.mSize.width = DEFAULT_IMAGE_WIDTH;
    imageData.mSize.height = DEFAULT_IMAGE_HEIGHT;

    mOverlayImage = new OverlayImage();
    mOverlayImage->SetData(imageData);
  }
#endif

  if (srcStream) {
    VideoSegment segment;
#ifdef MOZ_WIDGET_GONK
    const StreamTime delta = STREAM_TIME_MAX; // Because MediaStreamGraph will run out frames in non-autoplay mode,
                                              // we must give it bigger frame length to cover this situation.

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
  if (mOverlayImage->GetSidebandStream().IsValid()) {
    OverlayImage::SidebandStreamData imgData;
    imgData.mStream = mOverlayImage->GetSidebandStream();
    imgData.mSize = IntSize(width, height);
    mOverlayImage->SetData(imgData);
  } else {
    OverlayImage::Data imgData;
    imgData.mOverlayId = mOverlayImage->GetOverlayId();
    imgData.mSize = IntSize(width, height);
    mOverlayImage->SetData(imgData);
  }
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
DOMHwMediaStream::SetOverlayImage(OverlayImage* aImage)
{
  if (!aImage) {
    return;
  }
#ifdef MOZ_WIDGET_GONK
  mOverlayImage = aImage;
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
