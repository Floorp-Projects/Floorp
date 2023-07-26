/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_Pivot_h_
#define mozilla_a11y_Pivot_h_

#include <stdint.h>
#include "mozilla/a11y/Role.h"
#include "mozilla/dom/ChildIterator.h"

namespace mozilla {
namespace a11y {

class DocAccessible;
class Accessible;

class PivotRule {
 public:
  // A filtering function that returns a bitmask from
  // nsIAccessibleTraversalRule: FILTER_IGNORE (0x0): Don't match this
  // accessible. FILTER_MATCH (0x1): Match this accessible FILTER_IGNORE_SUBTREE
  // (0x2): Ignore accessible's subtree.
  virtual uint16_t Match(Accessible* aAcc) = 0;
};

// The Pivot class is used for searching for accessible nodes in a given subtree
// with a given criteria. Since it only holds a weak reference to the root,
// this class is meant to be used primarily on the stack.
class Pivot final {
 public:
  explicit Pivot(Accessible* aRoot);
  Pivot() = delete;
  Pivot(const Pivot&) = delete;
  Pivot& operator=(const Pivot&) = delete;

  ~Pivot();

  // Return the next accessible after aAnchor in pre-order that matches the
  // given rule. If aIncludeStart, return aAnchor if it matches the rule.
  Accessible* Next(Accessible* aAnchor, PivotRule& aRule,
                   bool aIncludeStart = false);

  // Return the previous accessible before aAnchor in pre-order that matches the
  // given rule. If aIncludeStart, return aAnchor if it matches the rule.
  Accessible* Prev(Accessible* aAnchor, PivotRule& aRule,
                   bool aIncludeStart = false);

  // Return the first accessible within the root that matches the pivot rule.
  Accessible* First(PivotRule& aRule);

  // Return the last accessible within the root that matches the pivot rule.
  Accessible* Last(PivotRule& aRule);

  // Return the accessible at the given screen coordinate if it matches the
  // pivot rule.
  Accessible* AtPoint(int32_t aX, int32_t aY, PivotRule& aRule);

 private:
  Accessible* AdjustStartPosition(Accessible* aAnchor, PivotRule& aRule,
                                  uint16_t* aFilterResult);

  // Search in preorder for the first accessible to match the rule.
  Accessible* SearchForward(Accessible* aAnchor, PivotRule& aRule,
                            bool aSearchCurrent);

  // Reverse search in preorder for the first accessible to match the rule.
  Accessible* SearchBackward(Accessible* aAnchor, PivotRule& aRule,
                             bool aSearchCurrent);

  Accessible* mRoot;
};

/**
 * This rule matches accessibles on a given role, filtering out non-direct
 * descendants if necessary.
 */
class PivotRoleRule : public PivotRule {
 public:
  explicit PivotRoleRule(role aRole);
  explicit PivotRoleRule(role aRole, Accessible* aDirectDescendantsFrom);

  virtual uint16_t Match(Accessible* aAcc) override;

 protected:
  role mRole;
  Accessible* mDirectDescendantsFrom;
};

/**
 * This rule matches accessibles with a given state.
 */
class PivotStateRule : public PivotRule {
 public:
  explicit PivotStateRule(uint64_t aState);

  virtual uint16_t Match(Accessible* aAcc) override;

 protected:
  uint64_t mState;
};

/**
 * This rule matches any local LocalAccessible (i.e. not RemoteAccessible) in
 * the same document as the anchor. That is, it includes any descendant
 * OuterDocAccessible, but not its descendants.
 */
class LocalAccInSameDocRule : public PivotRule {
 public:
  virtual uint16_t Match(Accessible* aAcc) override;
};

/**
 * This rule matches remote radio button accessibles with the given name
 * attribute. It assumes the cache is enabled.
 */
class PivotRadioNameRule : public PivotRule {
 public:
  explicit PivotRadioNameRule(const nsString& aName);

  virtual uint16_t Match(Accessible* aAcc) override;

 protected:
  const nsString& mName;
};

/**
 * This rule doesn't search iframes. Subtrees that should be
 * pruned by way of nsAccUtils::MustPrune are also not searched.
 */

class MustPruneSameDocRule : public PivotRule {
 public:
  virtual uint16_t Match(Accessible* aAcc) override;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_Pivot_h_
