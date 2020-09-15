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

// Role Rules

RotorHeadingRule::RotorHeadingRule() : PivotRoleRule(roles::HEADING) {}

RotorHeadingRule::RotorHeadingRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : PivotRoleRule(roles::HEADING, aDirectDescendantsFrom) {}

RotorArticleRule::RotorArticleRule() : PivotRoleRule(roles::ARTICLE) {}

RotorArticleRule::RotorArticleRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : PivotRoleRule(roles::ARTICLE, aDirectDescendantsFrom) {}

RotorTableRule::RotorTableRule() : PivotRoleRule(roles::TABLE) {}

RotorTableRule::RotorTableRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : PivotRoleRule(roles::TABLE, aDirectDescendantsFrom) {}

RotorLandmarkRule::RotorLandmarkRule() : PivotRoleRule(roles::LANDMARK) {}

RotorLandmarkRule::RotorLandmarkRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : PivotRoleRule(roles::LANDMARK, aDirectDescendantsFrom) {}

RotorButtonRule::RotorButtonRule() : PivotRoleRule(roles::PUSHBUTTON) {}

RotorButtonRule::RotorButtonRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : PivotRoleRule(roles::PUSHBUTTON, aDirectDescendantsFrom) {}

RotorControlRule::RotorControlRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : mDirectDescendantsFrom(aDirectDescendantsFrom) {}

RotorControlRule::RotorControlRule() : mDirectDescendantsFrom(nullptr) {}

uint16_t RotorControlRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAccOrProxy)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (!mDirectDescendantsFrom.IsNull() &&
      (aAccOrProxy != mDirectDescendantsFrom)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

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
      result |= nsIAccessibleTraversalRule::FILTER_MATCH;
      break;

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
          result |= nsIAccessibleTraversalRule::FILTER_MATCH;
          break;
        }
      }
      break;
    }

    case roles::DATE_EDITOR:
    case roles::TIME_EDITOR:
      result |= nsIAccessibleTraversalRule::FILTER_MATCH;
      result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
      break;

    default:
      break;
  }

  return result;
}

// Match All Rule

RotorAllRule::RotorAllRule(AccessibleOrProxy& aDirectDescendantsFrom)
    : mDirectDescendantsFrom(aDirectDescendantsFrom) {}

RotorAllRule::RotorAllRule() : mDirectDescendantsFrom(nullptr) {}

uint16_t RotorAllRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAccOrProxy)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (!mDirectDescendantsFrom.IsNull() &&
      (aAccOrProxy != mDirectDescendantsFrom)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  result |= nsIAccessibleTraversalRule::FILTER_MATCH;

  return result;
}
