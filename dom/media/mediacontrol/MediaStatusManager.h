/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIASTATUSMANAGER_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIASTATUSMANAGER_H_

#include "MediaControlKeySource.h"
#include "MediaEventSource.h"
#include "MediaPlaybackStatus.h"
#include "mozilla/dom/MediaMetadata.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "mozilla/Maybe.h"
#include "nsTHashMap.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class MediaSessionInfo {
 public:
  MediaSessionInfo() = default;

  explicit MediaSessionInfo(MediaMetadataBase& aMetadata) {
    mMetadata.emplace(aMetadata);
  }

  MediaSessionInfo(MediaMetadataBase& aMetadata,
                   MediaSessionPlaybackState& aState) {
    mMetadata.emplace(aMetadata);
    mDeclaredPlaybackState = aState;
  }

  static MediaSessionInfo EmptyInfo() { return MediaSessionInfo(); }

  static uint32_t GetActionBitMask(MediaSessionAction aAction) {
    return 1 << static_cast<uint8_t>(aAction);
  }

  void EnableAction(MediaSessionAction aAction) {
    mSupportedActions |= GetActionBitMask(aAction);
  }

  void DisableAction(MediaSessionAction aAction) {
    mSupportedActions &= ~GetActionBitMask(aAction);
  }

  bool IsActionSupported(MediaSessionAction aAction) const {
    return mSupportedActions & GetActionBitMask(aAction);
  }

  // These attributes are all propagated from the media session in the content
  // process.
  Maybe<MediaMetadataBase> mMetadata;
  MediaSessionPlaybackState mDeclaredPlaybackState =
      MediaSessionPlaybackState::None;
  // Use bitwise to store the supported actions.
  uint32_t mSupportedActions = 0;
};

/**
 * IMediaInfoUpdater is an interface which provides methods to update the media
 * related information that happens in the content process.
 */
class IMediaInfoUpdater {
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Use this method to update controlled media's playback state and the
  // browsing context where controlled media exists. When notifying the state
  // change, we MUST follow the following rules.
  // (1) `eStart` MUST be the first state and `eStop` MUST be the last state
  // (2) Do not notify same state again
  // (3) `ePaused` can only be notified after notifying `ePlayed`.
  virtual void NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                          MediaPlaybackState aState) = 0;

  // Use this method to update the audible state of controlled media, and MUST
  // follow the following rules in which `audible` and `inaudible` should be a
  // pair. `inaudible` should always be notified after `audible`. When audible
  // media paused, `inaudible` should be notified
  // Eg. (O) `audible` -> `inaudible` -> `audible` -> `inaudible`
  //     (X) `inaudible` -> `audible`    [notify `inaudible` before `audible`]
  //     (X) `audible` -> `audible`      [notify `audible` twice]
  //     (X) `audible` -> (media pauses) [forgot to notify `inaudible`]
  virtual void NotifyMediaAudibleChanged(uint64_t aBrowsingContextId,
                                         MediaAudibleState aState) = 0;

  // Use this method to update media session's declared playback state for the
  // specific media session.
  virtual void SetDeclaredPlaybackState(uint64_t aBrowsingContextId,
                                        MediaSessionPlaybackState aState) = 0;

  // Use these methods to update controller's media session list. We'd use it
  // when media session is created/destroyed in the content process.
  virtual void NotifySessionCreated(uint64_t aBrowsingContextId) = 0;
  virtual void NotifySessionDestroyed(uint64_t aBrowsingContextId) = 0;

  // Use this method to update the metadata for the specific media session.
  virtual void UpdateMetadata(uint64_t aBrowsingContextId,
                              const Maybe<MediaMetadataBase>& aMetadata) = 0;

  // Use this method to update the picture in picture mode state of controlled
  // media, and it's safe to notify same state again.
  virtual void SetIsInPictureInPictureMode(uint64_t aBrowsingContextId,
                                           bool aIsInPictureInPictureMode) = 0;

  // Use these methods to update the supported media session action for the
  // specific media session. For a media session from a given browsing context,
  // do not re-enable the same action, or disable the action without enabling it
  // before.
  virtual void EnableAction(uint64_t aBrowsingContextId,
                            MediaSessionAction aAction) = 0;
  virtual void DisableAction(uint64_t aBrowsingContextId,
                             MediaSessionAction aAction) = 0;

  // Use this method when media enters or leaves the fullscreen.
  virtual void NotifyMediaFullScreenState(uint64_t aBrowsingContextId,
                                          bool aIsInFullScreen) = 0;

  // Use this method when media session update its position state.
  virtual void UpdatePositionState(uint64_t aBrowsingContextId,
                                   const PositionState& aState) = 0;
};

/**
 * MediaStatusManager would decide the media related status which can represents
 * the whole tab. The status includes the playback status, tab's metadata and
 * the active media session ID if it exists.
 *
 * We would use `IMediaInfoUpdater` methods to update the media playback related
 * information and then use `MediaPlaybackStatus` to determine the final
 * playback state.
 *
 * The metadata would be the one from the active media session, or the default
 * one. This class would determine which media session is an active media
 * session [1] whithin a tab. It tracks all alive media sessions within a tab
 * and store their metadata which could be used to show on the virtual media
 * control interface. In addition, we can use it to get the current media
 * metadata even if there is no media session existing. However, the meaning of
 * active media session here is not equal to the definition from the spec [1].
 * We just choose the session which is the active one inside the tab, the global
 * active media session among different tabs would be the one inside the main
 * controller which is determined by MediaControlService.
 *
 * [1] https://w3c.github.io/mediasession/#active-media-session
 */
