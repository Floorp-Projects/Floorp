/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSMANAGER_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSMANAGER_H_

#include "MediaControlKeysEvent.h"

namespace mozilla {
namespace dom {

/**
 * MediaControlKeysManager is used to manage a source, which can globally
 * receive media control keys event, such as play, pause. It provides methods
 * to add or remove a listener to the source in order to listen media control
 * keys event.
 */
class MediaControlKeysManager final {
 public:
  MediaControlKeysManager();
  ~MediaControlKeysManager();

  // Return false when there is no event source created, so that we are not able
  // to add or remove listener, otherwise, calling these methods should always
  // succeed.
  bool AddListener(MediaControlKeysEventListener* aListener);
  bool RemoveListener(MediaControlKeysEventListener* aListener);

 private:
  void StartMonitoringControlKeys();
  void StopMonitoringControlKeys();
  void CreateEventSource();
  RefPtr<MediaControlKeysEventSource> mEventSource;
};

}  // namespace dom
}  // namespace mozilla

#endif
