/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"
#include "ProxyAccessible.h"

#import <Cocoa/Cocoa.h>

#import "mozAccessibleProtocol.h"

@class mozRootAccessible;

/**
 * All mozAccessibles are either abstract objects (that correspond to XUL
 * widgets, HTML frames, etc) or are attached to a certain view; for example
 * a document view. When we hand an object off to an AT, we always want
 * to give it the represented view, in the latter case.
 */

namespace mozilla {
namespace a11y {

inline id<mozAccessible> GetObjectOrRepresentedView(id<mozAccessible> aObject) {
  return [aObject hasRepresentedView] ? [aObject representedView] : aObject;
}

inline mozAccessible* GetNativeFromGeckoAccessible(Accessible* aAccessible) {
  mozAccessible* native = nil;
  aAccessible->GetNativeInterface((void**)&native);
  return native;
}

inline mozAccessible* GetNativeFromProxy(const ProxyAccessible* aProxy) {
  return reinterpret_cast<mozAccessible*>(aProxy->GetWrapper());
}

}  // a11y
}  // mozilla

// This is OR'd with the Accessible owner to indicate the wrap-ee is a proxy.
static const uintptr_t IS_PROXY = 1;

@interface mozAccessible : NSObject <mozAccessible> {
  /**
   * Weak reference; it owns us.
   */
  uintptr_t mGeckoAccessible;

  /**
   * The role of our gecko accessible.
   */
  mozilla::a11y::role mRole;

  /**
   * A cache of a subset of our states.
   */
  uint64_t mCachedState;
}

// return the Accessible for this mozAccessible if it exists.
- (mozilla::a11y::AccessibleWrap*)getGeckoAccessible;

// return the ProxyAccessible for this mozAccessible if it exists.
- (mozilla::a11y::ProxyAccessible*)getProxyAccessible;

// inits with the gecko owner.
- (id)initWithAccessible:(uintptr_t)aGeckoObj;

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

// information about focus.
- (BOOL)isFocused;
- (BOOL)canBeFocused;

// returns NO if for some reason we were unable to focus the element.
- (BOOL)focus;

// Given a gecko accessibility event type, post the relevant
// system accessibility notification.
- (void)handleAccessibleEvent:(uint32_t)eventType;

// Post the given accessibility system notification
- (void)postNotification:(NSString*)notification;

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

#pragma mark -

// makes ourselves "expired". after this point, we might be around if someone
// has retained us (e.g., a third-party), but we really contain no information.
- (void)expire;
- (BOOL)isExpired;

#ifdef DEBUG
- (void)printHierarchy;
- (void)printHierarchyWithLevel:(unsigned)numSpaces;

- (void)sanityCheckChildren;
- (void)sanityCheckChildren:(NSArray*)theChildren;
#endif

// ---- NSAccessibility methods ---- //

// whether to skip this element when traversing the accessibility
// hierarchy.
- (BOOL)accessibilityIsIgnored;

// called by third-parties to determine the deepest child element under the mouse
- (id)accessibilityHitTest:(NSPoint)point;

// returns the deepest unignored focused accessible element
- (id)accessibilityFocusedUIElement;

// a mozAccessible needs to at least provide links to its parent and
// children.
- (NSArray*)accessibilityAttributeNames;

// value for the specified attribute
- (id)accessibilityAttributeValue:(NSString*)attribute;

// gets the array length of specified attribute values. Added
// here to have a lighter way to get the child count instead
// of constructing a full array.
- (NSUInteger)accessibilityArrayAttributeCount:(NSString*)attribute;

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute;
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute;

@end
