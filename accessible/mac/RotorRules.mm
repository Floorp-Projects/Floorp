/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "RotorRules.h"

#include "nsCocoaUtils.h"
#include "DocAccessibleParent.h"

using namespace mozilla::a11y;

// Generic Rotor Rule

RotorRule::RotorRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : mDirectDescendantsFrom(aDirectDescendantsFrom) {}

RotorRule::RotorRule() : mDirectDescendantsFrom(nullptr) {}

uint16_t RotorRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAccOrProxy)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (!mDirectDescendantsFrom.IsNull() &&
      (aAccOrProxy != mDirectDescendantsFrom)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if ([GetNativeFromGeckoAccessible(aAccOrProxy) isAccessibilityElement]) {
    result |= nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  return result;
}

// Rotor Role Rule

RotorRoleRule::RotorRoleRule(role aRole,
                             AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorRule(aDirectDescendantsFrom), mRole(aRole){};

RotorRoleRule::RotorRoleRule(role aRole) : RotorRule(), mRole(aRole){};

uint16_t RotorRoleRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRule::Match(aAccOrProxy);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH) &&
      aAccOrProxy.Role() != mRole) {
    result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  return result;
}

// Rotor Mac Role Rule

RotorMacRoleRule::RotorMacRoleRule(NSString* aMacRole,
                                   AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorRule(aDirectDescendantsFrom), mMacRole(aMacRole) {
  [mMacRole retain];
};

RotorMacRoleRule::RotorMacRoleRule(NSString* aMacRole)
    : RotorRule(), mMacRole(aMacRole) {
  [mMacRole retain];
};

RotorMacRoleRule::~RotorMacRoleRule() { [mMacRole release]; }

uint16_t RotorMacRoleRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRule::Match(aAccOrProxy);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAccOrProxy);
    if (![[nativeMatch moxRole] isEqualToString:mMacRole]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// Rotor Control Rule

RotorControlRule::RotorControlRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorRule(aDirectDescendantsFrom){};

RotorControlRule::RotorControlRule() : RotorRule(){};

uint16_t RotorControlRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRule::Match(aAccOrProxy);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    switch (aAccOrProxy.Role()) {
      case roles::PUSHBUTTON:
      case roles::SPINBUTTON:
      case roles::DETAILS:
      case roles::CHECKBUTTON:
      case roles::COLOR_CHOOSER:
      case roles::BUTTONDROPDOWNGRID:  // xul colorpicker
      case roles::LISTBOX:
      case roles::COMBOBOX:
      case roles::EDITCOMBOBOX:
      case roles::RADIOBUTTON:
      case roles::RADIO_GROUP:
      case roles::PAGETAB:
      case roles::SLIDER:
      case roles::SWITCH:
      case roles::ENTRY:
      case roles::OUTLINE:
      case roles::PASSWORD_TEXT:
      case roles::BUTTONMENU:
        return result;

      case roles::DATE_EDITOR:
      case roles::TIME_EDITOR:
        result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
        return result;

      case roles::GROUPING: {
        // Groupings are sometimes used (like radio groups) to denote
        // sets of controls. If that's the case, we want to surface
        // them. We also want to surface grouped time and date controls.
        for (unsigned int i = 0; i < aAccOrProxy.ChildCount(); i++) {
          AccessibleOrProxy currChild = aAccOrProxy.ChildAt(i);
          if (currChild.Role() == roles::CHECKBUTTON ||
              currChild.Role() == roles::SWITCH ||
              currChild.Role() == roles::SPINBUTTON ||
              currChild.Role() == roles::RADIOBUTTON) {
            return result;
          }
        }

        // if we iterated through the groups children and didn't
        // find a control with one of the roles above, we should
        // ignore this grouping
        result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
        return result;
      }

      default:
        // if we did not match on any above role, we should
        // ignore this accessible.
        result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// Rotor TextEntry Rule

RotorTextEntryRule::RotorTextEntryRule(
    AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorRule(aDirectDescendantsFrom){};

RotorTextEntryRule::RotorTextEntryRule() : RotorRule(){};

uint16_t RotorTextEntryRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRule::Match(aAccOrProxy);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    if (aAccOrProxy.Role() != roles::PASSWORD_TEXT &&
        aAccOrProxy.Role() != roles::ENTRY) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// Rotor Link Rule

RotorLinkRule::RotorLinkRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorRule(aDirectDescendantsFrom){};

RotorLinkRule::RotorLinkRule() : RotorRule(){};

uint16_t RotorLinkRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRule::Match(aAccOrProxy);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAccOrProxy);
    if (![[nativeMatch moxRole] isEqualToString:@"AXLink"]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

RotorVisitedLinkRule::RotorVisitedLinkRule() : RotorLinkRule() {}

RotorVisitedLinkRule::RotorVisitedLinkRule(
    AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorLinkRule(aDirectDescendantsFrom) {}

uint16_t RotorVisitedLinkRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorLinkRule::Match(aAccOrProxy);

  if (result & nsIAccessibleTraversalRule::FILTER_MATCH) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAccOrProxy);
    if (![[nativeMatch moxVisited] boolValue]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

RotorUnvisitedLinkRule::RotorUnvisitedLinkRule() : RotorLinkRule() {}

RotorUnvisitedLinkRule::RotorUnvisitedLinkRule(
    AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorLinkRule(aDirectDescendantsFrom) {}

uint16_t RotorUnvisitedLinkRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorLinkRule::Match(aAccOrProxy);

  if (result & nsIAccessibleTraversalRule::FILTER_MATCH) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAccOrProxy);
    if ([[nativeMatch moxVisited] boolValue]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// Match Not Rule

RotorNotMacRoleRule::RotorNotMacRoleRule(
    NSString* aMacRole, AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorMacRoleRule(aMacRole, aDirectDescendantsFrom) {}

RotorNotMacRoleRule::RotorNotMacRoleRule(NSString* aMacRole)
    : RotorMacRoleRule(aMacRole) {}

uint16_t RotorNotMacRoleRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRule::Match(aAccOrProxy);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not different from the desired role, we flip the
  // match bit to "unmatch" otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAccOrProxy);
    if ([[nativeMatch moxRole] isEqualToString:mMacRole]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }
  return result;
}

// Rotor Static Text Rule

RotorStaticTextRule::RotorStaticTextRule(
    AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorRule(aDirectDescendantsFrom){};

RotorStaticTextRule::RotorStaticTextRule() : RotorRule(){};

uint16_t RotorStaticTextRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRule::Match(aAccOrProxy);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAccOrProxy);
    if (![[nativeMatch moxRole] isEqualToString:@"AXStaticText"]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// Rotor Heading Level Rule

RotorHeadingLevelRule::RotorHeadingLevelRule(
    int32_t aLevel, AccessibleOrProxy& aDirectDescendantsFrom)
    : RotorRoleRule(roles::HEADING, aDirectDescendantsFrom), mLevel(aLevel){};

RotorHeadingLevelRule::RotorHeadingLevelRule(int32_t aLevel)
    : RotorRoleRule(roles::HEADING), mLevel(aLevel){};

uint16_t RotorHeadingLevelRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRoleRule::Match(aAccOrProxy);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired heading level, we flip the match bit to
  // "unmatch" otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    int32_t currLevel = 0;
    if (LocalAccessible* acc = aAccOrProxy.AsAccessible()) {
      currLevel = acc->GroupPosition().level;
    } else if (RemoteAccessible* proxy = aAccOrProxy.AsProxy()) {
      currLevel = proxy->GroupPosition().level;
    }

    if (currLevel != mLevel) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

uint16_t RotorLiveRegionRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRule::Match(aAccOrProxy);

  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAccOrProxy);
    if (![nativeMatch moxIsLiveRegion]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }
  return result;
}

// Outline Rule

OutlineRule::OutlineRule() : RotorRule(){};

uint16_t OutlineRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = RotorRule::Match(aAccOrProxy);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here.
  if (result & nsIAccessibleTraversalRule::FILTER_MATCH) {
    if (aAccOrProxy.Role() == roles::OUTLINE) {
      // if the match is an outline, we ignore all children here
      // and unmatch the outline itself
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
      result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    } else if (aAccOrProxy.Role() != roles::OUTLINEITEM) {
      // if the match is not an outline item, we unmatch here
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}
