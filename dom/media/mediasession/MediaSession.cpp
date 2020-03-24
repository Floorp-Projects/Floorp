/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaSession.h"
#include "mozilla/EnumeratedArrayCycleCollection.h"

#include "MediaSessionUtils.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaSession, mParent, mMediaMetadata,
                                      mActionHandlers)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaSession)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaSession)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaSession)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MediaSession::MediaSession(nsPIDOMWindowInner* aParent) : mParent(aParent) {
  MOZ_ASSERT(mParent);
  NotifyMediaSessionStatus(SessionStatus::eCreated);
}

nsPIDOMWindowInner* MediaSession::GetParentObject() const { return mParent; }

JSObject* MediaSession::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return MediaSession_Binding::Wrap(aCx, this, aGivenProto);
}

MediaMetadata* MediaSession::GetMetadata() const { return mMediaMetadata; }

void MediaSession::SetMetadata(MediaMetadata* aMetadata) {
  mMediaMetadata = aMetadata;
  NotifyMetadataUpdated();
}

void MediaSession::SetPlaybackState(
    const MediaSessionPlaybackState& aPlaybackState) {
  if (mDeclaredPlaybackState == aPlaybackState) {
    return;
  }
  mDeclaredPlaybackState = aPlaybackState;

  RefPtr<BrowsingContext> currentBC = GetParentObject()->GetBrowsingContext();
  MOZ_ASSERT(currentBC,
             "Update session playback state after context destroyed!");
  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyMediaSessionPlaybackStateChanged(
        currentBC, mDeclaredPlaybackState);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<MediaController> controller =
          currentBC->Canonical()->GetMediaController()) {
    controller->SetDeclaredPlaybackState(currentBC->Id(),
                                         mDeclaredPlaybackState);
  }
}

MediaSessionPlaybackState MediaSession::PlaybackState() const {
  return mDeclaredPlaybackState;
}

void MediaSession::SetActionHandler(MediaSessionAction aAction,
                                    MediaSessionActionHandler* aHandler) {
  MOZ_ASSERT(size_t(aAction) < ArrayLength(mActionHandlers));
  mActionHandlers[aAction] = aHandler;
}

MediaSessionActionHandler* MediaSession::GetActionHandler(
    MediaSessionAction aAction) const {
  MOZ_ASSERT(size_t(aAction) < ArrayLength(mActionHandlers));
  return mActionHandlers[aAction];
}

void MediaSession::SetPositionState(const MediaPositionState& aState,
                                    ErrorResult& aRv) {
  // https://w3c.github.io/mediasession/#dom-mediasession-setpositionstate
  // If the state is an empty dictionary then clear the position state.
  if (!aState.IsAnyMemberPresent()) {
    mPositionState.reset();
    return;
  }

  // If the duration is not present, throw a TypeError.
  if (!aState.mDuration.WasPassed()) {
    return aRv.ThrowTypeError("Duration is not present");
  }

  // If the duration is negative, throw a TypeError.
  if (aState.mDuration.WasPassed() && aState.mDuration.Value() < 0.0) {
    return aRv.ThrowTypeError(nsPrintfCString(
        "Invalid duration %f, it can't be negative", aState.mDuration.Value()));
  }

  // If the position is negative or greater than duration, throw a TypeError.
  if (aState.mPosition.WasPassed() &&
      (aState.mPosition.Value() < 0.0 ||
       aState.mPosition.Value() > aState.mDuration.Value())) {
    return aRv.ThrowTypeError(nsPrintfCString(
        "Invalid position %f, it can't be negative or greater than duration",
        aState.mPosition.Value()));
  }

  // If the playbackRate is zero, throw a TypeError.
  if (aState.mPlaybackRate.WasPassed() && aState.mPlaybackRate.Value() == 0.0) {
    return aRv.ThrowTypeError("The playbackRate is zero");
  }

  // If the position is not present, set it to zero.
  double position = aState.mPosition.WasPassed() ? aState.mPosition.Value() : 0;

  // If the playbackRate is not present, set it to 1.0.
  double playbackRate =
      aState.mPlaybackRate.WasPassed() ? aState.mPlaybackRate.Value() : 1.0;

  // Update the position state and last position updated time.
  MOZ_ASSERT(aState.mDuration.WasPassed());
  mPositionState =
      Some(PositionState(aState.mDuration.Value(), playbackRate, position));
  // TODO : propagate position state to the media controller in the chrome
  // process.
}

void MediaSession::NotifyHandler(const MediaSessionActionDetails& aDetails) {
  DispatchNotifyHandler(aDetails);
}

void MediaSession::NotifyHandler(MediaSessionAction aAction) {
  MediaSessionActionDetails details;
  details.mAction = aAction;
  DispatchNotifyHandler(details);
}

void MediaSession::DispatchNotifyHandler(
    const MediaSessionActionDetails& aDetails) {
  class Runnable final : public mozilla::Runnable {
   public:
    Runnable(const MediaSession* aSession,
             const MediaSessionActionDetails& aDetails)
        : mozilla::Runnable("MediaSession::DispatchNotifyHandler"),
          mSession(aSession),
          mDetails(aDetails) {}

    MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
      if (RefPtr<MediaSessionActionHandler> handler =
              mSession->GetActionHandler(mDetails.mAction)) {
        handler->Call(mDetails);
      }
      return NS_OK;
    }

   private:
    RefPtr<const MediaSession> mSession;
    MediaSessionActionDetails mDetails;
  };

  RefPtr<nsIRunnable> runnable = new Runnable(this, aDetails);
  NS_DispatchToMainThread(runnable);
}

bool MediaSession::IsSupportedAction(MediaSessionAction aAction) const {
  MOZ_ASSERT(size_t(aAction) < ArrayLength(mActionHandlers));
  return mActionHandlers[aAction] != nullptr;
}

void MediaSession::Shutdown() {
  NotifyMediaSessionStatus(SessionStatus::eDestroyed);
}

void MediaSession::NotifyMediaSessionStatus(SessionStatus aState) {
  RefPtr<BrowsingContext> currentBC = GetParentObject()->GetBrowsingContext();
  MOZ_ASSERT(currentBC, "Update session status after context destroyed!");
  NotfiyMediaSessionCreationOrDeconstruction(currentBC,
                                             aState == SessionStatus::eCreated);
}

void MediaSession::NotifyMetadataUpdated() {
  RefPtr<BrowsingContext> currentBC = GetParentObject()->GetBrowsingContext();
  MOZ_ASSERT(currentBC, "Update session metadata after context destroyed!");
  Maybe<MediaMetadataBase> metadata;
  if (GetMetadata()) {
    metadata.emplace(*(GetMetadata()->AsMetadataBase()));
  }

  if (XRE_IsContentProcess()) {
    ContentChild* contentChild = ContentChild::GetSingleton();
    Unused << contentChild->SendNotifyUpdateMediaMetadata(currentBC, metadata);
    return;
  }
  // This would only happen when we disable e10s.
  if (RefPtr<MediaController> controller =
          currentBC->Canonical()->GetMediaController()) {
    controller->UpdateMetadata(currentBC->Id(), metadata);
  }
}

}  // namespace dom
}  // namespace mozilla
