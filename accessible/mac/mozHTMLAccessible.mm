/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozHTMLAccessible.h"

#import "Accessible-inl.h"
#import "HyperTextAccessible.h"

#import "nsCocoaUtils.h"

@implementation mozHeadingAccessible

- (NSString*)title {
  nsAutoString title;
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    mozilla::ErrorResult rv;
    // XXX use the flattening API when there are available
    // see bug 768298
    acc->GetContent()->GetTextContent(title, rv);
  } else if (ProxyAccessible* proxy = mGeckoAccessible.AsProxy()) {
    proxy->Title(title);
  }

  return nsCocoaUtils::ToNSString(title);
}

- (id)value {
  GroupPos groupPos;
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    groupPos = acc->GroupPosition();
  } else if (ProxyAccessible* proxy = mGeckoAccessible.AsProxy()) {
    groupPos = proxy->GroupPosition();
  }

  return [NSNumber numberWithInt:groupPos.level];
}

@end

@interface mozLinkAccessible ()
- (NSURL*)url;
@end

@implementation mozLinkAccessible

- (NSArray*)accessibilityAttributeNames {
  // if we're expired, we don't support any attributes.
  if ([self isExpired]) {
    return @[];
  }

  if (![self stateWithMask:states::LINKED]) {
    // Only expose link semantics if this accessible has a LINKED state.
    return [super accessibilityAttributeNames];
  }

  static NSMutableArray* attributes = nil;

  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:NSAccessibilityURLAttribute];
    [attributes addObject:@"AXVisited"];
  }

  return attributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([self stateWithMask:states::LINKED]) {
    // Only expose link semantics if this accessible has a LINKED state.
    if ([attribute isEqualToString:NSAccessibilityURLAttribute]) return [self url];
    if ([attribute isEqualToString:@"AXVisited"]) {
      return [NSNumber numberWithBool:[self stateWithMask:states::TRAVERSED] != 0];
    }
  }

  return [super accessibilityAttributeValue:attribute];
}

- (NSString*)customDescription {
  return @"";
}

- (NSString*)value {
  return @"";
}

- (NSURL*)url {
  nsAutoString value;
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    acc->Value(value);
  } else if (ProxyAccessible* proxy = mGeckoAccessible.AsProxy()) {
    proxy->Value(value);
  }

  NSString* urlString = value.IsEmpty() ? nil : nsCocoaUtils::ToNSString(value);
  if (!urlString) return nil;

  return [NSURL URLWithString:urlString];
}

- (NSString*)role {
  // If this is not LINKED, just expose this as a generic group accessible.
  // Chrome and Safari expose this as a childless AXStaticText, but
  // the HTML Accessibility API Mappings spec says this should be an AXGroup.
  if (![self stateWithMask:states::LINKED]) {
    return NSAccessibilityGroupRole;
  }

  return [super role];
}

@end
