/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMediaStream.h"

#include "AudioCaptureStream.h"
#include "AudioChannelAgent.h"
#include "AudioStreamTrack.h"
#include "Layers.h"
#include "MediaStreamGraph.h"
#include "MediaStreamGraphImpl.h"
#include "MediaStreamListener.h"
#include "VideoStreamTrack.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/AudioNode.h"
#include "mozilla/dom/AudioTrack.h"
#include "mozilla/dom/AudioTrackList.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaStreamTrackEvent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/VideoTrack.h"
#include "mozilla/dom/VideoTrackList.h"
#include "mozilla/media/MediaUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsIUUIDGenerator.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"
#include "nsRFPService.h"
#include "nsServiceManagerUtils.h"

#ifdef LOG
#  undef LOG
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::media;

static LazyLogModule gMediaStreamLog("MediaStream");
#define LOG(type, msg) MOZ_LOG(gMediaStreamLog, type, msg)

const TrackID TRACK_VIDEO_PRIMARY = 1;

static bool ContainsLiveTracks(
    nsTArray<RefPtr<DOMMediaStream::TrackPort>>& aTracks) {
  for (auto& port : aTracks) {
    if (port->GetTrack()->ReadyState() == MediaStreamTrackState::Live) {
      return true;
    }
  }

  return false;
}

DOMMediaStream::TrackPort::TrackPort(MediaInputPort* aInputPort,
                                     MediaStreamTrack* aTrack,
                                     const InputPortOwnership aOwnership)
    : mInputPort(aInputPort), mTrack(aTrack), mOwnership(aOwnership) {
  MOZ_ASSERT(mInputPort);
  MOZ_ASSERT(mTrack);

  MOZ_COUNT_CTOR(TrackPort);
}

DOMMediaStream::TrackPort::~TrackPort() {
  MOZ_COUNT_DTOR(TrackPort);

  if (mOwnership == InputPortOwnership::OWNED) {
    DestroyInputPort();
  }
}

void DOMMediaStream::TrackPort::DestroyInputPort() {
  if (mInputPort) {
    mInputPort->Destroy();
    mInputPort = nullptr;
  }
}

MediaStream* DOMMediaStream::TrackPort::GetSource() const {
  return mInputPort ? mInputPort->GetSource() : nullptr;
}

TrackID DOMMediaStream::TrackPort::GetSourceTrackId() const {
  return mInputPort ? mInputPort->GetSourceTrackId() : TRACK_INVALID;
}

RefPtr<GenericPromise> DOMMediaStream::TrackPort::BlockSourceTrackId(
    TrackID aTrackId, BlockingMode aBlockingMode) {
  if (!mInputPort) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  return mInputPort->BlockSourceTrackId(aTrackId, aBlockingMode);
}

NS_IMPL_CYCLE_COLLECTION(DOMMediaStream::TrackPort, mTrack)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMMediaStream::TrackPort, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMMediaStream::TrackPort, Release)

class DOMMediaStream::PlaybackTrackListener : public MediaStreamTrackConsumer {
 public:
  explicit PlaybackTrackListener(DOMMediaStream* aStream) : mStream(aStream) {}

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PlaybackTrackListener)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(PlaybackTrackListener)

  void NotifyEnded(MediaStreamTrack* aTrack) override {
    if (!mStream) {
      MOZ_ASSERT(false);
      return;
    }

    if (!aTrack) {
      MOZ_ASSERT(false);
      return;
    }

    MOZ_ASSERT(mStream->HasTrack(*aTrack));
    mStream->NotifyTrackRemoved(aTrack);
  }

 protected:
  virtual ~PlaybackTrackListener() {}

  RefPtr<DOMMediaStream> mStream;
};

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMMediaStream::PlaybackTrackListener,
                                     AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMMediaStream::PlaybackTrackListener,
                                       Release)
NS_IMPL_CYCLE_COLLECTION(DOMMediaStream::PlaybackTrackListener, mStream)

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMMediaStream,
                                                DOMEventTargetHelper)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwnedTracks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTracks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsumersToKeepAlive)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPlaybackTrackListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVideoPrincipal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMMediaStream,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwnedTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsumersToKeepAlive)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlaybackTrackListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVideoPrincipal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(DOMMediaStream, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DOMMediaStream, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMMediaStream)
  NS_INTERFACE_MAP_ENTRY(DOMMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream,
                                   mStreamNode)

NS_IMPL_ADDREF_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream)
NS_IMPL_RELEASE_INHERITED(DOMAudioNodeMediaStream, DOMMediaStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMAudioNodeMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMMediaStream)

