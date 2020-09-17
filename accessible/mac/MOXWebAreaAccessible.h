/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

using namespace mozilla::a11y;

@class MOXRootGroup;

@interface MOXWebAreaAccessible : mozAccessible {
  MOXRootGroup* mRootGroup;
}
// overrides
- (NSURL*)moxURL;

// override
- (NSNumber*)moxLoaded;

// override
- (NSNumber*)moxLoadingProgress;

// override
- (NSArray*)moxLinkUIElements;

// override
- (NSArray*)moxUnignoredChildren;

// override
- (void)handleAccessibleEvent:(uint32_t)eventType;

// override
- (void)dealloc;

- (NSArray*)rootGroupChildren;

- (id)rootGroup;

@end

@interface MOXRootGroup : MOXAccessibleBase {
  MOXWebAreaAccessible* mParent;
}

// override
- (id)initWithParent:(MOXWebAreaAccessible*)parent;

// override
- (NSString*)moxRole;

// override
- (NSString*)moxRoleDescription;

// override
- (id<mozAccessible>)moxParent;

// override
- (NSArray*)moxChildren;

// override
- (NSString*)moxIdentifier;

// override
- (id)moxHitTest:(NSPoint)point;

// override
- (NSValue*)moxPosition;

// override
- (NSValue*)moxSize;

// override
- (BOOL)disableChild:(id)child;

// override
- (void)expire;

// override
- (BOOL)isExpired;

@end
