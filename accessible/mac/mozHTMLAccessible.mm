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
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    mozilla::ErrorResult rv;
    // XXX use the flattening API when there are available
    // see bug 768298
    accWrap->GetContent()->GetTextContent(title, rv);
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    proxy->Title(title);
  }

  return nsCocoaUtils::ToNSString(title);
}

- (id)value {
  GroupPos groupPos;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    groupPos = accWrap->GroupPosition();
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
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
  if (![self getGeckoAccessible] && ![self getProxyAccessible]) return [NSArray array];

  static NSMutableArray* attributes = nil;

  if (!attributes) {
    attributes = [[super accessibilityAttributeNames] mutableCopy];
    [attributes addObject:NSAccessibilityURLAttribute];
  }

  return attributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if ([attribute isEqualToString:NSAccessibilityURLAttribute]) return [self url];

  return [super accessibilityAttributeValue:attribute];
}

- (NSArray*)accessibilityActionNames {
  // if we're expired, we don't support any actions.
  if (![self getGeckoAccessible] && ![self getProxyAccessible]) return [NSArray array];

  // Always advertise press action first.
  return [@[ NSAccessibilityPressAction ]
      arrayByAddingObjectsFromArray:[super accessibilityActionNames]];
}

- (void)accessibilityPerformAction:(NSString*)action {
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy) {
    return;
  }

  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if (accWrap) {
      accWrap->DoAction(0);
    } else if (proxy) {
      proxy->DoAction(0);
    }
    return;
  }

  [super accessibilityPerformAction:action];
}

- (NSString*)customDescription {
  return @"";
}

- (NSString*)value {
  return @"";
}

- (NSURL*)url {
  nsAutoString value;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    accWrap->Value(value);
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    proxy->Value(value);
  }

  NSString* urlString = value.IsEmpty() ? nil : nsCocoaUtils::ToNSString(value);
  if (!urlString) return nil;

  return [NSURL URLWithString:urlString];
}

@end
