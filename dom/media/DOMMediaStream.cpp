/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMediaStream.h"

#include "AudioCaptureTrack.h"
#include "AudioChannelAgent.h"
#include "AudioStreamTrack.h"
#include "Layers.h"
#include "MediaTrackGraph.h"
#include "MediaTrackGraphImpl.h"
#include "MediaTrackListener.h"
#include "Tracing.h"
#include "VideoStreamTrack.h"
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
#include "nsGlobalWindowInner.h"
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

static bool ContainsLiveTracks(
    const nsTArray<RefPtr<MediaStreamTrack>>& aTracks) {
  for (const auto& track : aTracks) {
    if (track->ReadyState() == MediaStreamTrackState::Live) {
      return true;
    }
  }

  return false;
}

static bool ContainsLiveAudioTracks(
    const nsTArray<RefPtr<MediaStreamTrack>>& aTracks) {
  for (const auto& track : aTracks) {
    if (track->AsAudioStreamTrack() &&
        track->ReadyState() == MediaStreamTrackState::Live) {
      return true;
    }
  }

  return false;
}

class DOMMediaStream::PlaybackTrackListener : public MediaStreamTrackConsumer {
 public:
  NS_INLINE_DECL_REFCOUNTING(PlaybackTrackListener)

  explicit PlaybackTrackListener(DOMMediaStream* aStream) : mStream(aStream) {}

  void NotifyEnded(MediaStreamTrack* aTrack) override {
    if (!mStream) {
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
  virtual ~PlaybackTrackListener() = default;

  WeakPtr<DOMMediaStream> mStream;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMMediaStream)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMMediaStream,
                                                DOMEventTargetHelper)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTracks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsumersToKeepAlive)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMMediaStream,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTracks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsumersToKeepAlive)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(DOMMediaStream, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DOMMediaStream, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMMediaStream)
  NS_INTERFACE_MAP_ENTRY(DOMMediaStream)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

