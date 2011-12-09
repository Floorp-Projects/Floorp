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

#include "nsRootAccessibleWrap.h"
#include "nsObjCExceptions.h"

#import "mozDocAccessible.h"

#import "mozView.h"

static id <mozAccessible, mozView> getNativeViewFromRootAccessible (nsAccessible *accessible)
{
  nsRootAccessibleWrap *root = static_cast<nsRootAccessibleWrap*>(accessible);
  id <mozAccessible, mozView> nativeView = nil;
  root->GetNativeWidget ((void**)&nativeView);
  return nativeView;
}

#pragma mark -

@implementation mozRootAccessible

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  
  // if we're expired, we don't support any attributes.
  if (mIsExpired)
    return [NSArray array];
  
  // standard attributes that are shared and supported by root accessible (AXMain) elements.
  static NSMutableArray* attributes = nil;
  
  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:NSAccessibilityMainAttribute];
    [attributes addObject:NSAccessibilityMinimizedAttribute];
  }

  return attributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  
  if ([attribute isEqualToString:NSAccessibilityMainAttribute])
    return [NSNumber numberWithBool:[[self window] isMainWindow]];
  if ([attribute isEqualToString:NSAccessibilityMinimizedAttribute])
    return [NSNumber numberWithBool:[[self window] isMiniaturized]];

  return [super accessibilityAttributeValue:attribute];
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}


// return the AXParent that our parallell NSView tells us about.
- (id)parent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mParallelView)
    mParallelView = (id<mozView, mozAccessible>)[self representedView];
  
  if (mParallelView)
    return [mParallelView accessibilityAttributeValue:NSAccessibilityParentAttribute];
  
  NSAssert(mParallelView, @"we're a root accessible w/o native view?");
  return [super parent];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)hasRepresentedView
{
  return YES;
}

// this will return our parallell NSView. see mozDocAccessible.h
- (id)representedView
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mParallelView)
    return (id)mParallelView;
  
  mParallelView = getNativeViewFromRootAccessible (mGeckoAccessible);
  
  NSAssert(mParallelView, @"can't return root accessible's native parallel view.");
  return mParallelView;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isRoot
{
  return YES;
}

@end
