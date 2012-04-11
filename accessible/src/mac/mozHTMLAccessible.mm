/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Hub Figuière <hub@mozilla.com>
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

#import "mozHTMLAccessible.h"

#import "nsHyperTextAccessible.h"

#import "nsCocoaUtils.h"

@implementation mozHeadingAccessible

- (id)value
{
  if (!mGeckoAccessible || !mGeckoAccessible->IsHyperText())
    return nil;

  PRUint32 level = mGeckoAccessible->AsHyperText()->GetLevelInternal();
  return [NSNumber numberWithInt:level];
}

@end

@interface mozLinkAccessible ()
-(NSURL*)url;
@end

@implementation mozLinkAccessible

- (NSArray*)accessibilityAttributeNames
{
  // if we're expired, we don't support any attributes.
  if (mIsExpired)
    return [NSArray array];
  
  static NSMutableArray* attributes = nil;
  
  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:NSAccessibilityURLAttribute];
  }

  return attributes;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
  if ([attribute isEqualToString:NSAccessibilityURLAttribute])
    return [self url];

  return [super accessibilityAttributeValue:attribute];
}

- (NSArray*)accessibilityActionNames 
{
    // if we're expired, we don't support any attributes.
  if (mIsExpired)
    return [NSArray array];

  static NSArray* actionNames = nil;

  if (!actionNames) {
    actionNames = [[NSArray alloc] initWithObjects:NSAccessibilityPressAction,
                                   nil];
  }

  return actionNames;
}

- (void)accessibilityPerformAction:(NSString*)action 
{
  if (!mGeckoAccessible)
    return;

  if ([action isEqualToString:NSAccessibilityPressAction])
    mGeckoAccessible->DoAction(0);
  else
    [super accessibilityPerformAction:action];
}

- (NSURL*)url
{
  if (!mGeckoAccessible || mGeckoAccessible->IsDefunct())
    return nil;

  nsAutoString value;
  mGeckoAccessible->Value(value);

  NSString* urlString = value.IsEmpty() ? nil : nsCocoaUtils::ToNSString(value);
  if (!urlString)
    return nil;

  return [NSURL URLWithString:urlString];
}

@end
