/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSEVENT_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSEVENT_H_

#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

/**
 * MediaControlKeysEvent represents all possible control buttons in controller
 * interface, no matter it's physical one, such as keyboard, headset, or a
 * virtual one, such as the interface provided by Android MediaController.
 */
enum class MediaControlKeysEvent : uint32_t {
  ePlay,
  ePause,
  ePlayPause,
  ePrevTrack,
  eNextTrack,
  eSeekBackward,
  eSeekForward,
  eStop,
};

/**
 * MediaControlKeysEventListener is a pure interface, which is used to monitor
 * MediaControlKeysEvent, we can add it onto the MediaControlKeysEventSource,
 * and then everytime when the media key events occur, `OnKeyPressed` will be
 * called so that we can do related handling.
 */
class MediaControlKeysEventListener : public nsISupports {
 public:
  MediaControlKeysEventListener() = default;

  virtual void OnKeyPressed(MediaControlKeysEvent aKeyEvent) = 0;

 protected:
  virtual ~MediaControlKeysEventListener() = default;
};

/**
 * MediaControlKeysHandler is used to operate media controller by corresponding
 * received media control key events.
 */
class MediaControlKeysHandler final : public MediaControlKeysEventListener {
 public:
  NS_DECL_ISUPPORTS

  void OnKeyPressed(MediaControlKeysEvent aKeyEvent) override;

 private:
  virtual ~MediaControlKeysHandler() = default;
};

/**
 * MediaControlKeysEventSource is a base class which is used to implement
 * transporting media control keys event to all its listeners when media keys
 * event happens.
 */
class MediaControlKeysEventSource : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  MediaControlKeysEventSource() = default;

  virtual void AddListener(MediaControlKeysEventListener* aListener);
  virtual void RemoveListener(MediaControlKeysEventListener* aListener);
  size_t GetListenersNum() const;

  // Return true if the initialization of the source succeeds, and inherited
  // sources should implement this method to handle the initialization fails.
  virtual bool Open() = 0;
  virtual void Close();
  virtual bool IsOpened() const = 0;

 protected:
  virtual ~MediaControlKeysEventSource() = default;
  nsTArray<RefPtr<MediaControlKeysEventListener>> mListeners;
};

}  // namespace dom
}  // namespace mozilla

#endif
