/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSMANAGER_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIACONTROLKEYSMANAGER_H_

#include "MediaControlKeysEvent.h"

namespace mozilla {
namespace dom {

/**
 * MediaControlKeysManager is used to create a source to intercept platform
 * level media keys event and assign a proper event listener to handle those
 * events.
 */
class MediaControlKeysManager final {
 public:
  MediaControlKeysManager();
  ~MediaControlKeysManager();

  void StartMonitoringControlKeys();
  void StopMonitoringControlKeys();

 private:
  void CreateEventSource();
  RefPtr<MediaControlKeysEventSource> mEventSource;
};

}  // namespace dom
}  // namespace mozilla

#endif
