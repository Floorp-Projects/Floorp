/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamTrack.h"

#include "DOMMediaStream.h"
#include "MediaSegment.h"
#include "MediaStreamError.h"
#include "MediaTrackGraphImpl.h"
#include "MediaTrackListener.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Promise.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "systemservices/MediaUtils.h"

#ifdef LOG
#  undef LOG
#endif

static mozilla::LazyLogModule gMediaStreamTrackLog("MediaStreamTrack");
#define LOG(type, msg) MOZ_LOG(gMediaStreamTrackLog, type, msg)

using namespace mozilla::media;

namespace mozilla::dom {

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
      MakeRefPtr<MediaMgrError>(MediaMgrError::Name::OverconstrainedError, ""),
      __func__);
}

/**
 * MTGListener monitors state changes of the media flowing through the
 * MediaTrackGraph.
 *
 *
 * For changes to PrincipalHandle the following applies:
 *
 * When the main thread principal for a MediaStreamTrack changes, its principal
 * will be set to the combination of the previous principal and the new one.
 *
 * As a PrincipalHandle change later happens on the MediaTrackGraph thread, we
 * will be notified. If the latest principal on main thread matches the
 * PrincipalHandle we just saw on MTG thread, we will set the track's principal
 * to the new one.
 *
 * We know at this point that the old principal has been flushed out and data
 * under it cannot leak to consumers.
 *
 * In case of multiple changes to the main thread state, the track's principal
 * will be a combination of its old principal and all the new ones until the
 * latest main thread principal matches the PrincipalHandle on the MTG thread.
 */
class MediaStreamTrack::MTGListener : public MediaTrackListener {
 public:
  explicit MTGListener(MediaStreamTrack* aTrack) : mTrack(aTrack) {}

  void DoNotifyPrincipalHandleChanged(
      const PrincipalHandle& aNewPrincipalHandle) {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mTrack) {
      return;
    }

