/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYMANAGER_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYMANAGER_H_

#include "MediaControlKeySource.h"
#include "MediaEventSource.h"

namespace mozilla {
namespace dom {

/**
 * MediaControlKeyManager is a wrapper of MediaControlKeySource, which
 * is used to manage creating and destroying a real media keys event source.
 *
 * It monitors the amount of the media controller in MediaService, and would
 * create the event source when there is any existing controller and destroy it
 * when there is no controller.
 */
class MediaControlKeyManager final : public MediaControlKeySource,
                                     public MediaControlKeyListener {
 public:
  NS_INLINE_DECL_REFCOUNTING(MediaControlKeyManager, override)

  MediaControlKeyManager() = default;

  // MediaControlKeySource methods
  bool Open() override;
  bool IsOpened() const override;

  void SetControlledTabBrowsingContextId(
      Maybe<uint64_t> aTopLevelBrowsingContextId) override;

  void SetPlaybackState(MediaSessionPlaybackState aState) override;
  MediaSessionPlaybackState GetPlaybackState() const override;

  // MediaControlKeyListener methods
  void OnActionPerformed(const MediaControlAction& aAction) override;

  // The callback function for monitoring the media controller amount changed
  // event.
  void ControllerAmountChanged(uint64_t aControllerAmount);

  void SetMediaMetadata(const MediaMetadataBase& aMetadata) override;
  void SetSupportedMediaKeys(const MediaKeysArray& aSupportedKeys) override;
  void SetEnableFullScreen(bool aIsEnabled) override;
  void SetEnablePictureInPictureMode(bool aIsEnabled) override;
  void SetPositionState(const PositionState& aState) override;

 private:
  ~MediaControlKeyManager();
  void StartMonitoringControlKeys();
  void StopMonitoringControlKeys();
  RefPtr<MediaControlKeySource> mEventSource;
  MediaEventListener mControllerAmountChangedListener;
  MediaMetadataBase mMetadata;
  nsTArray<MediaControlKey> mSupportedKeys;
};

}  // namespace dom
}  // namespace mozilla

#endif
