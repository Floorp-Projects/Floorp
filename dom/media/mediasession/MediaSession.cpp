/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaSession.h"

#include "MediaSessionUtils.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaSession, mParent, mMediaMetadata)
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

void MediaSession::SetActionHandler(MediaSessionAction aAction,
                                    MediaSessionActionHandler* aHandler) {
  size_t index = static_cast<size_t>(aAction);
  mActionHandlers[index] = aHandler;
}

MediaSessionActionHandler* MediaSession::GetActionHandler(
    MediaSessionAction aAction) const {
  return mActionHandlers[static_cast<size_t>(aAction)];
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
          mAction(aDetails.mAction) {}

    MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
      if (RefPtr<MediaSessionActionHandler> handler =
              mSession->GetActionHandler(mAction)) {
        MediaSessionActionDetails details;
        details.mAction = mAction;
        handler->Call(details);
      }
      return NS_OK;
    }

   private:
    RefPtr<const MediaSession> mSession;
    MediaSessionAction mAction;
  };

  RefPtr<nsIRunnable> runnable = new Runnable(this, aDetails);
  NS_DispatchToMainThread(runnable);
}

bool MediaSession::IsSupportedAction(MediaSessionAction aAction) const {
  size_t index = static_cast<size_t>(aAction);
  MOZ_ASSERT(index < ACTIONS);
  return mActionHandlers[index] != nullptr;
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
