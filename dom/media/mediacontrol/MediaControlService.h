/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLSERVICE_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLSERVICE_H_

#include "mozilla/AlreadyAddRefed.h"

#include "AudioFocusManager.h"
#include "MediaController.h"
#include "MediaControlKeyManager.h"
#include "mozilla/dom/MediaControllerBinding.h"
#include "nsIObserver.h"
#include "nsTArray.h"

namespace mozilla::dom {

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

  // Currently these following static methods are only being used in testing.
  static void GenerateMediaControlKey(const GlobalObject& global,
                                      MediaControlKey aKey);
  static void GetCurrentActiveMediaMetadata(const GlobalObject& aGlobal,
                                            MediaMetadataInit& aMetadata);
  static MediaSessionPlaybackState GetCurrentMediaSessionPlaybackState(
      GlobalObject& aGlobal);

  AudioFocusManager& GetAudioFocusManager() { return mAudioFocusManager; }
  MediaControlKeySource* GetMediaControlKeySource() {
    return mMediaControlKeyManager;
  }

  // Use these functions to register/unresgister controller to/from the active
  // controller list in the service. Return true if the controller is registered
  // or unregistered sucessfully.
  bool RegisterActiveMediaController(MediaController* aController);
  bool UnregisterActiveMediaController(MediaController* aController);
  uint64_t GetActiveControllersNum() const;

  // This method would be called when the controller changes its playback state.
  void NotifyControllerPlaybackStateChanged(MediaController* aController);

  // This method is used to help a media controller become a main controller, if
  // it fits the requirement.
  void RequestUpdateMainController(MediaController* aController);

  // The main controller is the controller which can receive the media control
  // key events and would show its metadata to virtual controller interface.
  MediaController* GetMainController() const;

  /**
   * These following functions are used for testing only. We use them to
   * generate fake media control key events, get the media metadata and playback
   * state from the main controller.
   */
  void GenerateTestMediaControlKey(MediaControlKey aKey);
  MediaMetadataBase GetMainControllerMediaMetadata() const;
  MediaSessionPlaybackState GetMainControllerPlaybackState() const;

  // Media title that should be used as a fallback. This commonly used
  // when playing media in private browsing mode and we are trying to avoid
  // exposing potentially sensitive titles.
  nsString GetFallbackTitle() const;

  // These functions are used to update the variable which would be used for
  // telemetry probe.
  void NotifyMediaControlHasEverBeenUsed();
  void NotifyMediaControlHasEverBeenEnabled();

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

    using MediaKeysArray = nsTArray<MediaControlKey>;
    using LinkedListControllerPtr = LinkedListElement<RefPtr<MediaController>>*;
    using ConstLinkedListControllerPtr =
        const LinkedListElement<RefPtr<MediaController>>*;

    bool AddController(MediaController* aController);
    bool RemoveController(MediaController* aController);
    void UpdateMainControllerIfNeeded(MediaController* aController);

    void Shutdown();

    MediaController* GetMainController() const;
    bool Contains(MediaController* aController) const;
    uint64_t GetControllersNum() const;

    // These functions are used for monitoring main controller's status change.
    void MainControllerPlaybackStateChanged(MediaSessionPlaybackState aState);
    void MainControllerMetadataChanged(const MediaMetadataBase& aMetadata);

   private:
    // When applying `eInsertAsMainController`, we would always insert the
    // element to the tail of the list. Eg. Insert C , [A, B] -> [A, B, C]
    // When applying `eInsertAsNormalController`, we would insert the element
    // prior to the element with a higher priority controller. Eg. Insert E and
    // C and D have higher priority. [A, B, C, D] -> [A, B, E, C, D]
    enum class InsertOptions {
      eInsertAsMainController,
      eInsertAsNormalController,
    };

    // Adjust the given controller's order by the insert option.
    void ReorderGivenController(MediaController* aController,
                                InsertOptions aOption);

    void UpdateMainControllerInternal(MediaController* aController);
    void ConnectMainControllerEvents();
    void DisconnectMainControllerEvents();

    LinkedList<RefPtr<MediaController>> mControllers;
    RefPtr<MediaController> mMainController;

    // These member are use to listen main controller's play state changes and
    // update the playback state to the event source.
    RefPtr<MediaControlKeySource> mSource;
    MediaEventListener mMetadataChangedListener;
    MediaEventListener mSupportedKeysChangedListener;
    MediaEventListener mFullScreenChangedListener;
    MediaEventListener mPictureInPictureModeChangedListener;
    MediaEventListener mPositionChangedListener;
  };

  void Init();
  void Shutdown();

  AudioFocusManager mAudioFocusManager;
  RefPtr<MediaControlKeyManager> mMediaControlKeyManager;
  RefPtr<MediaControlKeyListener> mMediaKeysHandler;
  MediaEventProducer<uint64_t> mMediaControllerAmountChangedEvent;
  UniquePtr<ControllerManager> mControllerManager;
  nsString mFallbackTitle;

  // Used for telemetry probe.
  void UpdateTelemetryUsageProbe();
  bool mHasEverUsedMediaControl = false;
  bool mHasEverEnabledMediaControl = false;
};

}  // namespace mozilla::dom

#endif
