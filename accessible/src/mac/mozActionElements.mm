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
#import "nsIAccessible.h"

extern const NSString *kInstanceDescriptionAttribute; // NSAccessibilityDescriptionAttribute
extern const NSString *kTopLevelUIElementAttribute;   // NSAccessibilityTopLevelUIElementAttribute

enum CheckboxValue {
  // these constants correspond to the values in the OS
  kUnchecked = 0,
  kChecked = 1,
  kMixed = 2
};

@implementation mozButtonAccessible

- (NSArray*)accessibilityAttributeNames
{
  static NSArray *attributes = nil;
  if (!attributes) {
    attributes = [[NSArray alloc] initWithObjects:NSAccessibilityParentAttribute, // required
                                                  NSAccessibilityRoleAttribute, // required
                                                  NSAccessibilityPositionAttribute, // required
                                                  NSAccessibilitySizeAttribute, // required
                                                  NSAccessibilityWindowAttribute, // required
                                                  NSAccessibilityPositionAttribute, // required
                                                  kTopLevelUIElementAttribute, // required
                                                  NSAccessibilityHelpAttribute,
                                                  NSAccessibilityEnabledAttribute, // required
                                                  NSAccessibilityFocusedAttribute, // required
                                                  NSAccessibilityTitleAttribute, // required
                                                  kInstanceDescriptionAttribute,
                                                  nil];
  }
  return attributes;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
    return nil;
  return [super accessibilityAttributeValue:attribute];
}

- (BOOL)accessibilityIsIgnored
{
  return mIsExpired;
}

- (NSArray*)accessibilityActionNames
{
  if ([self isEnabled])
    return [NSArray arrayWithObject:NSAccessibilityPressAction];
    
  return nil;
}

- (NSString*)accessibilityActionDescription:(NSString*)action 
{
  if ([action isEqualToString:NSAccessibilityPressAction])
    return @"press button"; // XXX: localize this later?
    
  return nil;
}

- (void)accessibilityPerformAction:(NSString*)action 
{
  if ([action isEqualToString:NSAccessibilityPressAction])
    [self click];
}

- (void)click
{
  // both buttons and checkboxes have only one action. we should really stop using arbitrary
  // arrays with actions, and define constants for these actions.
  mGeckoAccessible->DoAction(0);
}

@end

@implementation mozCheckboxAccessible

- (NSString*)accessibilityActionDescription:(NSString*)action 
{
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if ([self isChecked] != kUnchecked)
      return @"uncheck checkbox"; // XXX: localize this later?
    
    return @"check checkbox"; // XXX: localize this later?
  }
  
  return nil;
}

- (int)isChecked
{
  PRUint32 state = 0;
  mGeckoAccessible->GetState(&state);
  
  // check if we're checked or in a mixed state
  if (state & nsIAccessibleStates::STATE_CHECKED) {
    return (state & nsIAccessibleStates::STATE_MIXED) ? kMixed : kChecked;
  }
  
  return kUnchecked;
}

- (id)value
{
  return [NSNumber numberWithInt:[self isChecked]];
}

@end

@implementation mozPopupButtonAccessible

- (NSArray *)accessibilityAttributeNames
{
  static NSArray *attributes = nil;
  
  if (!attributes) {
    attributes = [[NSArray alloc] initWithObjects:NSAccessibilityParentAttribute, // required
                                                  NSAccessibilityPositionAttribute, // required
                                                  NSAccessibilityRoleAttribute, // required
                                                  NSAccessibilitySizeAttribute, // required
                                                  NSAccessibilityWindowAttribute, // required
                                                  kTopLevelUIElementAttribute, // required
                                                  NSAccessibilityHelpAttribute,
                                                  NSAccessibilityEnabledAttribute, // required
                                                  NSAccessibilityFocusedAttribute, // required
                                                  NSAccessibilityTitleAttribute, // required for popupmenus, and for menubuttons with a title
                                                  NSAccessibilityChildrenAttribute, // required
                                                  kInstanceDescriptionAttribute, // required if it has no title attr
                                                  nil];
  }
  return attributes;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    return [super children];
  }
  return [super accessibilityAttributeValue:attribute];
}

- (NSArray *)accessibilityActionNames
{
  if ([self isEnabled]) {
    return [NSArray arrayWithObjects:NSAccessibilityPressAction,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
                                     NSAccessibilityShowMenuAction,
#endif
                                     nil];
  }
  return nil;
}

- (NSString *)accessibilityActionDescription:(NSString *)action
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
  if ([action isEqualToString:NSAccessibilityShowMenuAction])
    return @"show menu";
#endif
  return [super accessibilityActionDescription:action];
}

- (void)accessibilityPerformAction:(NSString *)action
{
  // both the ShowMenu and Click action do the same thing.
  if ([self isEnabled]) {
    // TODO: this should bring up the menu, but currently doesn't.
    //       once msaa and atk have merged better, they will implement
    //       the action needed to show the menu.
    [super click];
  }
}

@end
