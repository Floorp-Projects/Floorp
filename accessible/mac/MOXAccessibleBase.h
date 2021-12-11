/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#import "mozAccessibleProtocol.h"
#import "MOXAccessibleProtocol.h"

#include "Platform.h"

inline id<mozAccessible> GetObjectOrRepresentedView(id<mozAccessible> aObject) {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) {
    // If platform a11y is not enabled, don't return represented view.
    // This is mostly for our mochitest environment because the represented
    // ChildView checks `ShouldA11yBeEnabled` before proxying accessibility
    // methods to mozAccessibles.
    return aObject;
  }

  return [aObject hasRepresentedView] ? [aObject representedView] : aObject;
}

@interface MOXAccessibleBase : NSObject <mozAccessible, MOXAccessible> {
  BOOL mIsExpired;
}

#pragma mark - mozAccessible/widget

// override
- (BOOL)hasRepresentedView;

// override
- (id)representedView;

// override
- (BOOL)isRoot;

#pragma mark - mozAccessible/NSAccessibility

// The methods below interface with the platform through NSAccessibility.
// They should not be called directly or overridden in subclasses.

// override, final
- (NSArray*)accessibilityAttributeNames;

// override, final
- (id)accessibilityAttributeValue:(NSString*)attribute;

// override, final
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute;

// override, final
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute;

// override, final
- (NSArray*)accessibilityActionNames;

// override, final
- (void)accessibilityPerformAction:(NSString*)action;

// override, final
- (NSString*)accessibilityActionDescription:(NSString*)action;

// override, final
- (NSArray*)accessibilityParameterizedAttributeNames;

// override, final
- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter;

// override, final
- (id)accessibilityHitTest:(NSPoint)point;

// override, final
- (id)accessibilityFocusedUIElement;

// override, final
- (BOOL)isAccessibilityElement;

// final
- (BOOL)accessibilityNotifiesWhenDestroyed;

#pragma mark - MOXAccessible protocol

// override
- (id)moxHitTest:(NSPoint)point;

// override
- (id)moxFocusedUIElement;

// override
- (void)moxPostNotification:(NSString*)notification;

// override
- (void)moxPostNotification:(NSString*)notification
               withUserInfo:(NSDictionary*)userInfo;

// override
- (BOOL)moxBlockSelector:(SEL)selector;

// override
- (NSArray*)moxUnignoredChildren;

// override
- (NSArray*)moxChildren;

// override
- (id<mozAccessible>)moxUnignoredParent;

// override
- (id<mozAccessible>)moxParent;

// override
- (NSNumber*)moxIndexForChildUIElement:(id)child;

// override
- (id)moxTopLevelUIElement;

// override
- (id<MOXTextMarkerSupport>)moxTextMarkerDelegate;

// override
- (BOOL)moxIsLiveRegion;

// override
- (id<MOXAccessible>)moxFindAncestor:(BOOL (^)(id<MOXAccessible> moxAcc,
                                               BOOL* stop))findBlock;

#pragma mark -

- (NSString*)description;

- (BOOL)isExpired;

// makes ourselves "expired". after this point, we might be around if someone
// has retained us (e.g., a third-party), but we really contain no information.
- (void)expire;

@end
