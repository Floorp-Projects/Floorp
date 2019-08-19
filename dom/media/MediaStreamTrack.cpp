/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamTrack.h"

#include "DOMMediaStream.h"
#include "MediaStreamError.h"
#include "MediaStreamGraphImpl.h"
#include "MediaStreamListener.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Promise.h"
#include "nsContentUtils.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "systemservices/MediaUtils.h"

#ifdef LOG
#  undef LOG
#endif

static mozilla::LazyLogModule gMediaStreamTrackLog("MediaStreamTrack");
#define LOG(type, msg) MOZ_LOG(gMediaStreamTrackLog, type, msg)

using namespace mozilla::media;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaStreamTrackSource)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaStreamTrackSource)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamTrackSource)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamTrackSource)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaStreamTrackSource)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrincipal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MediaStreamTrackSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

auto MediaStreamTrackSource::ApplyConstraints(
    const dom::MediaTrackConstraints& aConstraints, CallerType aCallerType)
    -> RefPtr<ApplyConstraintsPromise> {
  return ApplyConstraintsPromise::CreateAndReject(
      MakeRefPtr<MediaMgrError>(MediaMgrError::Name::OverconstrainedError,
                                NS_LITERAL_STRING("")),
      __func__);
}

/**
 * MSGListener monitors state changes of the media flowing through the
 * MediaStreamGraph.
 *
 *
 * For changes to PrincipalHandle the following applies:
 *
 * When the main thread principal for a MediaStreamTrack changes, its principal
 * will be set to the combination of the previous principal and the new one.
 *
 * As a PrincipalHandle change later happens on the MediaStreamGraph thread, we
 * will be notified. If the latest principal on main thread matches the
 * PrincipalHandle we just saw on MSG thread, we will set the track's principal
 * to the new one.
 *
 * We know at this point that the old principal has been flushed out and data
 * under it cannot leak to consumers.
 *
 * In case of multiple changes to the main thread state, the track's principal
 * will be a combination of its old principal and all the new ones until the
 * latest main thread principal matches the PrincipalHandle on the MSG thread.
 */
class MediaStreamTrack::MSGListener : public MediaStreamTrackListener {
 public:
  explicit MSGListener(MediaStreamTrack* aTrack)
      : mGraph(aTrack->GraphImpl()), mTrack(aTrack) {
    MOZ_ASSERT(mGraph);
  }

  void DoNotifyPrincipalHandleChanged(
      const PrincipalHandle& aNewPrincipalHandle) {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mTrack) {
      return;
    }

    mTrack->NotifyPrincipalHandleChanged(aNewPrincipalHandle);
  }

  void NotifyPrincipalHandleChanged(
      MediaStreamGraph* aGraph,
      const PrincipalHandle& aNewPrincipalHandle) override {
    mGraph->DispatchToMainThreadStableState(
        NewRunnableMethod<StoreCopyPassByConstLRef<PrincipalHandle>>(
            "dom::MediaStreamTrack::MSGListener::"
            "DoNotifyPrincipalHandleChanged",
            this, &MSGListener::DoNotifyPrincipalHandleChanged,
            aNewPrincipalHandle));
  }

  void NotifyRemoved() override {
    // `mTrack` is a WeakPtr and must be destroyed on main thread.
    // We dispatch ourselves to main thread here in case the MediaStreamGraph
    // is holding the last reference to us.
    mGraph->DispatchToMainThreadStableState(
        NS_NewRunnableFunction("MediaStreamTrack::MSGListener::mTrackReleaser",
                               [self = RefPtr<MSGListener>(this)]() {}));
  }

  void DoNotifyEnded() {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mTrack) {
      return;
    }

    mGraph->AbstractMainThread()->Dispatch(
        NewRunnableMethod("MediaStreamTrack::OverrideEnded", mTrack.get(),
                          &MediaStreamTrack::OverrideEnded));
  }

  void NotifyEnded() override {
    mGraph->DispatchToMainThreadStableState(
        NewRunnableMethod("MediaStreamTrack::MSGListener::DoNotifyEnded", this,
                          &MSGListener::DoNotifyEnded));
  }

 protected:
  const RefPtr<MediaStreamGraphImpl> mGraph;

  // Main thread only.
  WeakPtr<MediaStreamTrack> mTrack;
};

