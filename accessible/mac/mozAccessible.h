/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"
#include "ProxyAccessible.h"
#include "AccessibleOrProxy.h"

#import <Cocoa/Cocoa.h>

#import "MOXAccessibleBase.h"

@class mozRootAccessible;

/**
 * All mozAccessibles are either abstract objects (that correspond to XUL
 * widgets, HTML frames, etc) or are attached to a certain view; for example
 * a document view. When we hand an object off to an AT, we always want
 * to give it the represented view, in the latter case.
 */

namespace mozilla {
namespace a11y {

inline mozAccessible* GetNativeFromGeckoAccessible(mozilla::a11y::AccessibleOrProxy aAccOrProxy) {
  MOZ_ASSERT(!aAccOrProxy.IsNull(), "Cannot get native from null accessible");
  if (Accessible* acc = aAccOrProxy.AsAccessible()) {
    mozAccessible* native = nil;
    acc->GetNativeInterface((void**)&native);
    return native;
  }

  ProxyAccessible* proxy = aAccOrProxy.AsProxy();
  return reinterpret_cast<mozAccessible*>(proxy->GetWrapper());
}

}  // a11y
}  // mozilla

@interface mozAccessible : MOXAccessibleBase {
  /**
   * Reference to the accessible we were created with;
   * either a proxy accessible or an accessible wrap.
   */
  mozilla::a11y::AccessibleOrProxy mGeckoAccessible;

  /**
   * The role of our gecko accessible.
   */
  mozilla::a11y::role mRole;

  /**
   * A cache of a subset of our states.
   */
  uint64_t mCachedState;
}

// inits with the given wrap or proxy accessible
- (id)initWithAccessible:(mozilla::a11y::AccessibleOrProxy)aAccOrProxy;

// allows for gecko accessible access outside of the class
- (mozilla::a11y::AccessibleOrProxy)geckoAccessible;

// our accessible parent (AXParent)
- (id<mozAccessible>)parent;

// a lazy cache of our accessible children (AXChildren). updated
- (NSArray*)children;

// returns the size of this accessible.
- (NSValue*)size;

// returns the position, in cocoa coordinates.
- (NSValue*)position;

// can be overridden to report another role name.
- (NSString*)role;

// a subrole is a more specialized variant of the role. for example,
// the role might be "textfield", while the subrole is "password textfield".
- (NSString*)subrole;

// Return the role description, as there are a few exceptions.
- (NSString*)roleDescription;

// returns the native window we're inside.
- (NSWindow*)window;

// the value of this element.
- (id)value;

// name that is associated with this accessible (for buttons, etc)
- (NSString*)title;

// the accessible description (help text) of this particular instance.
- (NSString*)help;

// returns the orientation (vertical, horizontal, or undefined)
- (NSString*)orientation;

- (BOOL)isEnabled;

// should a child be disabled
- (BOOL)disableChild:(mozAccessible*)child;

// information about focus.
- (BOOL)isFocused;
- (BOOL)canBeFocused;

// returns NO if for some reason we were unable to focus the element.
- (BOOL)focus;

// Given a gecko accessibility event type, post the relevant
// system accessibility notification.
- (void)handleAccessibleEvent:(uint32_t)eventType;

// internal method to retrieve a child at a given index.
- (id)childAt:(uint32_t)i;

// Get gecko accessible's state.
- (uint64_t)state;

// Get gecko accessible's state filtered through given mask.
- (uint64_t)stateWithMask:(uint64_t)mask;

// Notify of a state change, so the cache can be altered.
- (void)stateChanged:(uint64_t)state isEnabled:(BOOL)enabled;

// Invalidate cached state.
- (void)invalidateState;

// This is called by isAccessibilityElement. If a subclass wants
// to alter the isAccessibilityElement return value, it should
// override this and not isAccessibilityElement directly.
- (BOOL)ignoreWithParent:(mozAccessible*)parent;

// Should the child be ignored. This allows subclasses to determine
// what kinds of accessibles are valid children. This causes the child
// to be skipped, but the unignored descendants will be added to the
// container in the default children getter.
- (BOOL)ignoreChild:(mozAccessible*)child;

#pragma mark -

// makes ourselves "expired". after this point, we might be around if someone
// has retained us (e.g., a third-party), but we really contain no information.
- (void)expire;
- (BOOL)isExpired;

// ---- MOXAccessible methods ---- //

// called to determine the deepest child element under the mouse
- (id)moxHitTest:(NSPoint)point;

// returns the deepest unignored focused accessible element
- (id)moxFocusedUIElement;

// ---- NSAccessibility methods ---- //

// whether to include this element in the platform's tree
- (BOOL)isAccessibilityElement;

// a mozAccessible needs to at least provide links to its parent and
// children.
- (NSArray*)accessibilityAttributeNames;

// value for the specified attribute
- (id)accessibilityAttributeValue:(NSString*)attribute;

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute;
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute;

@end
