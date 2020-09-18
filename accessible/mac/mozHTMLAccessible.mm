/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozHTMLAccessible.h"

#import "Accessible-inl.h"
#import "HyperTextAccessible.h"

#import "nsCocoaUtils.h"

using namespace mozilla::a11y;

@implementation mozHeadingAccessible

- (NSString*)moxTitle {
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

- (id)moxValue {
  GroupPos groupPos;
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    groupPos = acc->GroupPosition();
  } else if (ProxyAccessible* proxy = mGeckoAccessible.AsProxy()) {
    groupPos = proxy->GroupPosition();
  }

  return [NSNumber numberWithInt:groupPos.level];
}

@end

@implementation mozLinkAccessible

- (NSString*)moxValue {
  return @"";
}

- (NSURL*)moxURL {
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

- (NSNumber*)moxVisited {
  return @([self stateWithMask:states::TRAVERSED] != 0);
}

- (NSString*)moxRole {
  // If this is not LINKED, just expose this as a generic group accessible.
  // Chrome and Safari expose this as a childless AXStaticText, but
  // the HTML Accessibility API Mappings spec says this should be an AXGroup.
  if (![self stateWithMask:states::LINKED]) {
    return NSAccessibilityGroupRole;
  }

  return [super moxRole];
}

@end

@implementation MOXSummaryAccessible

- (NSNumber*)moxExpanded {
  return @([self stateWithMask:states::EXPANDED] != 0);
}

@end
