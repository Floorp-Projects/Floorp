/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

@interface MOXWebAreaAccessible : mozAccessible
// overrides
- (NSURL* _Nullable)moxURL;

// overrides
- (NSNumber* _Nullable)moxLoaded;

// overrides
- (NSNumber* _Nullable)moxLoadingProgress;

// overrides
- (void)handleAccessibleEvent:(uint32_t)eventType;

@end