class MediaStreamTrack::TrackSink : public MediaStreamTrackSource::Sink {
 public:
  explicit TrackSink(MediaStreamTrack* aTrack) : mTrack(aTrack) {}

  /**
   * Keep the track source alive. This track and any clones are controlling the
   * lifetime of the source by being registered as its sinks.
   */
  bool KeepsSourceAlive() const override { return true; }

  bool Enabled() const override {
    if (!mTrack) {
      return false;
    }
    return mTrack->Enabled();
  }

  void PrincipalChanged() override {
    if (mTrack) {
      mTrack->PrincipalChanged();
    }
  }

  void MutedChanged(bool aNewState) override {
    if (mTrack) {
      mTrack->MutedChanged(aNewState);
    }
  }

  void OverrideEnded() override {
    if (mTrack) {
      mTrack->OverrideEnded();
    }
  }

 private:
  WeakPtr<MediaStreamTrack> mTrack;
};

MediaStreamTrack::MediaStreamTrack(nsPIDOMWindowInner* aWindow,
                                   MediaStream* aInputStream, TrackID aTrackID,
                                   MediaStreamTrackSource* aSource,
                                   MediaStreamTrackState aReadyState,
                                   const MediaTrackConstraints& aConstraints)
    : mWindow(aWindow),
      mInputStream(aInputStream),
      mStream(aReadyState == MediaStreamTrackState::Live
                  ? mInputStream->Graph()->CreateTrackUnionStream()
                  : nullptr),
      mPort(mStream ? mStream->AllocateInputPort(mInputStream) : nullptr),
      mTrackID(aTrackID),
      mSource(aSource),
      mSink(MakeUnique<TrackSink>(this)),
      mPrincipal(aSource->GetPrincipal()),
      mReadyState(aReadyState),
      mEnabled(true),
      mMuted(false),
      mConstraints(aConstraints) {
  if (!Ended()) {
    MOZ_DIAGNOSTIC_ASSERT(!mInputStream->IsDestroyed());
    GetSource().RegisterSink(mSink.get());

    mMSGListener = new MSGListener(this);
    AddListener(mMSGListener);
  }

  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);

  nsID uuid;
  memset(&uuid, 0, sizeof(uuid));
  if (uuidgen) {
    uuidgen->GenerateUUIDInPlace(&uuid);
  }

  char chars[NSID_LENGTH];
  uuid.ToProvidedString(chars);
  mID = NS_ConvertASCIItoUTF16(chars);
}

MediaStreamTrack::~MediaStreamTrack() { Destroy(); }

void MediaStreamTrack::Destroy() {
  SetReadyState(MediaStreamTrackState::Ended);
  // Remove all listeners -- avoid iterating over the list we're removing from
  const nsTArray<RefPtr<MediaStreamTrackListener>> trackListeners(
      mTrackListeners);
  for (auto listener : trackListeners) {
    RemoveListener(listener);
  }
  // Do the same as above for direct listeners
  const nsTArray<RefPtr<DirectMediaStreamTrackListener>> directTrackListeners(
      mDirectTrackListeners);
  for (auto listener : directTrackListeners) {
    RemoveDirectListener(listener);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamTrack)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaStreamTrack,
                                                DOMEventTargetHelper)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingPrincipal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaStreamTrack,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingPrincipal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject* MediaStreamTrack::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return MediaStreamTrack_Binding::Wrap(aCx, this, aGivenProto);
}

void MediaStreamTrack::GetId(nsAString& aID) const { aID = mID; }

