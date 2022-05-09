/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_CONTENTMEDIACONTROLLER_H_
#define DOM_MEDIA_MEDIACONTROL_CONTENTMEDIACONTROLLER_H_

#include "MediaControlKeySource.h"
#include "MediaStatusManager.h"

namespace mozilla::dom {

class BrowsingContext;

/**
 * ContentMediaControlKeyReceiver is an interface which is used to receive media
 * control key sent from the chrome process.
 */
class ContentMediaControlKeyReceiver {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Return nullptr if the top level browsing context is no longer alive.
  static ContentMediaControlKeyReceiver* Get(BrowsingContext* aBC);

  // Use this method to handle the event from `ContentMediaAgent`.
  virtual void HandleMediaKey(MediaControlKey aKey) = 0;

  virtual bool IsPlaying() const = 0;
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
 * ContentMediaController exists in per inner window, which has a responsibility
 * to update the content media state to MediaController (ContentMediaAgent) and
 * delivers MediaControlKey to its receiver in order to control media in the
 * content page (ContentMediaControlKeyReceiver).
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

  // We don't need this method, so make it as private and simply return false.
  virtual bool IsPlaying() const override { return false; }

  void PauseOrStopMedia();

  nsTArray<RefPtr<ContentMediaControlKeyReceiver>> mReceivers;
};

}  // namespace mozilla::dom

#endif  // DOM_MEDIA_MEDIACONTROL_CONTENTMEDIACONTROLLER_H_
