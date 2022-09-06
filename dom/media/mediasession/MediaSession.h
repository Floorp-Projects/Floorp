/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaSession_h
#define mozilla_dom_MediaSession_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "mozilla/EnumeratedArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocumentActivity.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
class ErrorResult;

namespace dom {

class Document;
class MediaMetadata;

// https://w3c.github.io/mediasession/#position-state
struct PositionState {
  PositionState() = default;
  PositionState(double aDuration, double aPlaybackRate,
                double aLastReportedTime)
      : mDuration(aDuration),
        mPlaybackRate(aPlaybackRate),
        mLastReportedPlaybackPosition(aLastReportedTime) {}
  double mDuration;
  double mPlaybackRate;
  double mLastReportedPlaybackPosition;
};

class MediaSession final : public nsIDocumentActivity, public nsWrapperCache {
 public:
  // Ref counting and cycle collection
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MediaSession)
  NS_DECL_NSIDOCUMENTACTIVITY

  explicit MediaSession(nsPIDOMWindowInner* aParent);

  // WebIDL methods
  nsPIDOMWindowInner* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  MediaMetadata* GetMetadata() const;

  void SetMetadata(MediaMetadata* aMetadata);

  void SetPlaybackState(const MediaSessionPlaybackState& aPlaybackState);

  MediaSessionPlaybackState PlaybackState() const;

  void SetActionHandler(MediaSessionAction aAction,
                        MediaSessionActionHandler* aHandler);

  void SetPositionState(const MediaPositionState& aState, ErrorResult& aRv);

  bool IsSupportedAction(MediaSessionAction aAction) const;

  // Use this method to trigger media session action handler asynchronously.
  void NotifyHandler(const MediaSessionActionDetails& aDetails);

  void Shutdown();

  // `MediaStatusManager` would determine which media session is an active media
  // session and update it from the chrome process. This active session is not
  // 100% equal to the active media session in the spec, which is a globally
  // active media session *among all tabs*. The active session here is *among
  // different windows but in same tab*, so each tab can have at most one
  // active media session.
  bool IsActive() const;

 private:
  // When the document which media session belongs to is going to be destroyed,
  // or is in the bfcache, then the session would be inactive. Otherwise, it's
  // active all the time.
  enum class SessionDocStatus : bool {
    eInactive = false,
    eActive = true,
  };
  void SetMediaSessionDocStatus(SessionDocStatus aState);

  // These methods are used to propagate media session's status to the chrome
  // process.
  void NotifyMediaSessionDocStatus(SessionDocStatus aState);
  void NotifyMediaSessionAttributes();
  void NotifyPlaybackStateUpdated();
  void NotifyMetadataUpdated();
  void NotifyEnableSupportedAction(MediaSessionAction aAction);
  void NotifyDisableSupportedAction(MediaSessionAction aAction);
  void NotifyPositionStateChanged();

  void DispatchNotifyHandler(const MediaSessionActionDetails& aDetails);

  MediaSessionActionHandler* GetActionHandler(MediaSessionAction aAction) const;

  ~MediaSession() = default;

  nsCOMPtr<nsPIDOMWindowInner> mParent;

  RefPtr<MediaMetadata> mMediaMetadata;

  EnumeratedArray<MediaSessionAction, MediaSessionAction::EndGuard_,
                  RefPtr<MediaSessionActionHandler>>
      mActionHandlers;

  // This is used as is a hint for the user agent to determine whether the
  // browsing context is playing or paused.
  // https://w3c.github.io/mediasession/#declared-playback-state
  MediaSessionPlaybackState mDeclaredPlaybackState =
      MediaSessionPlaybackState::None;

  Maybe<PositionState> mPositionState;
  RefPtr<Document> mDoc;
  SessionDocStatus mSessionDocState = SessionDocStatus::eInactive;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MediaSession_h
