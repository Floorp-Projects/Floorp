/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_Pivot_h_
#define mozilla_a11y_Pivot_h_

#include <stdint.h>
#include "Role.h"
#include "mozilla/dom/ChildIterator.h"
#include "AccessibleOrProxy.h"

namespace mozilla {
namespace a11y {

class Accessible;
class HyperTextAccessible;
class DocAccessible;

class PivotRule {
 public:
  // A filtering function that returns a bitmask from
  // nsIAccessibleTraversalRule: FILTER_IGNORE (0x0): Don't match this
  // accessible. FILTER_MATCH (0x1): Match this accessible FILTER_IGNORE_SUBTREE
  // (0x2): Ignore accessible's subtree.
  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) = 0;
};

// The Pivot class is used for searching for accessible nodes in a given subtree
// with a given criteria. Since it only holds a weak reference to the root,
// this class is meant to be used primarily on the stack.
class Pivot final {
 public:
  explicit Pivot(const AccessibleOrProxy& aRoot);
  Pivot() = delete;
  Pivot(const Pivot&) = delete;
  Pivot& operator=(const Pivot&) = delete;

  ~Pivot();

  // Return the next accessible after aAnchor in pre-order that matches the
  // given rule. If aIncludeStart, return aAnchor if it matches the rule.
  AccessibleOrProxy Next(AccessibleOrProxy& aAnchor, PivotRule& aRule,
                         bool aIncludeStart = false);

  // Return the previous accessible before aAnchor in pre-order that matches the
  // given rule. If aIncludeStart, return aAnchor if it matches the rule.
  AccessibleOrProxy Prev(AccessibleOrProxy& aAnchor, PivotRule& aRule,
                         bool aIncludeStart = false);

  // Return the first accessible within the root that matches the pivot rule.
  AccessibleOrProxy First(PivotRule& aRule);

  // Return the last accessible within the root that matches the pivot rule.
  AccessibleOrProxy Last(PivotRule& aRule);

  // Return the next range of text according to the boundary type.
  Accessible* NextText(Accessible* aAnchor, int32_t* aStartOffset,
                       int32_t* aEndOffset, int32_t aBoundaryType);

  // Return the previous range of text according to the boundary type.
  Accessible* PrevText(Accessible* aAnchor, int32_t* aStartOffset,
                       int32_t* aEndOffset, int32_t aBoundaryType);

  // Return the accessible at the given screen coordinate if it matches the
  // pivot rule.
  AccessibleOrProxy AtPoint(int32_t aX, int32_t aY, PivotRule& aRule);

 private:
  AccessibleOrProxy AdjustStartPosition(AccessibleOrProxy& aAnchor,
                                        PivotRule& aRule,
                                        uint16_t* aFilterResult);

  // Search in preorder for the first accessible to match the rule.
  AccessibleOrProxy SearchForward(AccessibleOrProxy& aAnchor, PivotRule& aRule,
                                  bool aSearchCurrent);

  // Reverse search in preorder for the first accessible to match the rule.
  AccessibleOrProxy SearchBackward(AccessibleOrProxy& aAnchor, PivotRule& aRule,
                                   bool aSearchCurrent);

  // Search in preorder for the first text accessible.
  HyperTextAccessible* SearchForText(Accessible* aAnchor, bool aBackward);

  AccessibleOrProxy mRoot;
};

/**
 * This rule matches accessibles on a given role.
 */
class PivotRoleRule final : public PivotRule {
 public:
  explicit PivotRoleRule(role aRole);

  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;

 private:
  role mRole;
};

/**
 * This rule matches all accessibles.
 */
class PivotMatchAllRule final : public PivotRule {
 public:
  virtual uint16_t Match(const AccessibleOrProxy& aAccOrProxy) override;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_Pivot_h_
