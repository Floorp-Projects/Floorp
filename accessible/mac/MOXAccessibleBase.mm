/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MOXAccessibleBase.h"

#import "MacSelectorMap.h"

#include "nsObjCExceptions.h"
#include "xpcAccessibleMacInterface.h"

using namespace mozilla::a11y;

@implementation MOXAccessibleBase

#pragma mark - mozAccessible/widget

- (BOOL)hasRepresentedView {
  return NO;
}

- (id)representedView {
  return nil;
}

- (BOOL)isRoot {
  return NO;
}

#pragma mark - mozAccessible/NSAccessibility

- (NSArray*)accessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  return @[];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  return nil;
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;
  return NO;
  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSArray*)accessibilityActionNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  return @[];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)accessibilityPerformAction:(NSString*)action {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSString*)accessibilityActionDescription:(NSString*)action {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  // by default we return whatever the MacOS API know about.
  // if you have custom actions, override.
  return NSAccessibilityActionDescription(action);
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSArray*)accessibilityParameterizedAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  return @[];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  return nil;
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityHitTest:(NSPoint)point {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  return [self moxHitTest:point];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityFocusedUIElement {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  return [self moxFocusedUIElement];
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isAccessibilityElement {
  return YES;
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  return YES;
}

#pragma mark - MOXAccessible protocol

- (id)moxHitTest:(NSPoint)point {
  return GetObjectOrRepresentedView(self);
}

- (id)moxFocusedUIElement {
  return GetObjectOrRepresentedView(self);
}

- (void)moxPostNotification:(NSString*)notification {
  // This sends events via nsIObserverService to be consumed by our mochitests.
  xpcAccessibleMacInterface::FireEvent(self, notification);

  if (gfxPlatform::IsHeadless()) {
    // Using a headless toolkit for tests and whatnot, posting accessibility
    // notification won't work.
    return;
  }

  if (![self isAccessibilityElement]) {
    // If this is an ignored object, don't expose it to system.
    return;
  }

  NSAccessibilityPostNotification(GetObjectOrRepresentedView(self), notification);
}

#pragma mark -

- (BOOL)isExpired {
  return mIsExpired;
}

- (void)expire {
  MOZ_ASSERT(!mIsExpired, "expire called an expired mozAccessible!");

  mIsExpired = YES;

  [self moxPostNotification:NSAccessibilityUIElementDestroyedNotification];
}

@end
