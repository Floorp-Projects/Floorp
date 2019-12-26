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

  MediaController* GetOrCreateControllerById(const uint64_t aId) const;
  MediaController* GetControllerById(const uint64_t aId) const;
  AudioFocusManager& GetAudioFocusManager() { return mAudioFocusManager; }
  MediaControlKeysEventSource* GetMediaControlKeysEventSource() {
    return mMediaControlKeysManager;
  }

  void AddMediaController(MediaController* aController);
  void RemoveMediaController(MediaController* aController);
  uint64_t GetControllersNum() const;

  MediaController* GetLastAddedController() const;

  // This event is used to generate a media event indicating media controller
  // amount changed.
  MediaEventSource<uint64_t>& MediaControllerAmountChangedEvent() {
    return mMediaControllerAmountChangedEvent;
  }

  // This is used for testing only, to generate fake media control keys events.
  void GenerateMediaControlKeysTestEvent(MediaControlKeysEvent aEvent);

 private:
  MediaControlService();
  ~MediaControlService();

  void Init();
  void Shutdown();

  void PlayAllControllers() const;
  void PauseAllControllers() const;
  void StopAllControllers() const;
  void ShutdownAllControllers() const;

  nsDataHashtable<nsUint64HashKey, RefPtr<MediaController>> mControllers;
  nsTArray<uint64_t> mControllerHistory;
  AudioFocusManager mAudioFocusManager;
  RefPtr<MediaControlKeysManager> mMediaControlKeysManager;
  RefPtr<MediaControlKeysEventListener> mMediaKeysHandler;
  MediaEventProducer<uint64_t> mMediaControllerAmountChangedEvent;
};

}  // namespace dom
}  // namespace mozilla

#endif
