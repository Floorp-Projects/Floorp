/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mediahardwarekeyseventmac_h__
#define mozilla_dom_mediahardwarekeyseventmac_h__

#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include "MediaHardwareKeysEvent.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class MediaHardwareKeysEventSourceMac final
    : public MediaHardwareKeysEventSource {
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