DOMMediaStream::DOMMediaStream(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow),
      mInputStream(nullptr),
      mOwnedStream(nullptr),
      mPlaybackStream(nullptr),
      mTracksPendingRemoval(0),
      mPlaybackTrackListener(MakeAndAddRef<PlaybackTrackListener>(this)),
      mTracksCreated(false),
      mNotifiedOfMediaStreamGraphShutdown(false),
      mActive(false),
      mFinishedOnInactive(true),
      mCORSMode(CORS_NONE) {
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

DOMMediaStream::~DOMMediaStream() { Destroy(); }

void DOMMediaStream::Destroy() {
  LOG(LogLevel::Debug, ("DOMMediaStream %p Being destroyed.", this));
  for (const RefPtr<TrackPort>& info : mTracks) {
    // We must remove ourselves from each track's principal change observer list
    // before we die. CC may have cleared info->mTrack so guard against it.
    MediaStreamTrack* track = info->GetTrack();
    if (track) {
      track->RemovePrincipalChangeObserver(this);
      if (!track->Ended()) {
        track->RemoveConsumer(mPlaybackTrackListener);
      }
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
    mPlaybackStream->UnregisterUser();
    mPlaybackStream = nullptr;
  }
  if (mOwnedStream) {
    mOwnedStream->UnregisterUser();
    mOwnedStream = nullptr;
  }
  if (mInputStream) {
    mInputStream->UnregisterUser();
    mInputStream = nullptr;
  }
  mTrackListeners.Clear();
}

JSObject* DOMMediaStream::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return dom::MediaStream_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<DOMMediaStream> DOMMediaStream::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  Sequence<OwningNonNull<MediaStreamTrack>> emptyTrackSeq;
  return Constructor(aGlobal, emptyTrackSeq, aRv);
}

/* static */
already_AddRefed<DOMMediaStream> DOMMediaStream::Constructor(
    const GlobalObject& aGlobal, const DOMMediaStream& aStream,
    ErrorResult& aRv) {
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

/* static */
already_AddRefed<DOMMediaStream> DOMMediaStream::Constructor(
    const GlobalObject& aGlobal,
    const Sequence<OwningNonNull<MediaStreamTrack>>& aTracks,
    ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  auto newStream = MakeRefPtr<DOMMediaStream>(ownerWindow);

  for (MediaStreamTrack& track : aTracks) {
    if (!newStream->GetPlaybackStream()) {
      MOZ_RELEASE_ASSERT(track.Graph());
      newStream->InitPlaybackStreamCommon(track.Graph());
    }
    newStream->AddTrack(track);
  }

  if (!newStream->GetPlaybackStream()) {
    MOZ_ASSERT(aTracks.IsEmpty());
    MediaStreamGraph* graph = MediaStreamGraph::GetInstance(
        MediaStreamGraph::SYSTEM_THREAD_DRIVER, ownerWindow,
        MediaStreamGraph::REQUEST_DEFAULT_SAMPLE_RATE);
    newStream->InitPlaybackStreamCommon(graph);
  }

  return newStream.forget();
}

already_AddRefed<Promise> DOMMediaStream::CountUnderlyingStreams(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(aGlobal.GetAsSupports());
  if (!go) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  MediaStreamGraph* graph = MediaStreamGraph::GetInstanceIfExists(
      window, MediaStreamGraph::REQUEST_DEFAULT_SAMPLE_RATE);
  if (!graph) {
    p->MaybeResolve(0);
    return p.forget();
  }

  auto* graphImpl = static_cast<MediaStreamGraphImpl*>(graph);

  class Counter : public ControlMessage {
   public:
    Counter(MediaStreamGraphImpl* aGraph, const RefPtr<Promise>& aPromise)
        : ControlMessage(nullptr), mGraph(aGraph), mPromise(aPromise) {
      MOZ_ASSERT(NS_IsMainThread());
    }

    void Run() override {
      uint32_t streams =
          mGraph->mStreams.Length() + mGraph->mSuspendedStreams.Length();
      mGraph->DispatchToMainThreadStableState(NS_NewRunnableFunction(
          "DOMMediaStream::CountUnderlyingStreams (stable state)",
          [promise = std::move(mPromise), streams]() mutable {
            NS_DispatchToMainThread(NS_NewRunnableFunction(
                "DOMMediaStream::CountUnderlyingStreams",
                [promise = std::move(promise), streams]() {
                  promise->MaybeResolve(streams);
                }));
          }));
    }

    // mPromise can only be AddRefed/Released on main thread.
    // In case of shutdown, Run() does not run, so we dispatch mPromise to be
    // released on main thread here.
    void RunDuringShutdown() override {
      NS_ReleaseOnMainThreadSystemGroup(
          "DOMMediaStream::CountUnderlyingStreams::Counter::RunDuringShutdown",
          mPromise.forget());
    }

   private:
    // mGraph owns this Counter instance and decides its lifetime.
    MediaStreamGraphImpl* mGraph;
    RefPtr<Promise> mPromise;
  };
  graphImpl->AppendMessage(MakeUnique<Counter>(graphImpl, p));

  return p.forget();
}

void DOMMediaStream::GetId(nsAString& aID) const { aID = mID; }

void DOMMediaStream::GetAudioTracks(
    nsTArray<RefPtr<AudioStreamTrack>>& aTracks) const {
  for (const RefPtr<TrackPort>& info : mTracks) {
    if (AudioStreamTrack* t = info->GetTrack()->AsAudioStreamTrack()) {
      aTracks.AppendElement(t);
    }
  }
}

void DOMMediaStream::GetAudioTracks(
    nsTArray<RefPtr<MediaStreamTrack>>& aTracks) const {
  for (const RefPtr<TrackPort>& info : mTracks) {
    if (info->GetTrack()->AsAudioStreamTrack()) {
      aTracks.AppendElement(info->GetTrack());
    }
  }
}

void DOMMediaStream::GetVideoTracks(
    nsTArray<RefPtr<VideoStreamTrack>>& aTracks) const {
  for (const RefPtr<TrackPort>& info : mTracks) {
    if (VideoStreamTrack* t = info->GetTrack()->AsVideoStreamTrack()) {
      aTracks.AppendElement(t);
    }
  }
}

void DOMMediaStream::GetVideoTracks(
    nsTArray<RefPtr<MediaStreamTrack>>& aTracks) const {
  for (const RefPtr<TrackPort>& info : mTracks) {
    if (info->GetTrack()->AsVideoStreamTrack()) {
      aTracks.AppendElement(info->GetTrack());
    }
  }
}

void DOMMediaStream::GetTracks(
    nsTArray<RefPtr<MediaStreamTrack>>& aTracks) const {
  for (const RefPtr<TrackPort>& info : mTracks) {
    aTracks.AppendElement(info->GetTrack());
  }
}

void DOMMediaStream::AddTrack(MediaStreamTrack& aTrack) {
  MOZ_RELEASE_ASSERT(mPlaybackStream);

  RefPtr<ProcessedMediaStream> dest = mPlaybackStream->AsProcessedStream();
  MOZ_ASSERT(dest);
  if (!dest) {
    return;
  }

  LOG(LogLevel::Info,
      ("DOMMediaStream %p Adding track %p (from stream %p with ID %d)", this,
       &aTrack, aTrack.mOwningStream.get(), aTrack.mTrackID));

  if (mPlaybackStream->Graph() != aTrack.Graph()) {
    NS_ASSERTION(false,
                 "Cannot combine tracks from different MediaStreamGraphs");
    LOG(LogLevel::Error, ("DOMMediaStream %p Own MSG %p != aTrack's MSG %p",
                          this, mPlaybackStream->Graph(), aTrack.Graph()));

    nsAutoString trackId;
    aTrack.GetId(trackId);
    const char16_t* params[] = {trackId.get()};
    nsCOMPtr<nsPIDOMWindowInner> pWindow = GetParentObject();
    Document* document = pWindow ? pWindow->GetExtantDoc() : nullptr;
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("Media"), document,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "MediaStreamAddTrackDifferentAudioChannel",
                                    params, ArrayLength(params));
    return;
  }

  if (HasTrack(aTrack)) {
    LOG(LogLevel::Debug,
        ("DOMMediaStream %p already contains track %p", this, &aTrack));
    return;
  }

  // Hook up the underlying track with our underlying playback stream.
  RefPtr<MediaInputPort> inputPort = GetPlaybackStream()->AllocateInputPort(
      aTrack.GetOwnedStream(), aTrack.mTrackID);
  RefPtr<TrackPort> trackPort =
      new TrackPort(inputPort, &aTrack, TrackPort::InputPortOwnership::OWNED);
  mTracks.AppendElement(trackPort.forget());
  NotifyTrackAdded(&aTrack);

  LOG(LogLevel::Debug, ("DOMMediaStream %p Added track %p", this, &aTrack));
}

void DOMMediaStream::RemoveTrack(MediaStreamTrack& aTrack) {
  LOG(LogLevel::Info,
      ("DOMMediaStream %p Removing track %p (from stream %p with ID %d)", this,
       &aTrack, aTrack.mOwningStream.get(), aTrack.mTrackID));

  RefPtr<TrackPort> toRemove = FindPlaybackTrackPort(aTrack);
  if (!toRemove) {
    LOG(LogLevel::Debug,
        ("DOMMediaStream %p does not contain track %p", this, &aTrack));
    return;
  }

  DebugOnly<bool> removed = mTracks.RemoveElement(toRemove);
  NS_ASSERTION(removed,
               "If there's a track port we should be able to remove it");

  // If the track comes from a TRACK_ANY input port (i.e., mOwnedPort), we need
  // to block it in the port. Doing this for a locked track is still OK as it
  // will first block the track, then destroy the port. Both cause the track to
  // end.
  // If the track has already ended, it's input port might be gone, so in those
  // cases blocking the underlying track should be avoided.
  if (!aTrack.Ended()) {
    BlockPlaybackTrack(toRemove);
    NotifyTrackRemoved(&aTrack);
  }

  LOG(LogLevel::Debug, ("DOMMediaStream %p Removed track %p", this, &aTrack));
}

already_AddRefed<DOMMediaStream> DOMMediaStream::Clone() {
  auto newStream = MakeRefPtr<DOMMediaStream>(GetParentObject());

  LOG(LogLevel::Info,
      ("DOMMediaStream %p created clone %p", this, newStream.get()));

  MOZ_RELEASE_ASSERT(mPlaybackStream);
  MOZ_RELEASE_ASSERT(mPlaybackStream->Graph());
  MediaStreamGraph* graph = mPlaybackStream->Graph();

  // We initiate the owned and playback streams first, since we need to create
  // all existing DOM tracks before we add the generic input port from
  // mInputStream to mOwnedStream (see AllocateInputPort wrt. destination
  // TrackID as to why).
  newStream->InitOwnedStreamCommon(graph);
  newStream->InitPlaybackStreamCommon(graph);

  // Set up existing DOM tracks.
  TrackID allocatedTrackID = 1;
  for (const RefPtr<TrackPort>& info : mTracks) {
    MediaStreamTrack& track = *info->GetTrack();

    LOG(LogLevel::Debug,
        ("DOMMediaStream %p forwarding external track %p to clone %p", this,
         &track, newStream.get()));
    RefPtr<MediaStreamTrack> trackClone =
        newStream->CloneDOMTrack(track, allocatedTrackID++);
  }

  return newStream.forget();
}

bool DOMMediaStream::Active() const { return mActive; }

MediaStreamTrack* DOMMediaStream::GetTrackById(const nsAString& aId) const {
  for (const RefPtr<TrackPort>& info : mTracks) {
    nsString id;
    info->GetTrack()->GetId(id);
    if (id == aId) {
      return info->GetTrack();
    }
  }
  return nullptr;
}

MediaStreamTrack* DOMMediaStream::GetOwnedTrackById(const nsAString& aId) {
  for (const RefPtr<TrackPort>& info : mOwnedTracks) {
    nsString id;
    info->GetTrack()->GetId(id);
    if (id == aId) {
      return info->GetTrack();
    }
  }
  return nullptr;
}

bool DOMMediaStream::HasTrack(const MediaStreamTrack& aTrack) const {
  return !!FindPlaybackTrackPort(aTrack);
}

bool DOMMediaStream::OwnsTrack(const MediaStreamTrack& aTrack) const {
  return !!FindOwnedTrackPort(aTrack);
}

bool DOMMediaStream::IsFinished() const {
  return !mPlaybackStream || mPlaybackStream->IsFinished();
}

TrackRate DOMMediaStream::GraphRate() {
  if (mPlaybackStream) {
    return mPlaybackStream->GraphRate();
  }
  if (mOwnedStream) {
    return mOwnedStream->GraphRate();
  }
  if (mInputStream) {
    return mInputStream->GraphRate();
  }

  MOZ_ASSERT(false, "Not hooked up to a graph");
  return 0;
}

void DOMMediaStream::InitSourceStream(MediaStreamGraph* aGraph) {
  InitInputStreamCommon(aGraph->CreateSourceStream(), aGraph);
  InitOwnedStreamCommon(aGraph);
  InitPlaybackStreamCommon(aGraph);
}

void DOMMediaStream::InitTrackUnionStream(MediaStreamGraph* aGraph) {
  InitInputStreamCommon(aGraph->CreateTrackUnionStream(), aGraph);
  InitOwnedStreamCommon(aGraph);
  InitPlaybackStreamCommon(aGraph);
}

void DOMMediaStream::InitAudioCaptureStream(nsIPrincipal* aPrincipal,
                                            MediaStreamGraph* aGraph) {
  const TrackID AUDIO_TRACK = 1;

  RefPtr<BasicTrackSource> audioCaptureSource =
      new BasicTrackSource(aPrincipal, MediaSourceEnum::AudioCapture);

  AudioCaptureStream* audioCaptureStream = static_cast<AudioCaptureStream*>(
      aGraph->CreateAudioCaptureStream(AUDIO_TRACK));
  InitInputStreamCommon(audioCaptureStream, aGraph);
  InitOwnedStreamCommon(aGraph);
  InitPlaybackStreamCommon(aGraph);
  RefPtr<MediaStreamTrack> track =
      CreateDOMTrack(AUDIO_TRACK, MediaSegment::AUDIO, audioCaptureSource);
  AddTrackInternal(track);

  audioCaptureStream->Start();
}

void DOMMediaStream::InitInputStreamCommon(MediaStream* aStream,
                                           MediaStreamGraph* aGraph) {
  MOZ_ASSERT(!mOwnedStream,
             "Input stream must be initialized before owned stream");

  mInputStream = aStream;
  mInputStream->RegisterUser();
}

void DOMMediaStream::InitOwnedStreamCommon(MediaStreamGraph* aGraph) {
  MOZ_ASSERT(!mPlaybackStream,
             "Owned stream must be initialized before playback stream");

  mOwnedStream = aGraph->CreateTrackUnionStream();
  mOwnedStream->QueueSetAutofinish(true);
  mOwnedStream->RegisterUser();
  if (mInputStream) {
    mOwnedPort = mOwnedStream->AllocateInputPort(mInputStream);
  }
}

void DOMMediaStream::InitPlaybackStreamCommon(MediaStreamGraph* aGraph) {
  mPlaybackStream = aGraph->CreateTrackUnionStream();
  mPlaybackStream->QueueSetAutofinish(true);
  mPlaybackStream->RegisterUser();
  if (mOwnedStream) {
    mPlaybackPort = mPlaybackStream->AllocateInputPort(mOwnedStream);
  }

  LOG(LogLevel::Debug, ("DOMMediaStream %p Initiated with mInputStream=%p, "
                        "mOwnedStream=%p, mPlaybackStream=%p",
                        this, mInputStream, mOwnedStream, mPlaybackStream));
}

already_AddRefed<DOMMediaStream> DOMMediaStream::CreateSourceStreamAsInput(
    nsPIDOMWindowInner* aWindow, MediaStreamGraph* aGraph) {
  auto stream = MakeRefPtr<DOMMediaStream>(aWindow);
  stream->InitSourceStream(aGraph);
  return stream.forget();
}

already_AddRefed<DOMMediaStream> DOMMediaStream::CreateTrackUnionStreamAsInput(
    nsPIDOMWindowInner* aWindow, MediaStreamGraph* aGraph) {
  auto stream = MakeRefPtr<DOMMediaStream>(aWindow);
  stream->InitTrackUnionStream(aGraph);
  return stream.forget();
}

already_AddRefed<DOMMediaStream>
DOMMediaStream::CreateAudioCaptureStreamAsInput(nsPIDOMWindowInner* aWindow,
                                                nsIPrincipal* aPrincipal,
                                                MediaStreamGraph* aGraph) {
  auto stream = MakeRefPtr<DOMMediaStream>(aWindow);
  stream->InitAudioCaptureStream(aPrincipal, aGraph);
  return stream.forget();
}

void DOMMediaStream::PrincipalChanged(MediaStreamTrack* aTrack) {
  MOZ_ASSERT(aTrack);
  NS_ASSERTION(HasTrack(*aTrack), "Principal changed for an unknown track");
  LOG(LogLevel::Info,
      ("DOMMediaStream %p Principal changed for track %p", this, aTrack));
  RecomputePrincipal();
}

void DOMMediaStream::RecomputePrincipal() {
  nsCOMPtr<nsIPrincipal> previousPrincipal = mPrincipal.forget();
  nsCOMPtr<nsIPrincipal> previousVideoPrincipal = mVideoPrincipal.forget();

  if (mTracksPendingRemoval > 0) {
    LOG(LogLevel::Info, ("DOMMediaStream %p RecomputePrincipal() Cannot "
                         "recompute stream principal with tracks pending "
                         "removal.",
                         this));
    return;
  }

  LOG(LogLevel::Debug, ("DOMMediaStream %p Recomputing principal. "
                        "Old principal was %p.",
                        this, previousPrincipal.get()));

  // mPrincipal is recomputed based on all current tracks, and tracks that have
  // not ended in our playback stream.
  for (const RefPtr<TrackPort>& info : mTracks) {
    if (info->GetTrack()->Ended()) {
      continue;
    }
    LOG(LogLevel::Debug,
        ("DOMMediaStream %p Taking live track %p with "
         "principal %p into account.",
         this, info->GetTrack(), info->GetTrack()->GetPrincipal()));
    nsContentUtils::CombineResourcePrincipals(&mPrincipal,
                                              info->GetTrack()->GetPrincipal());
    if (info->GetTrack()->AsVideoStreamTrack()) {
      nsContentUtils::CombineResourcePrincipals(
          &mVideoPrincipal, info->GetTrack()->GetPrincipal());
    }
  }

  LOG(LogLevel::Debug,
      ("DOMMediaStream %p new principal is %p.", this, mPrincipal.get()));

  if (previousPrincipal != mPrincipal ||
      previousVideoPrincipal != mVideoPrincipal) {
    NotifyPrincipalChanged();
  }
}

void DOMMediaStream::NotifyPrincipalChanged() {
  if (!mPrincipal) {
    // When all tracks are removed, mPrincipal will change to nullptr.
    LOG(LogLevel::Info,
        ("DOMMediaStream %p Principal changed to nothing.", this));
  } else {
    LOG(LogLevel::Info, ("DOMMediaStream %p Principal changed. Now: "
                         "null=%d, codebase=%d, expanded=%d, system=%d",
                         this, mPrincipal->GetIsNullPrincipal(),
                         mPrincipal->GetIsCodebasePrincipal(),
                         mPrincipal->GetIsExpandedPrincipal(),
                         mPrincipal->IsSystemPrincipal()));
  }

  for (uint32_t i = 0; i < mPrincipalChangeObservers.Length(); ++i) {
    mPrincipalChangeObservers[i]->PrincipalChanged(this);
  }
}

bool DOMMediaStream::AddPrincipalChangeObserver(
    PrincipalChangeObserver<DOMMediaStream>* aObserver) {
  return mPrincipalChangeObservers.AppendElement(aObserver) != nullptr;
}

bool DOMMediaStream::RemovePrincipalChangeObserver(
    PrincipalChangeObserver<DOMMediaStream>* aObserver) {
  return mPrincipalChangeObservers.RemoveElement(aObserver);
}

void DOMMediaStream::AddTrackInternal(MediaStreamTrack* aTrack) {
  MOZ_ASSERT(aTrack->mOwningStream == this);
  MOZ_ASSERT(FindOwnedDOMTrack(aTrack->GetInputStream(), aTrack->mInputTrackID,
                               aTrack->mTrackID));
  MOZ_ASSERT(!FindPlaybackDOMTrack(aTrack->GetOwnedStream(), aTrack->mTrackID));

  LOG(LogLevel::Debug,
      ("DOMMediaStream %p Adding owned track %p", this, aTrack));

  mTracks.AppendElement(new TrackPort(mPlaybackPort, aTrack,
                                      TrackPort::InputPortOwnership::EXTERNAL));

  NotifyTrackAdded(aTrack);

  DispatchTrackEvent(NS_LITERAL_STRING("addtrack"), aTrack);
}

already_AddRefed<MediaStreamTrack> DOMMediaStream::CreateDOMTrack(
    TrackID aTrackID, MediaSegment::Type aType, MediaStreamTrackSource* aSource,
    const MediaTrackConstraints& aConstraints) {
  MOZ_RELEASE_ASSERT(mInputStream);
  MOZ_RELEASE_ASSERT(mOwnedStream);

  MOZ_ASSERT(FindOwnedDOMTrack(GetInputStream(), aTrackID) == nullptr);

  RefPtr<MediaStreamTrack> track;
  switch (aType) {
    case MediaSegment::AUDIO:
      track =
          new AudioStreamTrack(this, aTrackID, aTrackID, aSource, aConstraints);
      break;
    case MediaSegment::VIDEO:
      track =
          new VideoStreamTrack(this, aTrackID, aTrackID, aSource, aConstraints);
      break;
    default:
      MOZ_CRASH("Unhandled track type");
  }

  LOG(LogLevel::Debug, ("DOMMediaStream %p Created new track %p with ID %u",
                        this, track.get(), aTrackID));

  mOwnedTracks.AppendElement(new TrackPort(
      mOwnedPort, track, TrackPort::InputPortOwnership::EXTERNAL));

  return track.forget();
}

already_AddRefed<MediaStreamTrack> DOMMediaStream::CloneDOMTrack(
    MediaStreamTrack& aTrack, TrackID aCloneTrackID) {
  MOZ_RELEASE_ASSERT(mOwnedStream);
  MOZ_RELEASE_ASSERT(mPlaybackStream);
  MOZ_RELEASE_ASSERT(IsTrackIDExplicit(aCloneTrackID));

  TrackID inputTrackID = aTrack.mInputTrackID;
  MediaStream* inputStream = aTrack.GetInputStream();

  RefPtr<MediaStreamTrack> newTrack = aTrack.CloneInternal(this, aCloneTrackID);

  newTrack->mOriginalTrack =
      aTrack.mOriginalTrack ? aTrack.mOriginalTrack.get() : &aTrack;

  LOG(LogLevel::Debug,
      ("DOMMediaStream %p Created new track %p cloned from stream %p track %d",
       this, newTrack.get(), inputStream, inputTrackID));

  RefPtr<MediaInputPort> inputPort =
      mOwnedStream->AllocateInputPort(inputStream, inputTrackID, aCloneTrackID);

  mOwnedTracks.AppendElement(
      new TrackPort(inputPort, newTrack, TrackPort::InputPortOwnership::OWNED));

  mTracks.AppendElement(new TrackPort(mPlaybackPort, newTrack,
                                      TrackPort::InputPortOwnership::EXTERNAL));

  NotifyTrackAdded(newTrack);

  newTrack->SetEnabled(aTrack.Enabled());
  newTrack->SetMuted(aTrack.Muted());
  newTrack->SetReadyState(aTrack.ReadyState());

  if (aTrack.Ended()) {
    // For extra suspenders, make sure that we don't forward data by mistake
    // to the clone when the original has already ended.
    // We only block END_EXISTING to allow any pending clones to end.
    Unused << inputPort->BlockSourceTrackId(inputTrackID,
                                            BlockingMode::END_EXISTING);
  }

  return newTrack.forget();
}

static DOMMediaStream::TrackPort* FindTrackPortAmongTracks(
    const MediaStreamTrack& aTrack,
    const nsTArray<RefPtr<DOMMediaStream::TrackPort>>& aTracks) {
  for (const RefPtr<DOMMediaStream::TrackPort>& info : aTracks) {
    if (info->GetTrack() == &aTrack) {
      return info;
    }
  }
  return nullptr;
}

MediaStreamTrack* DOMMediaStream::FindOwnedDOMTrack(MediaStream* aInputStream,
                                                    TrackID aInputTrackID,
                                                    TrackID aTrackID) const {
  MOZ_RELEASE_ASSERT(mOwnedStream);

  for (const RefPtr<TrackPort>& info : mOwnedTracks) {
    if (info->GetInputPort() &&
        info->GetInputPort()->GetSource() == aInputStream &&
        info->GetTrack()->mInputTrackID == aInputTrackID &&
        (aTrackID == TRACK_ANY || info->GetTrack()->mTrackID == aTrackID)) {
      // This track is owned externally but in our playback stream.
      return info->GetTrack();
    }
  }
  return nullptr;
}

DOMMediaStream::TrackPort* DOMMediaStream::FindOwnedTrackPort(
    const MediaStreamTrack& aTrack) const {
  return FindTrackPortAmongTracks(aTrack, mOwnedTracks);
}

MediaStreamTrack* DOMMediaStream::FindPlaybackDOMTrack(
    MediaStream* aInputStream, TrackID aInputTrackID) const {
  if (!mPlaybackStream) {
    // One would think we can assert mPlaybackStream here, but track clones have
    // a dummy DOMMediaStream that doesn't have a playback stream, so we can't.
    return nullptr;
  }

  for (const RefPtr<TrackPort>& info : mTracks) {
    if (info->GetInputPort() == mPlaybackPort && aInputStream == mOwnedStream &&
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

DOMMediaStream::TrackPort* DOMMediaStream::FindPlaybackTrackPort(
    const MediaStreamTrack& aTrack) const {
  return FindTrackPortAmongTracks(aTrack, mTracks);
}

void DOMMediaStream::NotifyActive() {
  LOG(LogLevel::Info, ("DOMMediaStream %p NotifyActive(). ", this));

  MOZ_ASSERT(mActive);
  for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
    mTrackListeners[i]->NotifyActive();
  }
}

void DOMMediaStream::NotifyInactive() {
  LOG(LogLevel::Info, ("DOMMediaStream %p NotifyInactive(). ", this));

  MOZ_ASSERT(!mActive);
  for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
    mTrackListeners[i]->NotifyInactive();
  }
}

void DOMMediaStream::RegisterTrackListener(TrackListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mNotifiedOfMediaStreamGraphShutdown) {
    // No more tracks will ever be added, so just do nothing.
    return;
  }
  mTrackListeners.AppendElement(aListener);
}

void DOMMediaStream::UnregisterTrackListener(TrackListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  mTrackListeners.RemoveElement(aListener);
}

void DOMMediaStream::SetFinishedOnInactive(bool aFinishedOnInactive) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mFinishedOnInactive == aFinishedOnInactive) {
    return;
  }

  mFinishedOnInactive = aFinishedOnInactive;

  if (mFinishedOnInactive && !ContainsLiveTracks(mTracks)) {
    NotifyTrackRemoved(nullptr);
  }
}

