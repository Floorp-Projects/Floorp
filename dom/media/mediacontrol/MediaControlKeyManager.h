/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYMANAGER_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYMANAGER_H_

#include "MediaControlKeySource.h"
#include "MediaEventSource.h"
#include "nsIObserver.h"

namespace mozilla::dom {

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

  MediaControlKeyManager();

  // MediaControlKeySource methods
  bool Open() override;
  void Close() override;
  bool IsOpened() const override;

  void SetPlaybackState(MediaSessionPlaybackState aState) override;
  MediaSessionPlaybackState GetPlaybackState() const override;

  // MediaControlKeyListener methods
  void OnActionPerformed(const MediaControlAction& aAction) override;

  void SetMediaMetadata(const MediaMetadataBase& aMetadata) override;
  void SetSupportedMediaKeys(const MediaKeysArray& aSupportedKeys) override;
  void SetEnableFullScreen(bool aIsEnabled) override;
  void SetEnablePictureInPictureMode(bool aIsEnabled) override;
  void SetPositionState(const PositionState& aState) override;

 private:
  ~MediaControlKeyManager();
  void Shutdown();

  class Observer final : public nsIObserver {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    explicit Observer(MediaControlKeyManager* aManager);

   protected:
    virtual ~Observer() = default;

    MediaControlKeyManager* MOZ_OWNING_REF mManager;
  };
  RefPtr<Observer> mObserver;
  void OnPreferenceChange();

  bool StartMonitoringControlKeys();
  void StopMonitoringControlKeys();
  RefPtr<MediaControlKeySource> mEventSource;
  MediaMetadataBase mMetadata;
  nsTArray<MediaControlKey> mSupportedKeys;
};

}  // namespace mozilla::dom

#endif
