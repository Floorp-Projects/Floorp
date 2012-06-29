/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#import <Cocoa/Cocoa.h>

#import "mozAccessibleProtocol.h"

@class mozRootAccessible;

/**
 * All mozAccessibles are either abstract objects (that correspond to XUL
 * widgets, HTML frames, etc) or are attached to a certain view; for example
 * a document view. When we hand an object off to an AT, we always want
 * to give it the represented view, in the latter case.
 */
inline id <mozAccessible>
GetObjectOrRepresentedView(id <mozAccessible> aObject)
{
  return [aObject hasRepresentedView] ? [aObject representedView] : aObject;
}

inline mozAccessible*
GetNativeFromGeckoAccessible(nsIAccessible* aAccessible)
{
  mozAccessible* native = nil;
  aAccessible->GetNativeInterface((void**)&native);
  return native;
}

@interface mozAccessible : NSObject <mozAccessible>
{
  /**
   * Weak reference; it owns us.
   */
  AccessibleWrap* mGeckoAccessible;
  
  /**
   * Strong ref to array of children
   */
  NSMutableArray* mChildren;
  
  /** 
   * Weak reference to the parent
   */
  mozAccessible* mParent;

  /**
   * The nsIAccessible role of our gecko accessible.
   */
  mozilla::a11y::role        mRole;
}

// inits with the gecko owner.
- (id)initWithAccessible:(AccessibleWrap*)geckoParent;

// our accessible parent (AXParent)
- (id <mozAccessible>)parent;

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

// the accessible description of this particular instance.
- (NSString*)customDescription;

// the value of this element.
- (id)value;

// name that is associated with this accessible (for buttons, etc)
- (NSString*)title;

// help text associated with this element.
- (NSString*)help;

- (BOOL)isEnabled;

// information about focus.
- (BOOL)isFocused;
- (BOOL)canBeFocused;

// returns NO if for some reason we were unable to focus the element.
- (BOOL)focus;

// notifications sent out to listening accessible providers.
- (void)didReceiveFocus;
- (void)valueDidChange;
- (void)selectedTextDidChange;

#pragma mark -

// invalidates and removes all our children from our cached array.
- (void)invalidateChildren;

/** 
 * Append a child if they are already cached.
 */
- (void)appendChild:(Accessible*)aAccessible;

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

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute;
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute;

@end

