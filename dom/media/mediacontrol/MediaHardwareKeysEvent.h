/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mediahardwarekeysevent_h__
#define mozilla_dom_mediahardwarekeysevent_h__

#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

enum class MediaControlKeysEvent { ePlayPause, ePrev, eNext, eNone };

/**
 * MediaHardwareKeysEventListener is used to monitor MediaControlKeysEvent, we
 * can add it onto the MediaHardwareKeysEventSource, and then everytime when
 * the media key events occur, `OnKeyPressed` will be called so that we can do
 * related handling.
 */
class MediaHardwareKeysEventListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaHardwareKeysEventListener);
  MediaHardwareKeysEventListener() = default;

  virtual void OnKeyPressed(MediaControlKeysEvent aKeyEvent);

 protected:
  virtual ~MediaHardwareKeysEventListener() = default;
};

/**
 * MediaHardwareKeysEventSource is a base class which is used to implement
 * intercepting media hardware keys event per plaftform. When receiving media
 * key events, it would notify all listeners which have been added onto the
 * source.
 */
class MediaHardwareKeysEventSource {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaHardwareKeysEventSource);
  MediaHardwareKeysEventSource() = default;

  virtual void AddListener(MediaHardwareKeysEventListener* aListener);
  virtual void RemoveListener(MediaHardwareKeysEventListener* aListener);
  size_t GetListenersNum() const;
  void Close();

 protected:
  virtual ~MediaHardwareKeysEventSource() = default;
  nsTArray<RefPtr<MediaHardwareKeysEventListener>> mListeners;
};

}  // namespace dom
}  // namespace mozilla

#endif
