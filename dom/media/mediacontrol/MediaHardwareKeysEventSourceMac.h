/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_MEDIAHARDWAREKEYSEVENTSOURCEMAC_H_
#define DOM_MEDIA_MEDIACONTROL_MEDIAHARDWAREKEYSEVENTSOURCEMAC_H_

#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include "MediaControlKeysEvent.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class MediaHardwareKeysEventSourceMac final
    : public MediaControlKeysEventSource {
 public:
  MediaHardwareKeysEventSourceMac();
  ~MediaHardwareKeysEventSourceMac();

  static CGEventRef EventTapCallback(CGEventTapProxy proxy, CGEventType type,
                                     CGEventRef event, void* refcon);

 private:
  void StartEventTap();
  void StopEventTap();

  // They are used to intercept mac hardware media keys.
  CFMachPortRef mEventTap = nullptr;
  CFRunLoopSourceRef mEventTapSource = nullptr;
};

}  // namespace dom
}  // namespace mozilla

#endif
