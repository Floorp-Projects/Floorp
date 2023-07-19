/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "RotorRules.h"

#include "nsCocoaUtils.h"
#include "DocAccessibleParent.h"
#include "nsIAccessiblePivot.h"
#include "nsAccUtils.h"

#include "nsAccessibilityService.h"

using namespace mozilla;
using namespace mozilla::a11y;

// Generic Rotor Rule

RotorRule::RotorRule(Accessible* aDirectDescendantsFrom,
                     const nsString& aSearchText)
    : mDirectDescendantsFrom(aDirectDescendantsFrom),
      mSearchText(aSearchText) {}

RotorRule::RotorRule(const nsString& aSearchText)
    : mDirectDescendantsFrom(nullptr), mSearchText(aSearchText) {}

uint16_t RotorRule::Match(Accessible* aAcc) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAcc)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (mDirectDescendantsFrom && (aAcc != mDirectDescendantsFrom)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if ([GetNativeFromGeckoAccessible(aAcc) isAccessibilityElement]) {
    result |= nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH) &&
      !mSearchText.IsEmpty()) {
    // If we have a non-empty search text, there are some roles
    // we can safely ignore.
    switch (aAcc->Role()) {
      case roles::LANDMARK:
      case roles::COMBOBOX:
      case roles::LISTITEM:
      case roles::COMBOBOX_LIST:
      case roles::MENUBAR:
      case roles::MENUPOPUP:
      case roles::DOCUMENT:
      case roles::APPLICATION:
        // XXX: These roles either have AXTitle/AXDescription overridden as
        // empty, or should never be returned in search text results. This
        // should be better mapped somewhere.
        result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
        break;
      default:
        nsAutoString name;
        aAcc->Name(name);
        if (!CaseInsensitiveFindInReadable(mSearchText, name)) {
          result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
        }
        break;
    }
  }

  return result;
}

// Rotor Role Rule

RotorRoleRule::RotorRoleRule(role aRole, Accessible* aDirectDescendantsFrom,
                             const nsString& aSearchText)
    : RotorRule(aDirectDescendantsFrom, aSearchText), mRole(aRole){};

RotorRoleRule::RotorRoleRule(role aRole, const nsString& aSearchText)
    : RotorRule(aSearchText), mRole(aRole){};

uint16_t RotorRoleRule::Match(Accessible* aAcc) {
  uint16_t result = RotorRule::Match(aAcc);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH) &&
      aAcc->Role() != mRole) {
    result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  return result;
}

// Rotor Mac Role Rule

RotorMacRoleRule::RotorMacRoleRule(NSString* aMacRole,
                                   Accessible* aDirectDescendantsFrom,
                                   const nsString& aSearchText)
    : RotorRule(aDirectDescendantsFrom, aSearchText), mMacRole(aMacRole) {
  [mMacRole retain];
};

RotorMacRoleRule::RotorMacRoleRule(NSString* aMacRole,
                                   const nsString& aSearchText)
    : RotorRule(aSearchText), mMacRole(aMacRole) {
  [mMacRole retain];
};

RotorMacRoleRule::~RotorMacRoleRule() { [mMacRole release]; }

