// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CHROME_APPLICATION_MAC_H_
#define BASE_CHROME_APPLICATION_MAC_H_

#import <AppKit/AppKit.h>

#include "base/basictypes.h"
#include "base/scoped_nsobject.h"

// Event hooks must implement this protocol.
@protocol CrApplicationEventHookProtocol
- (void)hookForEvent:(NSEvent*)theEvent;
@end


@interface CrApplication : NSApplication {
 @private
  BOOL handlingSendEvent_;
 // Array of objects implementing the CrApplicationEventHookProtocol
  scoped_nsobject<NSMutableArray> eventHooks_;
}
@property(readonly,
          getter=isHandlingSendEvent,
          nonatomic) BOOL handlingSendEvent;

// Add or remove an event hook to be called for every sendEvent:
// that the application receives.  These handlers are called before
// the normal [NSApplication sendEvent:] call is made.

// This is not a good alternative to a nested event loop.  It should
// be used only when normal event logic and notification breaks down
// (e.g. when clicking outside a canBecomeKey:NO window to "switch
// context" out of it).
- (void)addEventHook:(id<CrApplicationEventHookProtocol>)hook;
- (void)removeEventHook:(id<CrApplicationEventHookProtocol>)hook;

+ (NSApplication*)sharedApplication;
@end

namespace chrome_application_mac {

// Controls the state of |handlingSendEvent_| in the event loop so that it is
// reset properly.
class ScopedSendingEvent {
 public:
  ScopedSendingEvent();
  ~ScopedSendingEvent();

 private:
  CrApplication* app_;
  BOOL handling_;
  DISALLOW_COPY_AND_ASSIGN(ScopedSendingEvent);
};

}  // chrome_application_mac

#endif  // BASE_CHROME_APPLICATION_MAC_H_
