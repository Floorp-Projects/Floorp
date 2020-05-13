/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIASESSION_MEDIASESSIONCONTROLLER_H_
#define DOM_MEDIA_MEDIASESSION_MEDIASESSIONCONTROLLER_H_

#include "mozilla/dom/MediaMetadata.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "mozilla/Maybe.h"
#include "nsDataHashtable.h"
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

  // These attributes are all propagated from the media session in the content
  // process.
  Maybe<MediaMetadataBase> mMetadata;
  MediaSessionPlaybackState mDeclaredPlaybackState =
      MediaSessionPlaybackState::None;
};

/**
 * IMediaInfoUpdater is an interface which provides methods to update the media
 * related information that happens in the content process.
 */
class IMediaInfoUpdater {
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Use this method to update controlled media's playback state and the
  // browsing context where controlled media exists.
  virtual void NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                          MediaPlaybackState aState) = 0;

  // Use this method to update controlled media's audible state and the
  // browsing context where controlled media exists.
  virtual void NotifyMediaAudibleChanged(uint64_t aBrowsingContextId,
                                         MediaAudibleState aState) = 0;
};

/**
 * MediaSessionController is used to track all alive media sessions within a tab
 * and store their metadata which could be used to show on the virtual media
 * control interface. In addition, we can use it to get the current media
 * metadata even if there is no media session existing.
 *
 * When media session is created in the content process, we would notify
 * MediaSessionController in the parent process to tell it in which browsing
 * context media session is created. If there are multiple media sessions
 * existing in the same tab, MediaSessionController would take a resbonsibility
 * to decide which media session should be an active media session. However,
 * the meaning of active media session here is not equal to the definition from
 * the spec [1]. We just choose the session which is the active one inside the
 * tab, the global active media session among different tabs would be selected
 * in other place, which is related to how we select media controller.
 *
 * [1] https://w3c.github.io/mediasession/#active-media-session
 */
class MediaSessionController : public IMediaInfoUpdater {
 public:
  explicit MediaSessionController(uint64_t aBrowsingContextId);

  // IMediaInfoUpdater's methods
  void NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                  MediaPlaybackState aState) override;
  void NotifyMediaAudibleChanged(uint64_t aBrowsingContextId,
                                 MediaAudibleState aState) override;

  // Use these functions to ensure that we can track all existing media session
  // in the same tab when media session is created or destroyed in the content
  // process.
  void NotifySessionCreated(uint64_t aSessionContextId);
  void NotifySessionDestroyed(uint64_t aSessionContextId);

  // Use this function to store the media metadata when media session updated
  // its metadata in the content process.
  void UpdateMetadata(uint64_t aSessionContextId,
                      const Maybe<MediaMetadataBase>& aMetadata);

  // Return active media session's metadata if active media session exists and
  // it has already set its metadata. Otherwise, return default media metadata
  // which is based on website's title and favicon.
  MediaMetadataBase GetCurrentMediaMetadata() const;
  uint64_t Id() const { return mTopLevelBCId; }

  bool IsMediaAudible() const;
  bool IsMediaPlaying() const;
  bool IsAnyMediaBeingControlled() const;

  MediaEventSource<MediaMetadataBase>& MetadataChangedEvent() {
    return mMetadataChangedEvent;
  }

  // This method is used to update the declared playback state for the specific
  // media session.
  virtual void SetDeclaredPlaybackState(uint64_t aSessionContextId,
                                        MediaSessionPlaybackState aState);

  // Return the active media session's declared playback state. If we don't have
  // active media session, we would return 'None'.
  MediaSessionPlaybackState GetCurrentDeclaredPlaybackState() const;

 protected:
  ~MediaSessionController() = default;
  uint64_t mTopLevelBCId;
  Maybe<uint64_t> mActiveMediaSessionContextId;

 private:
  nsString GetDefaultFaviconURL() const;
  nsString GetDefaultTitle() const;
  MediaMetadataBase CreateDefaultMetadata() const;
  bool IsInPrivateBrowsing() const;
  void FillMissingTitleAndArtworkIfNeeded(MediaMetadataBase& aMetadata) const;

  void UpdateActiveMediaSessionContextId();

  nsDataHashtable<nsUint64HashKey, MediaSessionInfo> mMediaSessionInfoMap;
  MediaEventProducer<MediaMetadataBase> mMetadataChangedEvent;
  MediaPlaybackStatus mMediaStatusDelegate;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIASESSION_MEDIASESSIONCONTROLLER_H_
