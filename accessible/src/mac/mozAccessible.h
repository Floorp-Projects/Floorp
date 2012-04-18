/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Håkan Waara <hwaara@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAccessibleWrap.h"

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

@interface mozAccessible : NSObject <mozAccessible>
{
  /**
   * Weak reference; it owns us.
   */
  nsAccessibleWrap* mGeckoAccessible;
  
  /**
   * Strong ref to array of children
   */
  NSMutableArray* mChildren;
  
  /** 
   * Weak reference to the parent
   */
  mozAccessible* mParent;
  
  /**
   * We can be marked as 'expired' if Shutdown() is called on our geckoAccessible.
   * since we might still be retained by some third-party, we need to do cleanup
   * in |expire|, and prevent any potential harm that could come from someone using us
   * after this point.
   */
  BOOL mIsExpired;
  
  /**
   * The nsIAccessible role of our gecko accessible.
   */
  mozilla::a11y::role        mRole;
}

// inits with the gecko owner.
- (id)initWithAccessible:(nsAccessibleWrap*)geckoParent;

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
- (void)appendChild:(nsAccessible*)aAccessible;

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