    mTrack->NotifyPrincipalHandleChanged(aNewPrincipalHandle);
  }

  void NotifyPrincipalHandleChanged(
      MediaTrackGraph* aGraph,
      const PrincipalHandle& aNewPrincipalHandle) override {
    aGraph->DispatchToMainThreadStableState(
        NewRunnableMethod<StoreCopyPassByConstLRef<PrincipalHandle>>(
            "dom::MediaStreamTrack::MTGListener::"
            "DoNotifyPrincipalHandleChanged",
            this, &MTGListener::DoNotifyPrincipalHandleChanged,
            aNewPrincipalHandle));
  }

  void NotifyRemoved(MediaTrackGraph* aGraph) override {
    // `mTrack` is a WeakPtr and must be destroyed on main thread.
    // We dispatch ourselves to main thread here in case the MediaTrackGraph
    // is holding the last reference to us.
    aGraph->DispatchToMainThreadStableState(
        NS_NewRunnableFunction("MediaStreamTrack::MTGListener::mTrackReleaser",
                               [self = RefPtr<MTGListener>(this)]() {}));
  }

  void DoNotifyEnded() {
    MOZ_ASSERT(NS_IsMainThread());

    if (!mTrack) {
      return;
    }

    if (!mTrack->GetParentObject()) {
      return;
    }

    AbstractThread* mainThread =
        nsGlobalWindowInner::Cast(mTrack->GetParentObject())
            ->AbstractMainThreadFor(TaskCategory::Other);
    mainThread->Dispatch(NewRunnableMethod("MediaStreamTrack::OverrideEnded",
                                           mTrack.get(),
                                           &MediaStreamTrack::OverrideEnded));
  }

  void NotifyEnded(MediaTrackGraph* aGraph) override {
    aGraph->DispatchToMainThreadStableState(
        NewRunnableMethod("MediaStreamTrack::MTGListener::DoNotifyEnded", this,
                          &MTGListener::DoNotifyEnded));
  }

 protected:
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
                                   mozilla::MediaTrack* aInputTrack,
                                   MediaStreamTrackSource* aSource,
                                   MediaStreamTrackState aReadyState,
                                   bool aMuted,
                                   const MediaTrackConstraints& aConstraints)
    : mWindow(aWindow),
      mInputTrack(aInputTrack),
      mSource(aSource),
      mSink(MakeUnique<TrackSink>(this)),
      mPrincipal(aSource->GetPrincipal()),
      mReadyState(aReadyState),
      mEnabled(true),
      mMuted(aMuted),
      mConstraints(aConstraints) {
  if (!Ended()) {
    GetSource().RegisterSink(mSink.get());

    // Even if the input track is destroyed we need mTrack so that methods
    // like AddListener still work. Keeping the number of paths to a minimum
    // also helps prevent bugs elsewhere. We'll be ended through the
    // MediaStreamTrackSource soon enough.
    auto graph = mInputTrack->IsDestroyed()
                     ? MediaTrackGraph::GetInstanceIfExists(
                           mWindow, mInputTrack->mSampleRate,
                           MediaTrackGraph::DEFAULT_OUTPUT_DEVICE)
                     : mInputTrack->Graph();
    MOZ_DIAGNOSTIC_ASSERT(graph,
                          "A destroyed input track is only expected when "
                          "cloning, but since we're live there must be another "
                          "live track that is keeping the graph alive");

    mTrack = graph->CreateForwardedInputTrack(mInputTrack->mType);
    mPort = mTrack->AllocateInputPort(mInputTrack);
    mMTGListener = new MTGListener(this);
    AddListener(mMTGListener);
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
  for (const auto& listener : mTrackListeners.Clone()) {
    RemoveListener(listener);
  }
  // Do the same as above for direct listeners
  for (const auto& listener : mDirectTrackListeners.Clone()) {
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
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

  mTrack->SetDisabledTrackMode(mEnabled ? DisabledTrackMode::ENABLED
                                        : DisabledTrackMode::SILENCE_BLACK);
  NotifyEnabledChanged();
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
    aResult.mFacingMode.Value().AssignASCII(
        VideoFacingModeEnumValues::GetString(VideoFacingModeEnum::User));
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
          GetCurrentSerialEventTarget(), __func__,
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

ProcessedMediaTrack* MediaStreamTrack::GetTrack() const {
  MOZ_DIAGNOSTIC_ASSERT(!Ended());
  return mTrack;
}

MediaTrackGraph* MediaStreamTrack::Graph() const {
  MOZ_DIAGNOSTIC_ASSERT(!Ended());
  return mTrack->Graph();
}

MediaTrackGraphImpl* MediaStreamTrack::GraphImpl() const {
  MOZ_DIAGNOSTIC_ASSERT(!Ended());
  return mTrack->GraphImpl();
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
                       "MediaTrackGraph thread to %p. Current principal: %p, "
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

  if (Ended()) {
    return;
  }

  nsString eventName = aNewState ? u"mute"_ns : u"unmute"_ns;
  DispatchTrustedEvent(eventName);
}

void MediaStreamTrack::NotifyEnded() {
  MOZ_ASSERT(mReadyState == MediaStreamTrackState::Ended);

  for (const auto& consumer : mConsumers.Clone()) {
    if (consumer) {
      consumer->NotifyEnded(this);
    } else {
      MOZ_ASSERT_UNREACHABLE("A consumer was not explicitly removed");
      mConsumers.RemoveElement(consumer);
    }
  }
}

void MediaStreamTrack::NotifyEnabledChanged() {
  GetSource().SinkEnabledStateChanged();

  for (const auto& consumer : mConsumers.Clone()) {
    if (consumer) {
      consumer->NotifyEnabledChanged(this, Enabled());
    } else {
      MOZ_ASSERT_UNREACHABLE("A consumer was not explicitly removed");
      mConsumers.RemoveElement(consumer);
    }
  }
}

bool MediaStreamTrack::AddPrincipalChangeObserver(
    PrincipalChangeObserver<MediaStreamTrack>* aObserver) {
  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  mPrincipalChangeObservers.AppendElement(aObserver);
  return true;
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
    if (mMTGListener) {
      RemoveListener(mMTGListener);
    }
    if (mPort) {
      mPort->Destroy();
    }
    if (mTrack) {
      mTrack->Destroy();
    }
    mPort = nullptr;
    mTrack = nullptr;
    mMTGListener = nullptr;
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

  DispatchTrustedEvent(u"ended"_ns);
}

void MediaStreamTrack::AddListener(MediaTrackListener* aListener) {
  LOG(LogLevel::Debug,
      ("MediaStreamTrack %p adding listener %p", this, aListener));
  mTrackListeners.AppendElement(aListener);

  if (Ended()) {
    return;
  }
  mTrack->AddListener(aListener);
}

void MediaStreamTrack::RemoveListener(MediaTrackListener* aListener) {
  LOG(LogLevel::Debug,
      ("MediaStreamTrack %p removing listener %p", this, aListener));
  mTrackListeners.RemoveElement(aListener);

  if (Ended()) {
    return;
  }
  mTrack->RemoveListener(aListener);
}

void MediaStreamTrack::AddDirectListener(DirectMediaTrackListener* aListener) {
  LOG(LogLevel::Debug, ("MediaStreamTrack %p (%s) adding direct listener %p to "
                        "track %p",
                        this, AsAudioStreamTrack() ? "audio" : "video",
                        aListener, mTrack.get()));
  mDirectTrackListeners.AppendElement(aListener);

  if (Ended()) {
    return;
  }
  mTrack->AddDirectListener(aListener);
}

void MediaStreamTrack::RemoveDirectListener(
    DirectMediaTrackListener* aListener) {
  LOG(LogLevel::Debug,
      ("MediaStreamTrack %p removing direct listener %p from track %p", this,
       aListener, mTrack.get()));
  mDirectTrackListeners.RemoveElement(aListener);

  if (Ended()) {
    return;
  }
  mTrack->RemoveDirectListener(aListener);
}

already_AddRefed<MediaInputPort> MediaStreamTrack::ForwardTrackContentsTo(
    ProcessedMediaTrack* aTrack) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(aTrack);
  return aTrack->AllocateInputPort(mTrack);
}

}  // namespace mozilla::dom
