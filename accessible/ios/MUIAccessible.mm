/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MUIAccessible.h"

#include "nsString.h"
#include "RootAccessibleWrap.h"

using namespace mozilla;
using namespace mozilla::a11y;

#ifdef A11Y_LOG
#  define DEBUG_HINTS
#endif

#ifdef DEBUG_HINTS
static NSString* ToNSString(const nsACString& aCString) {
  if (aCString.IsEmpty()) {
    return [NSString string];
  }
  return [[[NSString alloc] initWithBytes:aCString.BeginReading()
                                   length:aCString.Length()
                                 encoding:NSUTF8StringEncoding] autorelease];
}
#endif

static NSString* ToNSString(const nsAString& aString) {
  if (aString.IsEmpty()) {
    return [NSString string];
  }
  return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(
                                            aString.BeginReading())
                                 length:aString.Length()];
}

// These rules offer conditions for whether a gecko accessible
// should be considered a UIKit accessibility element. Each role is mapped to a
// rule.
enum class IsAccessibilityElementRule {
  // Always yes
  Yes,
  // Always no
  No,
  // If the accessible has no children. For example an empty header
  // which is labeled.
  IfChildless,
  // If the accessible has no children and it is named and focusable.
  IfChildlessWithNameAndFocusable,
  // If this accessible isn't a child of an accessibility element. For example,
  // a text leaf child of a button.
  IfParentIsntElementWithName,
  // If this accessible has multiple leafs that should functionally be
  // united, for example a link with span elements.
  IfBrokenUp,
};

#pragma mark -

@interface NSObject (AccessibilityPrivate)
- (void)_accessibilityUnregister;
@end

@implementation MUIAccessible

- (id)initWithAccessible:(Accessible*)aAcc {
  MOZ_ASSERT(aAcc, "Cannot init MUIAccessible with null");
  if ((self = [super init])) {
    mGeckoAccessible = aAcc;
  }

  return self;
}

- (mozilla::a11y::Accessible*)geckoAccessible {
  return mGeckoAccessible;
}

- (void)expire {
  mGeckoAccessible = nullptr;
  if ([self respondsToSelector:@selector(_accessibilityUnregister)]) {
    [self _accessibilityUnregister];
  }
}

- (void)dealloc {
  [super dealloc];
}

static bool isAccessibilityElementInternal(Accessible* aAccessible) {
  MOZ_ASSERT(aAccessible);
  IsAccessibilityElementRule rule = IsAccessibilityElementRule::No;

#define ROLE(_geckoRole, stringRole, ariaRole, atkRole, macRole, macSubrole, \
             msaaRole, ia2Role, androidClass, iosIsElement, nameRule)        \
  case roles::_geckoRole:                                                    \
    rule = iosIsElement;                                                     \
    break;
  switch (aAccessible->Role()) {
#include "RoleMap.h"
  }

  switch (rule) {
    case IsAccessibilityElementRule::Yes:
      return true;
    case IsAccessibilityElementRule::No:
      return false;
    case IsAccessibilityElementRule::IfChildless:
      return aAccessible->ChildCount() == 0;
    case IsAccessibilityElementRule::IfParentIsntElementWithName: {
      nsAutoString name;
      aAccessible->Name(name);
      name.CompressWhitespace();
      if (name.IsEmpty()) {
        return false;
      }

      if (isAccessibilityElementInternal(aAccessible->Parent())) {
        // This is a text leaf that needs to be pruned from a button or the
        // likes. It should also be ignored in the event of its parent being a
        // pruned link.
        return false;
      }

      return true;
    }
    case IsAccessibilityElementRule::IfChildlessWithNameAndFocusable:
      if (aAccessible->ChildCount() == 0 &&
          (aAccessible->State() & states::FOCUSABLE)) {
        nsAutoString name;
        aAccessible->Name(name);
        name.CompressWhitespace();
        return !name.IsEmpty();
      }
      return false;
    case IsAccessibilityElementRule::IfBrokenUp: {
      uint32_t childCount = aAccessible->ChildCount();
      if (childCount == 1) {
        // If this is a single child container just use the text leaf and its
        // traits will be inherited.
        return false;
      }

      for (uint32_t idx = 0; idx < childCount; idx++) {
        Accessible* child = aAccessible->ChildAt(idx);
        role accRole = child->Role();
        if (accRole != roles::STATICTEXT && accRole != roles::TEXT_LEAF &&
            accRole != roles::GRAPHIC) {
          // If this container contains anything but text leafs and images
          // ignore this accessible. Its descendants will inherit the
          // container's traits.
          return false;
        }
      }

      return true;
    }
    default:
      break;
  }

  MOZ_ASSERT_UNREACHABLE("Unhandled IsAccessibilityElementRule");

  return false;
}

- (BOOL)isAccessibilityElement {
  if (!mGeckoAccessible) {
    return NO;
  }

  return isAccessibilityElementInternal(mGeckoAccessible) ? YES : NO;
}

- (NSString*)accessibilityLabel {
  if (!mGeckoAccessible) {
    return @"";
  }

  nsAutoString name;
  mGeckoAccessible->Name(name);

  return ToNSString(name);
}

- (NSString*)accessibilityHint {
  if (!mGeckoAccessible) {
    return @"";
  }

#ifdef DEBUG_HINTS
  // Just put in a debug description as the label so we get a clue about which
  // accessible ends up where.
  nsAutoCString desc;
  mGeckoAccessible->DebugDescription(desc);
  return ToNSString(desc);
#else
  return @"";
#endif
}

- (CGRect)accessibilityFrame {
  RootAccessibleWrap* rootAcc = static_cast<RootAccessibleWrap*>(
      mGeckoAccessible->IsLocal()
          ? mGeckoAccessible->AsLocal()->RootAccessible()
          : mGeckoAccessible->AsRemote()
                ->OuterDocOfRemoteBrowser()
                ->RootAccessible());

  if (!rootAcc) {
    return CGRectMake(0, 0, 0, 0);
  }

  LayoutDeviceIntRect rect = mGeckoAccessible->Bounds();
  return rootAcc->DevPixelsRectToUIKit(rect);
}

- (NSString*)accessibilityValue {
  return nil;
}

- (uint64_t)accessibilityTraits {
  return 1ul << 17;  // web content
}

- (NSInteger)accessibilityElementCount {
  return mGeckoAccessible ? mGeckoAccessible->ChildCount() : 0;
}

- (nullable id)accessibilityElementAtIndex:(NSInteger)index {
  if (!mGeckoAccessible) {
    return nil;
  }

  Accessible* child = mGeckoAccessible->ChildAt(index);
  return GetNativeFromGeckoAccessible(child);
}

- (NSInteger)indexOfAccessibilityElement:(id)element {
  Accessible* acc = [(MUIAccessible*)element geckoAccessible];
  if (!acc || mGeckoAccessible != acc->Parent()) {
    return -1;
  }

  return acc->IndexInParent();
}

- (NSArray* _Nullable)accessibilityElements {
  NSMutableArray* children = [[[NSMutableArray alloc] init] autorelease];
  uint32_t childCount = mGeckoAccessible->ChildCount();
  for (uint32_t i = 0; i < childCount; i++) {
    if (MUIAccessible* child =
            GetNativeFromGeckoAccessible(mGeckoAccessible->ChildAt(i))) {
      [children addObject:child];
    }
  }

  return children;
}

- (UIAccessibilityContainerType)accessibilityContainerType {
  return UIAccessibilityContainerTypeNone;
}

@end
