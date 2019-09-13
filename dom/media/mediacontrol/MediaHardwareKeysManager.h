/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mediahardwarekeysobservice_h__
#define mozilla_dom_mediahardwarekeysobservice_h__

#include "MediaHardwareKeysEvent.h"

namespace mozilla {
namespace dom {

/**
 * MediaHardwareKeysManager is used to create a source to intercept platform
 * level media keys event and assign a proper event listener to handle those
 * events.
 */
class MediaHardwareKeysManager final {
 public:
  MediaHardwareKeysManager();
  ~MediaHardwareKeysManager();

  void StartMonitoringHardwareKeys();
  void StopMonitoringHardwareKeys();

 private:
  void CreateEventSource();
  RefPtr<MediaHardwareKeysEventSource> mEventSource;
};

}  // namespace dom
}  // namespace mozilla

#endif
