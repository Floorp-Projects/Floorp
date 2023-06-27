/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessiblePivot.h"
#include "xpcAccessibleDocument.h"

#include "Pivot.h"

using namespace mozilla::a11y;

using mozilla::DebugOnly;

/**
 * An object that stores a given traversal rule during the pivot movement.
 */
class xpcPivotRule : public PivotRule {
 public:
  explicit xpcPivotRule(nsIAccessibleTraversalRule* aRule) : mRule(aRule) {}
  ~xpcPivotRule() {}

  virtual uint16_t Match(Accessible* aAcc) override;

 private:
  nsCOMPtr<nsIAccessibleTraversalRule> mRule;
};

////////////////////////////////////////////////////////////////////////////////
// xpcAccessiblePivot

xpcAccessiblePivot::xpcAccessiblePivot(nsIAccessible* aRoot) : mRoot(aRoot) {
  NS_ASSERTION(aRoot, "A root accessible is required");
}

xpcAccessiblePivot::~xpcAccessiblePivot() {}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_CYCLE_COLLECTION(xpcAccessiblePivot, mRoot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(xpcAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY(nsIAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessiblePivot)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(xpcAccessiblePivot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(xpcAccessiblePivot)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessiblePivot

NS_IMETHODIMP
xpcAccessiblePivot::Next(nsIAccessible* aAnchor,
                         nsIAccessibleTraversalRule* aRule, bool aIncludeStart,
                         uint8_t aArgc, nsIAccessible** aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  Accessible* root = Root();
  Accessible* anchor = aAnchor->ToInternalGeneric();
  NS_ENSURE_TRUE(root && anchor, NS_ERROR_NOT_IN_TREE);

  Pivot pivot(Root());
  xpcPivotRule rule(aRule);
  Accessible* result =
      pivot.Next(anchor, rule, (aArgc > 0) ? aIncludeStart : false);
  NS_IF_ADDREF(*aResult = ToXPC(result));

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessiblePivot::Prev(nsIAccessible* aAnchor,
                         nsIAccessibleTraversalRule* aRule, bool aIncludeStart,
                         uint8_t aArgc, nsIAccessible** aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  Accessible* root = Root();
  Accessible* anchor = aAnchor->ToInternalGeneric();
  NS_ENSURE_TRUE(root && anchor, NS_ERROR_NOT_IN_TREE);

  Pivot pivot(Root());
  xpcPivotRule rule(aRule);
  Accessible* result =
      pivot.Prev(anchor, rule, (aArgc > 0) ? aIncludeStart : false);
  NS_IF_ADDREF(*aResult = ToXPC(result));

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessiblePivot::First(nsIAccessibleTraversalRule* aRule,
                          nsIAccessible** aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  Accessible* root = Root();
  NS_ENSURE_TRUE(root, NS_ERROR_NOT_IN_TREE);

  Pivot pivot(root);
  xpcPivotRule rule(aRule);
  Accessible* result = pivot.First(rule);
  NS_IF_ADDREF(*aResult = ToXPC(result));

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessiblePivot::Last(nsIAccessibleTraversalRule* aRule,
                         nsIAccessible** aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  Accessible* root = Root();
  NS_ENSURE_TRUE(root, NS_ERROR_NOT_IN_TREE);

  Pivot pivot(root);
  xpcPivotRule rule(aRule);
  Accessible* result = pivot.Last(rule);
  NS_IF_ADDREF(*aResult = ToXPC(result));

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessiblePivot::AtPoint(int32_t aX, int32_t aY,
                            nsIAccessibleTraversalRule* aRule,
                            nsIAccessible** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aRule);

  Accessible* root = Root();
  NS_ENSURE_TRUE(root, NS_ERROR_NOT_IN_TREE);

  xpcPivotRule rule(aRule);
  Pivot pivot(root);

  Accessible* result = pivot.AtPoint(aX, aY, rule);
  NS_IF_ADDREF(*aResult = ToXPC(result));

  return NS_OK;
}

uint16_t xpcPivotRule::Match(Accessible* aAcc) {
  uint16_t matchResult = nsIAccessibleTraversalRule::FILTER_IGNORE;

  DebugOnly<nsresult> rv = mRule->Match(ToXPC(aAcc), &matchResult);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return matchResult;
}
