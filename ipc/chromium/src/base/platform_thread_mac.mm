// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_thread.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"

// A simple class that demonstrates our impressive ability to do nothing.
@interface NoOp : NSObject

// Does the deed.  Or does it?
+ (void)noOp;

@end

@implementation NoOp

+ (void)noOp {
}

@end

namespace base {

// If Cocoa is to be used on more than one thread, it must know that the
// application is multithreaded.  Since it's possible to enter Cocoa code
// from threads created by pthread_thread_create, Cocoa won't necessarily
// be aware that the application is multithreaded.  Spawning an NSThread is
// enough to get Cocoa to set up for multithreaded operation, so this is done
// if necessary before pthread_thread_create spawns any threads.
//
// http://developer.apple.com/documentation/Cocoa/Conceptual/Multithreading/CreatingThreads/chapter_4_section_4.html
void InitThreading() {
  static BOOL multithreaded = [NSThread isMultiThreaded];
  if (!multithreaded) {
    [NSThread detachNewThreadSelector:@selector(noOp)
                             toTarget:[NoOp class]
                           withObject:nil];
    multithreaded = YES;

    DCHECK([NSThread isMultiThreaded]);
  }
}

}  // namespace base
