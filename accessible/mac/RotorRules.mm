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