void DOMMediaStream::NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mTracksPendingRemoval > 0) {
    // If there are tracks pending removal we may not degrade the current
    // principals until those tracks have been confirmed removed from the
    // playback stream. Instead combine with the new track and the (potentially)
    // degraded principal will be calculated when it's safe.
    nsContentUtils::CombineResourcePrincipals(&mPrincipal,
                                              aTrack->GetPrincipal());
    LOG(LogLevel::Debug, ("DOMMediaStream %p saw a track get added. Combining "
                          "its principal %p into our while waiting for pending "
                          "tracks to be removed. New principal is %p.",
                          this, aTrack->GetPrincipal(), mPrincipal.get()));
    if (aTrack->AsVideoStreamTrack()) {
      nsContentUtils::CombineResourcePrincipals(&mVideoPrincipal,
                                                aTrack->GetPrincipal());
    }
  } else {
    LOG(LogLevel::Debug, ("DOMMediaStream %p saw a track get added. "
                          "Recomputing principal.",
                          this));
    RecomputePrincipal();
  }

  aTrack->AddPrincipalChangeObserver(this);
  aTrack->AddConsumer(mPlaybackTrackListener);

  for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
    mTrackListeners[i]->NotifyTrackAdded(aTrack);
  }

  if (mActive) {
    return;
  }

  // Check if we became active.
  if (ContainsLiveTracks(mTracks)) {
    mActive = true;
    NotifyActive();
  }
}

