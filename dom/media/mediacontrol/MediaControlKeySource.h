/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSOURCE_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSOURCE_H_

#include "mozilla/dom/MediaControllerBinding.h"
#include "mozilla/dom/MediaMetadata.h"
#include "mozilla/dom/MediaSession.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

/**
 * MediaControlKeyListener is a pure interface, which is used to monitor
 * MediaControlKey, we can add it onto the MediaControlKeySource,
 * and then everytime when the media key events occur, `OnKeyPressed` will be
 * called so that we can do related handling.
 */
class MediaControlKeyListener {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
  MediaControlKeyListener() = default;

  virtual void OnKeyPressed(MediaControlKey aKey) = 0;

 protected:
  virtual ~MediaControlKeyListener() = default;
};

/**
 * MediaControlKeyHandler is used to operate media controller by corresponding
 * received media control key events.
 */
class MediaControlKeyHandler final : public MediaControlKeyListener {
 public:
  NS_INLINE_DECL_REFCOUNTING(MediaControlKeyHandler, override)
  void OnKeyPressed(MediaControlKey aKey) override;

 private:
  virtual ~MediaControlKeyHandler() = default;
};

/**
 * MediaControlKeySource is an abstract class which is used to implement
 * transporting media control keys event to all its listeners when media keys
 * event happens.
 */
class MediaControlKeySource {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
  MediaControlKeySource();

  using MediaKeysArray = nsTArray<MediaControlKey>;

  virtual void AddListener(MediaControlKeyListener* aListener);
  virtual void RemoveListener(MediaControlKeyListener* aListener);
  size_t GetListenersNum() const;

  // Return true if the initialization of the source succeeds, and inherited
  // sources should implement this method to handle the initialization fails.
  virtual bool Open() = 0;
  virtual void Close();
  virtual bool IsOpened() const = 0;

  /**
   * All following `SetXXX()` functions are used to update the playback related
   * properties change from a specific tab, which can represent the playback
   * status for Firefox instance. Even if we have multiple tabs playing media at
   * the same time, we would only update information from one of that tabs that
   * would be done by `MediaControlService`.
   */

  // Currently, this method is only useful for GeckoView. It would be called
  // before all the other `SetXXX()` functions after the key source is open.
  // It's used to indicate where the property change occurs. We would send the
  // top level browsing context Id in order to allow the key source know which
  // tab that the following coming information belong to. This function would be
  // called whenever we change the controlled tab. If no controlled tab exists,
  // the `aTopLevelBrowsingContextId` would be `Nothing()`.
  virtual void SetControlledTabBrowsingContextId(
      Maybe<uint64_t> aTopLevelBrowsingContextId){};

  virtual void SetPlaybackState(MediaSessionPlaybackState aState);
  virtual MediaSessionPlaybackState GetPlaybackState() const;

  // Override this method if the event source needs to handle the metadata.
  virtual void SetMediaMetadata(const MediaMetadataBase& aMetadata) {}

  // Set the supported media keys which the event source can use to determine
  // what kinds of buttons should be shown on the UI.
  virtual void SetSupportedMediaKeys(const MediaKeysArray& aSupportedKeys) = 0;

  // Override these methods if the inherited key source want to know the change
  // for following attributes. For example, GeckoView would use these methods
  // to notify change to the embedded application.
  virtual void SetEnableFullScreen(bool aIsEnabled){};
  virtual void SetEnablePictureInPictureMode(bool aIsEnabled){};
  virtual void SetPositionState(const PositionState& aState){};

 protected:
  virtual ~MediaControlKeySource() = default;
  nsTArray<RefPtr<MediaControlKeyListener>> mListeners;
  MediaSessionPlaybackState mPlaybackState;
};

}  // namespace dom
}  // namespace mozilla

#endif