DOMMediaStream::DOMMediaStream(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow),
      mPlaybackTrackListener(MakeAndAddRef<PlaybackTrackListener>(this)) {
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
  for (const auto& track : mTracks) {
    // We must remove ourselves from each track's principal change observer list
    // before we die.
    if (!track->Ended()) {
      track->RemoveConsumer(mPlaybackTrackListener);
    }
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
    newStream->AddTrack(track);
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

  MediaTrackGraph* graph = MediaTrackGraph::GetInstanceIfExists(
      window, MediaTrackGraph::REQUEST_DEFAULT_SAMPLE_RATE,
      MediaTrackGraph::DEFAULT_OUTPUT_DEVICE);
  if (!graph) {
    p->MaybeResolve(0);
    return p.forget();
  }

  auto* graphImpl = static_cast<MediaTrackGraphImpl*>(graph);

  class Counter : public ControlMessage {
   public:
    Counter(MediaTrackGraphImpl* aGraph, const RefPtr<Promise>& aPromise)
        : ControlMessage(nullptr), mGraph(aGraph), mPromise(aPromise) {
      MOZ_ASSERT(NS_IsMainThread());
    }

    void Run() override {
      TRACE("DOMMediaStream::Counter")
      uint32_t streams =
          mGraph->mTracks.Length() + mGraph->mSuspendedTracks.Length();
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
      NS_ReleaseOnMainThread(
          "DOMMediaStream::CountUnderlyingStreams::Counter::RunDuringShutdown",
          mPromise.forget());
    }

   private:
    // mGraph owns this Counter instance and decides its lifetime.
    MediaTrackGraphImpl* mGraph;
    RefPtr<Promise> mPromise;
  };
  graphImpl->AppendMessage(MakeUnique<Counter>(graphImpl, p));

  return p.forget();
}

void DOMMediaStream::GetId(nsAString& aID) const { aID = mID; }

void DOMMediaStream::GetAudioTracks(
    nsTArray<RefPtr<AudioStreamTrack>>& aTracks) const {
  for (const auto& track : mTracks) {
    if (AudioStreamTrack* t = track->AsAudioStreamTrack()) {
      aTracks.AppendElement(t);
    }
  }
}

void DOMMediaStream::GetAudioTracks(
    nsTArray<RefPtr<MediaStreamTrack>>& aTracks) const {
  for (const auto& track : mTracks) {
    if (track->AsAudioStreamTrack()) {
      aTracks.AppendElement(track);
    }
  }
}

void DOMMediaStream::GetVideoTracks(
    nsTArray<RefPtr<VideoStreamTrack>>& aTracks) const {
  for (const auto& track : mTracks) {
    if (VideoStreamTrack* t = track->AsVideoStreamTrack()) {
      aTracks.AppendElement(t);
    }
  }
}

void DOMMediaStream::GetVideoTracks(
    nsTArray<RefPtr<MediaStreamTrack>>& aTracks) const {
  for (const auto& track : mTracks) {
    if (track->AsVideoStreamTrack()) {
      aTracks.AppendElement(track);
    }
  }
}

void DOMMediaStream::GetTracks(
    nsTArray<RefPtr<MediaStreamTrack>>& aTracks) const {
  for (const auto& track : mTracks) {
    aTracks.AppendElement(track);
  }
}

void DOMMediaStream::AddTrack(MediaStreamTrack& aTrack) {
  LOG(LogLevel::Info, ("DOMMediaStream %p Adding track %p (from track %p)",
                       this, &aTrack, aTrack.GetTrack()));

  if (HasTrack(aTrack)) {
    LOG(LogLevel::Debug,
        ("DOMMediaStream %p already contains track %p", this, &aTrack));
    return;
  }

  mTracks.AppendElement(&aTrack);

  if (!aTrack.Ended()) {
    NotifyTrackAdded(&aTrack);
  }
}

void DOMMediaStream::RemoveTrack(MediaStreamTrack& aTrack) {
  LOG(LogLevel::Info, ("DOMMediaStream %p Removing track %p (from track %p)",
                       this, &aTrack, aTrack.GetTrack()));

  if (!mTracks.RemoveElement(&aTrack)) {
    LOG(LogLevel::Debug,
        ("DOMMediaStream %p does not contain track %p", this, &aTrack));
    return;
  }

  if (!aTrack.Ended()) {
    NotifyTrackRemoved(&aTrack);
  }
}

already_AddRefed<DOMMediaStream> DOMMediaStream::Clone() {
  auto newStream = MakeRefPtr<DOMMediaStream>(GetOwner());

  LOG(LogLevel::Info,
      ("DOMMediaStream %p created clone %p", this, newStream.get()));

  for (const auto& track : mTracks) {
    LOG(LogLevel::Debug,
        ("DOMMediaStream %p forwarding external track %p to clone %p", this,
         track.get(), newStream.get()));
    RefPtr<MediaStreamTrack> clone = track->Clone();
    newStream->AddTrack(*clone);
  }

  return newStream.forget();
}

bool DOMMediaStream::Active() const { return mActive; }
bool DOMMediaStream::Audible() const { return mAudible; }

MediaStreamTrack* DOMMediaStream::GetTrackById(const nsAString& aId) const {
  for (const auto& track : mTracks) {
    nsString id;
    track->GetId(id);
    if (id == aId) {
      return track;
    }
  }
  return nullptr;
}

bool DOMMediaStream::HasTrack(const MediaStreamTrack& aTrack) const {
  return mTracks.Contains(&aTrack);
}

void DOMMediaStream::AddTrackInternal(MediaStreamTrack* aTrack) {
  LOG(LogLevel::Debug,
      ("DOMMediaStream %p Adding owned track %p", this, aTrack));
  AddTrack(*aTrack);
  DispatchTrackEvent(u"addtrack"_ns, aTrack);
}

void DOMMediaStream::RemoveTrackInternal(MediaStreamTrack* aTrack) {
  LOG(LogLevel::Debug,
      ("DOMMediaStream %p Removing owned track %p", this, aTrack));
  if (!HasTrack(*aTrack)) {
    return;
  }
  RemoveTrack(*aTrack);
  DispatchTrackEvent(u"removetrack"_ns, aTrack);
}

already_AddRefed<nsIPrincipal> DOMMediaStream::GetPrincipal() {
  if (!GetOwner()) {
    return nullptr;
  }
  nsCOMPtr<nsIPrincipal> principal =
      nsGlobalWindowInner::Cast(GetOwner())->GetPrincipal();
  for (const auto& t : mTracks) {
    if (t->Ended()) {
      continue;
    }
    nsContentUtils::CombineResourcePrincipals(&principal, t->GetPrincipal());
  }
  return principal.forget();
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

void DOMMediaStream::NotifyAudible() {
  LOG(LogLevel::Info, ("DOMMediaStream %p NotifyAudible(). ", this));

  MOZ_ASSERT(mAudible);
  for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
    mTrackListeners[i]->NotifyAudible();
  }
}

void DOMMediaStream::NotifyInaudible() {
  LOG(LogLevel::Info, ("DOMMediaStream %p NotifyInaudible(). ", this));

  MOZ_ASSERT(!mAudible);
  for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
    mTrackListeners[i]->NotifyInaudible();
  }
}

void DOMMediaStream::RegisterTrackListener(TrackListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  mTrackListeners.AppendElement(aListener);
}

void DOMMediaStream::UnregisterTrackListener(TrackListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  mTrackListeners.RemoveElement(aListener);
}

void DOMMediaStream::NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack) {
  MOZ_ASSERT(NS_IsMainThread());

  aTrack->AddConsumer(mPlaybackTrackListener);

  for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
    mTrackListeners[i]->NotifyTrackAdded(aTrack);
  }

  if (!mActive) {
    // Check if we became active.
    if (ContainsLiveTracks(mTracks)) {
      mActive = true;
      NotifyActive();
    }
  }

  if (!mAudible) {
    // Check if we became audible.
    if (ContainsLiveAudioTracks(mTracks)) {
      mAudible = true;
      NotifyAudible();
    }
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

    for (int32_t i = mTrackListeners.Length() - 1; i >= 0; --i) {
      mTrackListeners[i]->NotifyTrackRemoved(aTrack);
    }

    if (!mActive) {
      NS_ASSERTION(false, "Shouldn't remove a live track if already inactive");
      return;
    }
  }

  if (mAudible) {
    // Check if we became inaudible.
    if (!ContainsLiveAudioTracks(mTracks)) {
      mAudible = false;
      NotifyInaudible();
    }
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
