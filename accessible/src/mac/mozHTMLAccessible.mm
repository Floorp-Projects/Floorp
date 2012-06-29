/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozHTMLAccessible.h"

#import "HyperTextAccessible.h"

#import "nsCocoaUtils.h"

@implementation mozHeadingAccessible

- (NSString*)title
{
  nsAutoString title;
  // XXX use the flattening API when there are available
  // see bug 768298
  nsresult rv = mGeckoAccessible->GetContent()->GetTextContent(title);
  NS_ENSURE_SUCCESS(rv, nil);

  return nsCocoaUtils::ToNSString(title);
}

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
  if (!mGeckoAccessible)
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
  if (!mGeckoAccessible)
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