void DOMMediaStream::NotifyTrackRemoved(
    const RefPtr<MediaStreamTrack>& aTrack) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aTrack) {
    // aTrack may be null to allow HTMLMediaElement::MozCaptureStream streams
    // to be played until the source media element has ended. The source media
    // element will then call NotifyTrackRemoved(nullptr) to signal that we can
    // go inactive, regardless of the timing of the last track ending.

    aTrack->RemoveConsumer(mPlaybackTrackListener);
    aTrack->RemovePrincipalChangeObserver(this);

    for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
      mTrackListeners[i]->NotifyTrackRemoved(aTrack);
    }

    // Don't call RecomputePrincipal here as the track may still exist in the
    // playback stream in the MediaStreamGraph. It will instead be called when
    // the track has been confirmed removed by the graph. See
    // BlockPlaybackTrack().

    if (!mActive) {
      NS_ASSERTION(false, "Shouldn't remove a live track if already inactive");
      return;
    }
  }

  if (!mFinishedOnInactive) {
    return;
  }

  // Check if we became inactive.
  if (!ContainsLiveTracks(mTracks)) {
    mActive = false;
    NotifyInactive();
  }
}

nsresult DOMMediaStream::DispatchTrackEvent(
    const nsAString& aName, const RefPtr<MediaStreamTrack>& aTrack) {
  MediaStreamTrackEventInit init;
  init.mTrack = aTrack;

  RefPtr<MediaStreamTrackEvent> event =
      MediaStreamTrackEvent::Constructor(this, aName, init);

  return DispatchTrustedEvent(event);
}