uint16_t RotorMacRoleRule::Match(Accessible* aAcc) {
  uint16_t result = RotorRule::Match(aAcc);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAcc);
    if (![[nativeMatch moxRole] isEqualToString:mMacRole]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// Rotor Control Rule

RotorControlRule::RotorControlRule(Accessible* aDirectDescendantsFrom,
                                   const nsString& aSearchText)
    : RotorRule(aDirectDescendantsFrom, aSearchText){};

RotorControlRule::RotorControlRule(const nsString& aSearchText)
    : RotorRule(aSearchText){};

uint16_t RotorControlRule::Match(Accessible* aAcc) {
  uint16_t result = RotorRule::Match(aAcc);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    switch (aAcc->Role()) {
      case roles::PUSHBUTTON:
      case roles::SPINBUTTON:
      case roles::DETAILS:
      case roles::CHECKBUTTON:
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
        for (unsigned int i = 0; i < aAcc->ChildCount(); i++) {
          Accessible* currChild = aAcc->ChildAt(i);
          if (currChild->Role() == roles::CHECKBUTTON ||
              currChild->Role() == roles::SWITCH ||
              currChild->Role() == roles::SPINBUTTON ||
              currChild->Role() == roles::RADIOBUTTON) {
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

RotorTextEntryRule::RotorTextEntryRule(Accessible* aDirectDescendantsFrom,
                                       const nsString& aSearchText)
    : RotorRule(aDirectDescendantsFrom, aSearchText){};

RotorTextEntryRule::RotorTextEntryRule(const nsString& aSearchText)
    : RotorRule(aSearchText){};

uint16_t RotorTextEntryRule::Match(Accessible* aAcc) {
  uint16_t result = RotorRule::Match(aAcc);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    if (aAcc->Role() != roles::PASSWORD_TEXT && aAcc->Role() != roles::ENTRY) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// Rotor Link Rule

RotorLinkRule::RotorLinkRule(Accessible* aDirectDescendantsFrom,
                             const nsString& aSearchText)
    : RotorRule(aDirectDescendantsFrom, aSearchText){};

RotorLinkRule::RotorLinkRule(const nsString& aSearchText)
    : RotorRule(aSearchText){};

uint16_t RotorLinkRule::Match(Accessible* aAcc) {
  uint16_t result = RotorRule::Match(aAcc);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAcc);
    if (![[nativeMatch moxRole] isEqualToString:@"AXLink"]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

RotorVisitedLinkRule::RotorVisitedLinkRule(const nsString& aSearchText)
    : RotorLinkRule(aSearchText) {}

RotorVisitedLinkRule::RotorVisitedLinkRule(Accessible* aDirectDescendantsFrom,
                                           const nsString& aSearchText)
    : RotorLinkRule(aDirectDescendantsFrom, aSearchText) {}

uint16_t RotorVisitedLinkRule::Match(Accessible* aAcc) {
  uint16_t result = RotorLinkRule::Match(aAcc);

  if (result & nsIAccessibleTraversalRule::FILTER_MATCH) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAcc);
    if (![[nativeMatch moxVisited] boolValue]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

RotorUnvisitedLinkRule::RotorUnvisitedLinkRule(const nsString& aSearchText)
    : RotorLinkRule(aSearchText) {}

RotorUnvisitedLinkRule::RotorUnvisitedLinkRule(
    Accessible* aDirectDescendantsFrom, const nsString& aSearchText)
    : RotorLinkRule(aDirectDescendantsFrom, aSearchText) {}

uint16_t RotorUnvisitedLinkRule::Match(Accessible* aAcc) {
  uint16_t result = RotorLinkRule::Match(aAcc);

  if (result & nsIAccessibleTraversalRule::FILTER_MATCH) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAcc);
    if ([[nativeMatch moxVisited] boolValue]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// Match Not Rule

RotorNotMacRoleRule::RotorNotMacRoleRule(NSString* aMacRole,
                                         Accessible* aDirectDescendantsFrom,
                                         const nsString& aSearchText)
    : RotorMacRoleRule(aMacRole, aDirectDescendantsFrom, aSearchText) {}

RotorNotMacRoleRule::RotorNotMacRoleRule(NSString* aMacRole,
                                         const nsString& aSearchText)
    : RotorMacRoleRule(aMacRole, aSearchText) {}

uint16_t RotorNotMacRoleRule::Match(Accessible* aAcc) {
  uint16_t result = RotorRule::Match(aAcc);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not different from the desired role, we flip the
  // match bit to "unmatch" otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAcc);
    if ([[nativeMatch moxRole] isEqualToString:mMacRole]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }
  return result;
}

// Rotor Static Text Rule

RotorStaticTextRule::RotorStaticTextRule(Accessible* aDirectDescendantsFrom,
                                         const nsString& aSearchText)
    : RotorRule(aDirectDescendantsFrom, aSearchText){};

RotorStaticTextRule::RotorStaticTextRule(const nsString& aSearchText)
    : RotorRule(aSearchText){};

uint16_t RotorStaticTextRule::Match(Accessible* aAcc) {
  uint16_t result = RotorRule::Match(aAcc);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired role, we flip the match bit to "unmatch"
  // otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAcc);
    if (![[nativeMatch moxRole] isEqualToString:@"AXStaticText"]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// Rotor Heading Level Rule

RotorHeadingLevelRule::RotorHeadingLevelRule(int32_t aLevel,
                                             Accessible* aDirectDescendantsFrom,
                                             const nsString& aSearchText)
    : RotorRoleRule(roles::HEADING, aDirectDescendantsFrom, aSearchText),
      mLevel(aLevel){};

RotorHeadingLevelRule::RotorHeadingLevelRule(int32_t aLevel,
                                             const nsString& aSearchText)
    : RotorRoleRule(roles::HEADING, aSearchText), mLevel(aLevel){};

uint16_t RotorHeadingLevelRule::Match(Accessible* aAcc) {
  uint16_t result = RotorRoleRule::Match(aAcc);

  // if a match was found in the base-class's Match function,
  // it is valid to consider that match again here. if it is
  // not of the desired heading level, we flip the match bit to
  // "unmatch" otherwise, the match persists.
  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    int32_t currLevel = aAcc->GroupPosition().level;

    if (currLevel != mLevel) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

uint16_t RotorLiveRegionRule::Match(Accessible* aAcc) {
  uint16_t result = RotorRule::Match(aAcc);

  if ((result & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    mozAccessible* nativeMatch = GetNativeFromGeckoAccessible(aAcc);
    if (![nativeMatch moxIsLiveRegion]) {
      result &= ~nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }
  return result;
}
