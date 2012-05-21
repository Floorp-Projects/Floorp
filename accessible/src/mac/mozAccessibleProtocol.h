/* -*- Mode: Objective-C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#import "mozView.h"

/* This protocol's primary use is so widget/cocoa can talk back to us
   properly.
   
   ChildView owns the topmost mozRootAccessible, and needs to take care of setting up 
   that parent/child relationship.
   
   This protocol is thus used to make sure it knows it's talking to us, and not
   just some random |id|.
*/

@protocol mozAccessible

// returns whether this accessible is the root accessible. there is one
// root accessible per window.
- (BOOL)isRoot;

// some mozAccessibles implement accessibility support in place of another object. for example, 
// ChildView gets its support from us.
//
// instead of returning a mozAccessible to the OS when it wants an object, we need to pass the view we represent, so the
// OS doesn't get confused and think we return some random object.
- (BOOL)hasRepresentedView;
- (id)representedView;

#ifdef DEBUG
// debug utility that will print the native accessibility tree, starting
// at this node.
- (void)printHierarchy;
#endif

/*** general ***/

// returns the accessible at the specified point.
- (id)accessibilityHitTest:(NSPoint)point;

// whether this element is flagged as ignored.
- (BOOL)accessibilityIsIgnored;

// currently focused UI element (possibly a child accessible)
- (id)accessibilityFocusedUIElement;

/*** attributes ***/

// all supported attributes
- (NSArray*)accessibilityAttributeNames;

// value for given attribute.
- (id)accessibilityAttributeValue:(NSString*)attribute;

// whether a particular attribute can be modified
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute;

/*** actions ***/

- (NSArray*)accessibilityActionNames;
- (NSString*)accessibilityActionDescription:(NSString*)action;
- (void)accessibilityPerformAction:(NSString*)action;

@end