void MediaStreamTrack::SetEnabled(bool aEnabled) {
  LOG(LogLevel::Info,
      ("MediaStreamTrack %p %s", this, aEnabled ? "Enabled" : "Disabled"));

  if (mEnabled == aEnabled) {
    return;
  }

  mEnabled = aEnabled;

  if (Ended()) {
    return;
  }

  mStream->SetTrackEnabled(mTrackID, mEnabled
                                         ? DisabledTrackMode::ENABLED
                                         : DisabledTrackMode::SILENCE_BLACK);
  GetSource().SinkEnabledStateChanged();
}

void MediaStreamTrack::Stop() {
  LOG(LogLevel::Info, ("MediaStreamTrack %p Stop()", this));

  if (Ended()) {
    LOG(LogLevel::Warning, ("MediaStreamTrack %p Already ended", this));
    return;
  }

  SetReadyState(MediaStreamTrackState::Ended);

  NotifyEnded();
}

void MediaStreamTrack::GetConstraints(dom::MediaTrackConstraints& aResult) {
  aResult = mConstraints;
}

void MediaStreamTrack::GetSettings(dom::MediaTrackSettings& aResult,
                                   CallerType aCallerType) {
  GetSource().GetSettings(aResult);

  // Spoof values when privacy.resistFingerprinting is true.
  if (!nsContentUtils::ResistFingerprinting(aCallerType)) {
    return;
  }
  if (aResult.mFacingMode.WasPassed()) {
    aResult.mFacingMode.Value().Assign(NS_ConvertASCIItoUTF16(
        VideoFacingModeEnumValues::strings[uint8_t(VideoFacingModeEnum::User)]
            .value));
  }
}

already_AddRefed<Promise> MediaStreamTrack::ApplyConstraints(
    const MediaTrackConstraints& aConstraints, CallerType aCallerType,
    ErrorResult& aRv) {
  if (MOZ_LOG_TEST(gMediaStreamTrackLog, LogLevel::Info)) {
    nsString str;
    aConstraints.ToJSON(str);

    LOG(LogLevel::Info, ("MediaStreamTrack %p ApplyConstraints() with "
                         "constraints %s",
                         this, NS_ConvertUTF16toUTF8(str).get()));
  }

  nsIGlobalObject* go = mWindow ? mWindow->AsGlobal() : nullptr;

  RefPtr<Promise> promise = Promise::Create(go, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Forward constraints to the source.
  //
  // After GetSource().ApplyConstraints succeeds (after it's been to
  // media-thread and back), and no sooner, do we set mConstraints to the newly
  // applied values.

  // Keep a reference to this, to make sure it's still here when we get back.
  RefPtr<MediaStreamTrack> self(this);
  GetSource()
      .ApplyConstraints(aConstraints, aCallerType)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [this, self, promise, aConstraints](bool aDummy) {
            if (!mWindow || !mWindow->IsCurrentInnerWindow()) {
              return;  // Leave Promise pending after navigation by design.
            }
            mConstraints = aConstraints;
            promise->MaybeResolve(false);
          },
          [this, self, promise](const RefPtr<MediaMgrError>& aError) {
            if (!mWindow || !mWindow->IsCurrentInnerWindow()) {
              return;  // Leave Promise pending after navigation by design.
            }
            promise->MaybeReject(
                MakeRefPtr<MediaStreamError>(mWindow, *aError));
          });
  return promise.forget();
}

ProcessedMediaStream* MediaStreamTrack::GetStream() const {
  MOZ_DIAGNOSTIC_ASSERT(!Ended());
  return mStream;
}

MediaStreamGraph* MediaStreamTrack::Graph() const {
  MOZ_DIAGNOSTIC_ASSERT(!Ended());
  return mStream->Graph();
}

MediaStreamGraphImpl* MediaStreamTrack::GraphImpl() const {
  MOZ_DIAGNOSTIC_ASSERT(!Ended());
  return mStream->GraphImpl();
}

