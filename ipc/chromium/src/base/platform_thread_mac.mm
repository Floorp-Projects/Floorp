// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_thread.h"

#import <Foundation/Foundation.h>
#include <dlfcn.h>

#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"

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
  // this is called early in startup, before the event loop, so provide
  // an autorelease pool to prevent leaks here
  ScopedNSAutoreleasePool pool;

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

// static
void PlatformThread::SetName(const char* name) {
  // pthread_setname_np is only available in 10.6 or later, so test
  // for it at runtime.
  int (*dynamic_pthread_setname_np)(const char*);
  *reinterpret_cast<void**>(&dynamic_pthread_setname_np) =
      dlsym(RTLD_DEFAULT, "pthread_setname_np");
  if (!dynamic_pthread_setname_np) return;

  // Mac OS X does not expose the length limit of the name, so
  // hardcode it.
  const int kMaxNameLength = 63;
  std::string shortened_name = std::string(name).substr(0, kMaxNameLength);
  // pthread_setname() fails (harmlessly) in the sandbox, ignore when it does.
  // See http://crbug.com/47058

  // The name parameter is copied thus it's safe to release it after calling.
  // Checked against the bionic implementation in bionic/libc/bionic/pthread.c
  dynamic_pthread_setname_np(shortened_name.c_str());
}
