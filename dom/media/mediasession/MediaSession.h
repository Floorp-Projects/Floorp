/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaSession_h
#define mozilla_dom_MediaSession_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/MediaMetadata.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EnumeratedArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

// https://w3c.github.io/mediasession/#position-state
struct PositionState {
  PositionState(double aDuration, double aPlaybackRate,
                double aLastReportedTime)
      : mDuration(aDuration),
        mPlaybackRate(aPlaybackRate),
        mLastReportedPlaybackPosition(aLastReportedTime) {}
  double mDuration;
  double mPlaybackRate;
  double mLastReportedPlaybackPosition;
};

class MediaSession final : public nsISupports, public nsWrapperCache {
 public:
  // Ref counting and cycle collection
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaSession)

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

  // Use these methods to trigger media session action handler asynchronously.
  void NotifyHandler(const MediaSessionActionDetails& aDetails);
  void NotifyHandler(MediaSessionAction aAction);

  void Shutdown();

 private:
  // Propagate media context status to the media session controller in the
  // chrome process when we create or destroy the media session.
  enum class SessionStatus : bool {
    eDestroyed = false,
    eCreated = true,
  };
  void NotifyMediaSessionStatus(SessionStatus aState);

  void NotifyMetadataUpdated();

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
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MediaSession_h