void MediaStreamTrack::SetPrincipal(nsIPrincipal* aPrincipal) {
  if (aPrincipal == mPrincipal) {
    return;
  }
  mPrincipal = aPrincipal;

  LOG(LogLevel::Info,
      ("MediaStreamTrack %p principal changed to %p. Now: "
       "null=%d, codebase=%d, expanded=%d, system=%d",
       this, mPrincipal.get(), mPrincipal->GetIsNullPrincipal(),
       mPrincipal->GetIsContentPrincipal(),
       mPrincipal->GetIsExpandedPrincipal(), mPrincipal->IsSystemPrincipal()));
  for (PrincipalChangeObserver<MediaStreamTrack>* observer :
       mPrincipalChangeObservers) {
    observer->PrincipalChanged(this);
  }
}

void MediaStreamTrack::PrincipalChanged() {
  mPendingPrincipal = GetSource().GetPrincipal();
  nsCOMPtr<nsIPrincipal> newPrincipal = mPrincipal;
  LOG(LogLevel::Info, ("MediaStreamTrack %p Principal changed on main thread "
                       "to %p (pending). Combining with existing principal %p.",
                       this, mPendingPrincipal.get(), mPrincipal.get()));
  if (nsContentUtils::CombineResourcePrincipals(&newPrincipal,
                                                mPendingPrincipal)) {
    SetPrincipal(newPrincipal);
  }
}

void MediaStreamTrack::NotifyPrincipalHandleChanged(
    const PrincipalHandle& aNewPrincipalHandle) {
  PrincipalHandle handle(aNewPrincipalHandle);
  LOG(LogLevel::Info, ("MediaStreamTrack %p principalHandle changed on "
                       "MediaStreamGraph thread to %p. Current principal: %p, "
                       "pending: %p",
                       this, GetPrincipalFromHandle(handle), mPrincipal.get(),
                       mPendingPrincipal.get()));
  if (PrincipalHandleMatches(handle, mPendingPrincipal)) {
    SetPrincipal(mPendingPrincipal);
    mPendingPrincipal = nullptr;
  }
}

void MediaStreamTrack::MutedChanged(bool aNewState) {
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * 4.3.1 Life-cycle and Media flow - Media flow
   * To set a track's muted state to newState, the User Agent MUST run the
   * following steps:
   *  1. Let track be the MediaStreamTrack in question.
   *  2. Set track's muted attribute to newState.
   *  3. If newState is true let eventName be mute, otherwise unmute.
   *  4. Fire a simple event named eventName on track.
   */

  if (mMuted == aNewState) {
    return;
  }

  LOG(LogLevel::Info,
      ("MediaStreamTrack %p became %s", this, aNewState ? "muted" : "unmuted"));

  mMuted = aNewState;
  nsString eventName =
      aNewState ? NS_LITERAL_STRING("mute") : NS_LITERAL_STRING("unmute");
  DispatchTrustedEvent(eventName);
}

void MediaStreamTrack::NotifyEnded() {
  MOZ_ASSERT(mReadyState == MediaStreamTrackState::Ended);

  auto consumers(mConsumers);
  for (const auto& consumer : consumers) {
    if (consumer) {
      consumer->NotifyEnded(this);
    } else {
      MOZ_ASSERT_UNREACHABLE("A consumer was not explicitly removed");
      mConsumers.RemoveElement(consumer);
    }
  }
}

bool MediaStreamTrack::AddPrincipalChangeObserver(
    PrincipalChangeObserver<MediaStreamTrack>* aObserver) {
  return mPrincipalChangeObservers.AppendElement(aObserver) != nullptr;
}

bool MediaStreamTrack::RemovePrincipalChangeObserver(
    PrincipalChangeObserver<MediaStreamTrack>* aObserver) {
  return mPrincipalChangeObservers.RemoveElement(aObserver);
}

void MediaStreamTrack::AddConsumer(MediaStreamTrackConsumer* aConsumer) {
  MOZ_ASSERT(!mConsumers.Contains(aConsumer));
  mConsumers.AppendElement(aConsumer);

  // Remove destroyed consumers for cleanliness
  while (mConsumers.RemoveElement(nullptr)) {
    MOZ_ASSERT_UNREACHABLE("A consumer was not explicitly removed");
  }
}