void DOMMediaStream::BlockPlaybackTrack(TrackPort* aTrack) {
  MOZ_ASSERT(aTrack);
  ++mTracksPendingRemoval;
  RefPtr<DOMMediaStream> that = this;
  aTrack
      ->BlockSourceTrackId(aTrack->GetTrack()->mTrackID, BlockingMode::CREATION)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [this, that](bool aIgnore) { NotifyPlaybackTrackBlocked(); },
          [](const nsresult& aIgnore) {
            NS_ERROR("Could not remove track from MSG");
          });
}

void DOMMediaStream::NotifyPlaybackTrackBlocked() {
  MOZ_ASSERT(mTracksPendingRemoval > 0,
             "A track reported finished blocking more times than we asked for");
  if (--mTracksPendingRemoval == 0) {
    // The MediaStreamGraph has reported a track was blocked and we are not
    // waiting for any further tracks to get blocked. It is now safe to
    // recompute the principal based on our main thread track set state.
    LOG(LogLevel::Debug, ("DOMMediaStream %p saw all tracks pending removal "
                          "finish. Recomputing principal.",
                          this));
    RecomputePrincipal();
  }
}

DOMAudioNodeMediaStream::DOMAudioNodeMediaStream(nsPIDOMWindowInner* aWindow,
                                                 AudioNode* aNode)
    : DOMMediaStream(aWindow), mStreamNode(aNode) {}

DOMAudioNodeMediaStream::~DOMAudioNodeMediaStream() {}

already_AddRefed<DOMAudioNodeMediaStream>
DOMAudioNodeMediaStream::CreateTrackUnionStreamAsInput(
    nsPIDOMWindowInner* aWindow, AudioNode* aNode, MediaStreamGraph* aGraph) {
  RefPtr<DOMAudioNodeMediaStream> stream =
      new DOMAudioNodeMediaStream(aWindow, aNode);
  stream->InitTrackUnionStream(aGraph);
  return stream.forget();
}