class MediaStatusManager : public IMediaInfoUpdater {
 public:
  explicit MediaStatusManager(uint64_t aBrowsingContextId);

  // IMediaInfoUpdater's methods
  void NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                  MediaPlaybackState aState) override;
  void NotifyMediaAudibleChanged(uint64_t aBrowsingContextId,
                                 MediaAudibleState aState) override;
  void SetDeclaredPlaybackState(uint64_t aSessionContextId,
                                MediaSessionPlaybackState aState) override;
  void NotifySessionCreated(uint64_t aSessionContextId) override;
  void NotifySessionDestroyed(uint64_t aSessionContextId) override;
  void UpdateMetadata(uint64_t aSessionContextId,
                      const Maybe<MediaMetadataBase>& aMetadata) override;
  void EnableAction(uint64_t aBrowsingContextId,
                    MediaSessionAction aAction) override;
  void DisableAction(uint64_t aBrowsingContextId,
                     MediaSessionAction aAction) override;
  void UpdatePositionState(uint64_t aBrowsingContextId,
                           const PositionState& aState) override;

  // Return active media session's metadata if active media session exists and
  // it has already set its metadata. Otherwise, return default media metadata
  // which is based on website's title and favicon.
  MediaMetadataBase GetCurrentMediaMetadata() const;

  bool IsMediaAudible() const;
  bool IsMediaPlaying() const;
  bool IsAnyMediaBeingControlled() const;

  // These events would be notified when the active media session's certain
  // property changes.
  MediaEventSource<MediaMetadataBase>& MetadataChangedEvent() {
    return mMetadataChangedEvent;
  }

  MediaEventSource<PositionState>& PositionChangedEvent() {
    return mPositionStateChangedEvent;
  }

  MediaEventSource<MediaSessionPlaybackState>& PlaybackChangedEvent() {
    return mPlaybackStateChangedEvent;
  }

  // Return the actual playback state.
  MediaSessionPlaybackState PlaybackState() const;

  // When page title changes, we might need to update it on the default
  // metadata as well.
  void NotifyPageTitleChanged();

 protected:
  ~MediaStatusManager() = default;

  // This event would be notified when the active media session changes its
  // supported actions.
  MediaEventSource<nsTArray<MediaSessionAction>>&
  SupportedActionsChangedEvent() {
    return mSupportedActionsChangedEvent;
  }

  uint64_t mTopLevelBrowsingContextId;

  // Within a tab, the Id of the browsing context which has already created a
  // media session and owns the audio focus within a tab.
  Maybe<uint64_t> mActiveMediaSessionContextId;

 private:
  nsString GetDefaultFaviconURL() const;
  nsString GetDefaultTitle() const;
  MediaMetadataBase CreateDefaultMetadata() const;
  bool IsInPrivateBrowsing() const;
  void FillMissingTitleAndArtworkIfNeeded(MediaMetadataBase& aMetadata) const;

  bool IsSessionOwningAudioFocus(uint64_t aBrowsingContextId) const;
  void SetActiveMediaSessionContextId(uint64_t aBrowsingContextId);
  void ClearActiveMediaSessionContextIdIfNeeded();
  void HandleAudioFocusOwnerChanged(Maybe<uint64_t>& aBrowsingContextId);

  void NotifySupportedKeysChangedIfNeeded(uint64_t aBrowsingContextId);

  // Return a copyable array filled with the supported media session actions.
  // Use copyable array so that we can use the result as a parameter for the
  // media event.
  CopyableTArray<MediaSessionAction> GetSupportedActions() const;

  void StoreMediaSessionContextIdOnWindowContext();

  // When the amount of playing media changes, we would use this function to
  // update the guessed playback state.
  void SetGuessedPlayState(MediaSessionPlaybackState aState);

  // Whenever the declared playback state or the guessed playback state changes,
  // we should recompute actual playback state to know if we need to update the
  // virtual control interface.
  void UpdateActualPlaybackState();

  // Return the active media session's declared playback state. If the active
  // media session doesn't exist, return  'None' instead.
  MediaSessionPlaybackState GetCurrentDeclaredPlaybackState() const;

  // This state can match to the `guessed playback state` in the spec [1], it
  // indicates if we have any media element playing within the tab which this
  // controller belongs to. But currently we only take media elements into
  // account, which is different from the way the spec recommends. In addition,
  // We don't support web audio and plugin and not consider audible state of
  // media.
  // [1] https://w3c.github.io/mediasession/#guessed-playback-state
  MediaSessionPlaybackState mGuessedPlaybackState =
      MediaSessionPlaybackState::None;

  // This playback state would be the final playback which can be used to know
  // if the controller is playing or not.
  // https://w3c.github.io/mediasession/#actual-playback-state
  MediaSessionPlaybackState mActualPlaybackState =
      MediaSessionPlaybackState::None;

  nsTHashMap<nsUint64HashKey, MediaSessionInfo> mMediaSessionInfoMap;
  MediaEventProducer<MediaMetadataBase> mMetadataChangedEvent;
  MediaEventProducer<nsTArray<MediaSessionAction>>
      mSupportedActionsChangedEvent;
  MediaEventProducer<PositionState> mPositionStateChangedEvent;
  MediaEventProducer<MediaSessionPlaybackState> mPlaybackStateChangedEvent;
  MediaPlaybackStatus mPlaybackStatusDelegate;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIACONTROL_MEDIASTATUSMANAGER_H_
