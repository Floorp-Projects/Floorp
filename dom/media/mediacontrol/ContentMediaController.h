/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_CONTENTMEDIACONTROLLER_H_
#define DOM_MEDIA_MEDIACONTROL_CONTENTMEDIACONTROLLER_H_

#include "MediaControlKeySource.h"
#include "MediaStatusManager.h"

namespace mozilla {
namespace dom {

class BrowsingContext;

/**
 * ContentMediaControlKeyReceiver is an interface which is used to receive media
 * control key sent from the chrome process, this class MUST only be used
 * in PlaybackController.
 *
 * Each browsing context tree would only have one ContentMediaControlKeyReceiver
 * that is used to handle media control key for that browsing context tree.
 */
class ContentMediaControlKeyReceiver {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Return nullptr if the top level browsing context is no longer alive.
  static ContentMediaControlKeyReceiver* Get(BrowsingContext* aBC);

  // Use this method to handle the event from `ContentMediaAgent`.
  virtual void HandleMediaKey(MediaControlKey aKey) = 0;
};

/**
 * ContentMediaAgent is an interface which we use to (1) propoagate media
 * related information from the content process to the chrome process (2) act an
 * event source to dispatch media control key to its listeners.
 *
 * If the media would like to know the media control key, then media MUST
 * inherit from ContentMediaControlKeyReceiver, and register themselves to
 * ContentMediaAgent. Whenever media control key delivers, ContentMediaAgent
 * would notify all its receivers. In addition, whenever controlled media
 * changes its playback status or audible state, they should update their status
 * update via ContentMediaAgent.
 *
 * Each browsing context tree would only have one ContentMediaAgent that is used
 * to update controlled media status existing in that browsing context tree.
 */
class ContentMediaAgent : public IMediaInfoUpdater {
 public:
  // Return nullptr if the top level browsing context is no longer alive.
  static ContentMediaAgent* Get(BrowsingContext* aBC);

  // IMediaInfoUpdater Methods
  void NotifyMediaPlaybackChanged(uint64_t aBrowsingContextId,
                                  MediaPlaybackState aState) override;
  void NotifyMediaAudibleChanged(uint64_t aBrowsingContextId,
                                 MediaAudibleState aState) override;
  void SetIsInPictureInPictureMode(uint64_t aBrowsingContextId,
                                   bool aIsInPictureInPictureMode) override;
  void SetDeclaredPlaybackState(uint64_t aBrowsingContextId,
                                MediaSessionPlaybackState aState) override;
  void NotifySessionCreated(uint64_t aBrowsingContextId) override;
  void NotifySessionDestroyed(uint64_t aBrowsingContextId) override;
  void UpdateMetadata(uint64_t aBrowsingContextId,
                      const Maybe<MediaMetadataBase>& aMetadata) override;
  void EnableAction(uint64_t aBrowsingContextId,
                    MediaSessionAction aAction) override;
  void DisableAction(uint64_t aBrowsingContextId,
                     MediaSessionAction aAction) override;
  void NotifyMediaFullScreenState(uint64_t aBrowsingContextId,
                                  bool aIsInFullScreen) override;
  void UpdatePositionState(uint64_t aBrowsingContextId,
                           const PositionState& aState) override;

  // Use these methods to register/unregister `ContentMediaControlKeyReceiver`
  // in order to listen to media control key events.
  virtual void AddReceiver(ContentMediaControlKeyReceiver* aReceiver) = 0;
  virtual void RemoveReceiver(ContentMediaControlKeyReceiver* aReceiver) = 0;
};

/**
 * ContentMediaController has a responsibility to update the content media state
 * to MediaController that exists in the chrome process and control all media
 * within a tab. It also delivers control commands from MediaController in order
 * to control media in the content page.
 *
 * Each ContentMediaController has its own ID that is corresponding to the top
 * level browsing context ID. That means we share same the
 * ContentMediaController for those media existing in the same browsing context
 * tree and same process.
 *
 * So if the content of a page are all in the same process, then we would only
 * create one ContentMediaController. However, if the content of a page are
 * running in different processes because a page has cross-origin iframes, then
 * we would have multiple ContentMediaController at the same time, creating one
 * ContentMediaController in each process to manage media.
 */
class ContentMediaController final : public ContentMediaAgent,
                                     public ContentMediaControlKeyReceiver {
 public:
  NS_INLINE_DECL_REFCOUNTING(ContentMediaController, override)

  explicit ContentMediaController(uint64_t aId);
  // ContentMediaAgent methods
  void AddReceiver(ContentMediaControlKeyReceiver* aListener) override;
  void RemoveReceiver(ContentMediaControlKeyReceiver* aListener) override;

  // ContentMediaControlKeyReceiver method
  void HandleMediaKey(MediaControlKey aKey) override;

 private:
  ~ContentMediaController() = default;

  nsTArray<RefPtr<ContentMediaControlKeyReceiver>> mReceivers;
  uint64_t mTopLevelBrowsingContextId;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIACONTROL_CONTENTMEDIACONTROLLER_H_
