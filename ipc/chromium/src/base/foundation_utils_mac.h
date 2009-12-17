// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FOUNDATION_UTILS_MAC_H_
#define BASE_FOUNDATION_UTILS_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

// CFTypeRefToNSObjectAutorelease transfers ownership of a Core Foundation
// object (one derived from CFTypeRef) to the Foundation memory management
// system.  In a traditional managed-memory environment, cf_object is
// autoreleased and returned as an NSObject.  In a garbage-collected
// environment, cf_object is marked as eligible for garbage collection.
//
// This function should only be used to convert a concrete CFTypeRef type to
// its equivalent "toll-free bridged" NSObject subclass, for example,
// converting a CFStringRef to NSString.
//
// By calling this function, callers relinquish any ownership claim to
// cf_object.  In a managed-memory environment, the object's ownership will be
// managed by the innermost NSAutoreleasePool, so after this function returns,
// callers should not assume that cf_object is valid any longer than the
// returned NSObject.
static inline id CFTypeRefToNSObjectAutorelease(CFTypeRef cf_object) {
  // When GC is on, NSMakeCollectable marks cf_object for GC and autorelease
  // is a no-op.
  //
  // In the traditional GC-less environment, NSMakeCollectable is a no-op,
  // and cf_object is autoreleased, balancing out the caller's ownership claim.
  //
  // NSMakeCollectable returns nil when used on a NULL object.
  return [NSMakeCollectable(cf_object) autorelease];
}

#endif  // BASE_FOUNDATION_UTILS_MAC_H_
