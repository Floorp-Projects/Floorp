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

#import "mozActionElements.h"

#import "MacUtils.h"
#include "Accessible-inl.h"
#include "nsXULTabAccessible.h"

#include "nsObjCExceptions.h"

using namespace mozilla::a11y;

enum CheckboxValue {
  // these constants correspond to the values in the OS
  kUnchecked = 0,
  kChecked = 1,
  kMixed = 2
};

@implementation mozButtonAccessible

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  static NSArray *attributes = nil;
  if (!attributes) {
    attributes = [[NSArray alloc] initWithObjects:NSAccessibilityParentAttribute, // required
                                                  NSAccessibilityRoleAttribute, // required
                                                  NSAccessibilityRoleDescriptionAttribute,
                                                  NSAccessibilityPositionAttribute, // required
                                                  NSAccessibilitySizeAttribute, // required
                                                  NSAccessibilityWindowAttribute, // required
                                                  NSAccessibilityPositionAttribute, // required
                                                  NSAccessibilityTopLevelUIElementAttribute, // required
                                                  NSAccessibilityHelpAttribute,
                                                  NSAccessibilityEnabledAttribute, // required
                                                  NSAccessibilityFocusedAttribute, // required
                                                  NSAccessibilityTitleAttribute, // required
                                                  NSAccessibilityDescriptionAttribute,
#if DEBUG
                                                  @"AXMozDescription",
#endif
                                                  nil];
  }
  return attributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
    return nil;
  if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
    if ([self isTab])
      return utils::LocalizedString(NS_LITERAL_STRING("tab"));
    
    return NSAccessibilityRoleDescription([self role], nil);
  }
  
  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsIgnored
{
  return mIsExpired;
}

- (NSArray*)accessibilityActionNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([self isEnabled])
    return [NSArray arrayWithObject:NSAccessibilityPressAction];
    
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)accessibilityActionDescription:(NSString*)action 
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if ([self isTab])
      return utils::LocalizedString(NS_LITERAL_STRING("switch"));
  
    return @"press button"; // XXX: localize this later?
  }
  
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)accessibilityPerformAction:(NSString*)action 
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([action isEqualToString:NSAccessibilityPressAction])
    [self click];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)click
{
  // both buttons and checkboxes have only one action. we should really stop using arbitrary
  // arrays with actions, and define constants for these actions.
  mGeckoAccessible->DoAction(0);
}

- (BOOL)isTab
{
  return (mGeckoAccessible && (mGeckoAccessible->Role() == roles::PAGETAB));
}

@end

@implementation mozCheckboxAccessible

- (NSString*)accessibilityActionDescription:(NSString*)action 
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if ([self isChecked] != kUnchecked)
      return @"uncheck checkbox"; // XXX: localize this later?
    
    return @"check checkbox"; // XXX: localize this later?
  }
  
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (int)isChecked
{
  PRUint64 state = mGeckoAccessible->NativeState();

  // check if we're checked or in a mixed state
  if (state & states::CHECKED) {
    return (state & states::MIXED) ? kMixed : kChecked;
  }
  
  return kUnchecked;
}

- (id)value
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSNumber numberWithInt:[self isChecked]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

@end

@implementation mozPopupButtonAccessible

- (NSArray *)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  static NSArray *attributes = nil;
  
  if (!attributes) {
    attributes = [[NSArray alloc] initWithObjects:NSAccessibilityParentAttribute, // required
                                                  NSAccessibilityPositionAttribute, // required
                                                  NSAccessibilityRoleAttribute, // required
                                                  NSAccessibilitySizeAttribute, // required
                                                  NSAccessibilityWindowAttribute, // required
                                                  NSAccessibilityTopLevelUIElementAttribute, // required
                                                  NSAccessibilityHelpAttribute,
                                                  NSAccessibilityEnabledAttribute, // required
                                                  NSAccessibilityFocusedAttribute, // required
                                                  NSAccessibilityTitleAttribute, // required for popupmenus, and for menubuttons with a title
                                                  NSAccessibilityChildrenAttribute, // required
                                                  NSAccessibilityDescriptionAttribute, // required if it has no title attr
#if DEBUG
                                                  @"AXMozDescription",
#endif
                                                  nil];
  }
  return attributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    return [super children];
  }
  return [super accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSArray *)accessibilityActionNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([self isEnabled]) {
    return [NSArray arrayWithObjects:NSAccessibilityPressAction,
                                     NSAccessibilityShowMenuAction,
                                     nil];
  }
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString *)accessibilityActionDescription:(NSString *)action
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([action isEqualToString:NSAccessibilityShowMenuAction])
    return @"show menu";
  return [super accessibilityActionDescription:action];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)accessibilityPerformAction:(NSString *)action
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // both the ShowMenu and Click action do the same thing.
  if ([self isEnabled]) {
    // TODO: this should bring up the menu, but currently doesn't.
    //       once msaa and atk have merged better, they will implement
    //       the action needed to show the menu.
    [super click];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end

@implementation mozTabsAccessible

- (void)dealloc
{
  [mTabs release];

  [super dealloc];
}

- (NSArray*)accessibilityAttributeNames
{
  // standard attributes that are shared and supported by root accessible (AXMain) elements.
  static NSMutableArray* attributes = nil;
  
  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:NSAccessibilityContentsAttribute];
    [attributes addObject:NSAccessibilityTabsAttribute];
  }
  
  return attributes;  
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{  
  if ([attribute isEqualToString:NSAccessibilityContentsAttribute])
    return [super children];
  if ([attribute isEqualToString:NSAccessibilityTabsAttribute])
    return [self tabs];
  
  return [super accessibilityAttributeValue:attribute];  
}

/**
 * Returns the selected tab (the mozAccessible)
 */
- (id)value
{
  if (!mGeckoAccessible)
    return nil;
    
  nsAccessible* accessible = mGeckoAccessible->GetSelectedItem(0);
  if (!accessible)
    return nil;

  mozAccessible* nativeAcc = nil;
  nsresult rv = accessible->GetNativeInterface((void**)&nativeAcc);
  NS_ENSURE_SUCCESS(rv, nil);
  
  return nativeAcc;
}

/**
 * Return the mozAccessibles that are the tabs.
 */
- (id)tabs
{
  if (mTabs)
    return mTabs;

  NSArray* children = [self children];
  NSEnumerator* enumerator = [children objectEnumerator];
  mTabs = [[NSMutableArray alloc] init];
  
  id obj;
  while ((obj = [enumerator nextObject]))
    if ([obj isTab])
      [mTabs addObject:obj];

  return mTabs;
}

- (void)invalidateChildren
{
  [super invalidateChildren];

  [mTabs release];
  mTabs = nil;
}

@end
