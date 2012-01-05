/* -*- Mode: Objective-C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

