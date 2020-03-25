/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLSERVICE_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLSERVICE_H_

#include "mozilla/AlreadyAddRefed.h"

#include "AudioFocusManager.h"
#include "MediaController.h"
#include "MediaControlKeysManager.h"
#include "MediaEventSource.h"
#include "nsDataHashtable.h"
#include "nsIObserver.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

/**
 * MediaControlService is an interface to access controllers by providing
 * controller Id. Everytime when controller becomes active, which means there is
 * one or more media started in the corresponding browsing context, so now the
 * controller is actually controlling something in the content process, so it
 * would be added into the list of the MediaControlService. The controller would
 * be removed from the list of the MediaControlService when it becomes inactive,
 * which means no media is playing in the corresponding browsing context. Note
 * that, a controller can't be added to or remove from the list twice. It should
 * should have a responsibility to add and remove itself in the proper time.
 */
class MediaControlService final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static RefPtr<MediaControlService> GetService();

  AudioFocusManager& GetAudioFocusManager() { return mAudioFocusManager; }
  MediaControlKeysEventSource* GetMediaControlKeysEventSource() {
    return mMediaControlKeysManager;
  }

  // Use these functions to register/unresgister controller to/from the active
  // controller list in the service. Return true if the controller is registered
  // or unregistered sucessfully.
  bool RegisterActiveMediaController(MediaController* aController);
  bool UnregisterActiveMediaController(MediaController* aController);
  uint64_t GetActiveControllersNum() const;

  // This method would be called when the controller changes its playback state.
  void NotifyControllerPlaybackStateChanged(MediaController* aController);

  // The main controller is the controller which can receive the media control
  // key events and would show its metadata to virtual controller interface.
  MediaController* GetMainController() const;

  // This event is used to generate a media event indicating media controller
  // amount changed.
  MediaEventSource<uint64_t>& MediaControllerAmountChangedEvent() {
    return mMediaControllerAmountChangedEvent;
  }

  /**
   * These following functions are used for testing only. We use them to
   * generate fake media control key events, get the media metadata and playback
   * state from the main controller.
   */
  void GenerateMediaControlKeysTestEvent(MediaControlKeysEvent aEvent);
  MediaMetadataBase GetMainControllerMediaMetadata() const;
  MediaSessionPlaybackState GetMainControllerPlaybackState() const;

 private:
  MediaControlService();
  ~MediaControlService();

  /**
   * When there are multiple media controllers existing, we would only choose
   * one media controller as the main controller which can be controlled by
   * media control keys event. The latest controller which is added into the
   * service would become the main controller.
   *
   * However, as the main controller would be changed from time to time, so we
   * create this wrapper to hold a real main controller if it exists. This class
   * would also observe the playback state of controller in order to update the
   * playback state of the event source.
   *
   * In addition, after finishing bug1592037, we would get the media metadata
   * from the main controller, and update them to the event source in order to
   * show those information on the virtual media controller interface on each
   * platform.
   */
  class ControllerManager final {
   public:
    explicit ControllerManager(MediaControlService* aService);
    ~ControllerManager() = default;

    bool AddController(MediaController* aController);
    bool RemoveController(MediaController* aController);
    void UpdateMainController(MediaController* aController);

    void Shutdown();

    MediaController* GetMainController() const;
    MediaController* GetControllerById(uint64_t aId) const;
    bool Contains(MediaController* aController) const;
    uint64_t GetControllersNum() const;

    // These functions are used for monitoring main controller's status change.
    void MainControllerPlaybackStateChanged(MediaSessionPlaybackState aState);
    void MainControllerMetadataChanged(const MediaMetadataBase& aMetadata);

   private:
    void UpdateMainControllerInternal(MediaController* aController);
    void ConnectToMainControllerEvents();
    void DisconnectMainControllerEvents();

    LinkedList<RefPtr<MediaController>> mControllers;
    RefPtr<MediaController> mMainController;

    // These member are use to listen main controller's play state changes and
    // update the playback state to the event source.
    RefPtr<MediaControlKeysEventSource> mSource;
    MediaEventListener mPlayStateChangedListener;
    MediaEventListener mMetadataChangedListener;
  };

  void Init();
  void Shutdown();

  AudioFocusManager mAudioFocusManager;
  RefPtr<MediaControlKeysManager> mMediaControlKeysManager;
  RefPtr<MediaControlKeysEventListener> mMediaKeysHandler;
  MediaEventProducer<uint64_t> mMediaControllerAmountChangedEvent;
  UniquePtr<ControllerManager> mControllerManager;
};

}  // namespace dom
}  // namespace mozilla

#endif