void MediaStreamTrack::RemoveConsumer(MediaStreamTrackConsumer* aConsumer) {
  mConsumers.RemoveElement(aConsumer);

  // Remove destroyed consumers for cleanliness
  while (mConsumers.RemoveElement(nullptr)) {
    MOZ_ASSERT_UNREACHABLE("A consumer was not explicitly removed");
  }
}

already_AddRefed<MediaStreamTrack> MediaStreamTrack::Clone() {
  RefPtr<MediaStreamTrack> newTrack = CloneInternal();
  newTrack->SetEnabled(Enabled());
  newTrack->SetMuted(Muted());
  return newTrack.forget();
}

void MediaStreamTrack::SetReadyState(MediaStreamTrackState aState) {
  MOZ_ASSERT(!(mReadyState == MediaStreamTrackState::Ended &&
               aState == MediaStreamTrackState::Live),
             "We don't support overriding the ready state from ended to live");

  if (Ended()) {
    return;
  }

  if (mReadyState == MediaStreamTrackState::Live &&
      aState == MediaStreamTrackState::Ended) {
    if (mSource) {
      mSource->UnregisterSink(mSink.get());
    }
    if (mMSGListener) {
      RemoveListener(mMSGListener);
    }
    if (mPort) {
      mPort->Destroy();
    }
    if (mStream) {
      mStream->Destroy();
    }
    mPort = nullptr;
    mStream = nullptr;
    mMSGListener = nullptr;
  }

  mReadyState = aState;
}

void MediaStreamTrack::OverrideEnded() {
  MOZ_ASSERT(NS_IsMainThread());

  if (Ended()) {
    return;
  }

  LOG(LogLevel::Info, ("MediaStreamTrack %p ended", this));

  SetReadyState(MediaStreamTrackState::Ended);

  NotifyEnded();

  DispatchTrustedEvent(NS_LITERAL_STRING("ended"));
}

void MediaStreamTrack::AddListener(MediaStreamTrackListener* aListener) {
  LOG(LogLevel::Debug,
      ("MediaStreamTrack %p adding listener %p", this, aListener));
  mTrackListeners.AppendElement(aListener);

  if (Ended()) {
    return;
  }
  mStream->AddTrackListener(aListener, mTrackID);
}

void MediaStreamTrack::RemoveListener(MediaStreamTrackListener* aListener) {
  LOG(LogLevel::Debug,
      ("MediaStreamTrack %p removing listener %p", this, aListener));
  mTrackListeners.RemoveElement(aListener);

  if (Ended()) {
    return;
  }
  mStream->RemoveTrackListener(aListener, mTrackID);
}

void MediaStreamTrack::AddDirectListener(
    DirectMediaStreamTrackListener* aListener) {
  LOG(LogLevel::Debug, ("MediaStreamTrack %p (%s) adding direct listener %p to "
                        "stream %p, track %d",
                        this, AsAudioStreamTrack() ? "audio" : "video",
                        aListener, mStream.get(), mTrackID));
  mDirectTrackListeners.AppendElement(aListener);

  if (Ended()) {
    return;
  }
  mStream->AddDirectTrackListener(aListener, mTrackID);
}

void MediaStreamTrack::RemoveDirectListener(
    DirectMediaStreamTrackListener* aListener) {
  LOG(LogLevel::Debug,
      ("MediaStreamTrack %p removing direct listener %p from stream %p", this,
       aListener, mStream.get()));
  mDirectTrackListeners.RemoveElement(aListener);

  if (Ended()) {
    return;
  }
  mStream->RemoveDirectTrackListener(aListener, mTrackID);
}

already_AddRefed<MediaInputPort> MediaStreamTrack::ForwardTrackContentsTo(
    ProcessedMediaStream* aStream) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(aStream);
  MOZ_DIAGNOSTIC_ASSERT(!Ended());
  return aStream->AllocateInputPort(mStream, mTrackID, mTrackID);
}

}  // namespace dom
}  // namespace mozilla
