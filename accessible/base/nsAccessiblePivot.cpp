/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessiblePivot.h"

#include "HyperTextAccessible.h"
#include "nsAccUtils.h"
#include "States.h"
#include "Pivot.h"
#include "xpcAccessibleDocument.h"
#include "nsTArray.h"
#include "mozilla/Maybe.h"

using namespace mozilla::a11y;
using mozilla::DebugOnly;
using mozilla::Maybe;

/**
 * An object that stores a given traversal rule during the pivot movement.
 */
class RuleCache : public PivotRule {
 public:
  explicit RuleCache(nsIAccessibleTraversalRule* aRule)
      : mRule(aRule), mPreFilter{0} {}
  ~RuleCache() {}

  virtual uint16_t Match(Accessible* aAcc) override;

 private:
  nsCOMPtr<nsIAccessibleTraversalRule> mRule;
  Maybe<nsTArray<uint32_t>> mAcceptRoles;
  uint32_t mPreFilter;
};

////////////////////////////////////////////////////////////////////////////////
// nsAccessiblePivot

nsAccessiblePivot::nsAccessiblePivot(LocalAccessible* aRoot)
    : mRoot(aRoot), mModalRoot(nullptr), mPosition(nullptr) {
  NS_ASSERTION(aRoot, "A root accessible is required");
}

nsAccessiblePivot::~nsAccessiblePivot() {}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_CYCLE_COLLECTION(nsAccessiblePivot, mRoot, mPosition, mObservers)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY(nsIAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessiblePivot)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAccessiblePivot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAccessiblePivot)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessiblePivot

NS_IMETHODIMP
nsAccessiblePivot::GetRoot(nsIAccessible** aRoot) {
  NS_ENSURE_ARG_POINTER(aRoot);

  NS_IF_ADDREF(*aRoot = ToXPC(mRoot));

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetPosition(nsIAccessible** aPosition) {
  NS_ENSURE_ARG_POINTER(aPosition);

  NS_IF_ADDREF(*aPosition = ToXPC(mPosition));

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetPosition(nsIAccessible* aPosition) {
  RefPtr<LocalAccessible> position = nullptr;

  if (aPosition) {
    position = aPosition->ToInternalAccessible();
    if (!position || !IsDescendantOf(position, GetActiveRoot())) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  // Swap old position with new position, saves us an AddRef/Release.
  mPosition.swap(position);
  NotifyOfPivotChange(position, nsIAccessiblePivot::REASON_NONE, false);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetModalRoot(nsIAccessible** aModalRoot) {
  NS_ENSURE_ARG_POINTER(aModalRoot);

  NS_IF_ADDREF(*aModalRoot = ToXPC(mModalRoot));

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetModalRoot(nsIAccessible* aModalRoot) {
  LocalAccessible* modalRoot = nullptr;

  if (aModalRoot) {
    modalRoot = aModalRoot->ToInternalAccessible();
    if (!modalRoot || !IsDescendantOf(modalRoot, mRoot)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  mModalRoot = modalRoot;
  return NS_OK;
}

// Traversal functions

NS_IMETHODIMP
nsAccessiblePivot::MoveNext(nsIAccessibleTraversalRule* aRule,
                            nsIAccessible* aAnchor, bool aIncludeStart,
                            bool aIsFromUserInput, uint8_t aArgc,
                            bool* aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);
  *aResult = false;

  LocalAccessible* anchor = mPosition;
  if (aArgc > 0 && aAnchor) anchor = aAnchor->ToInternalAccessible();

  if (anchor &&
      (anchor->IsDefunct() || !IsDescendantOf(anchor, GetActiveRoot()))) {
    return NS_ERROR_NOT_IN_TREE;
  }

  Pivot pivot(GetActiveRoot());
  RuleCache rule(aRule);
  Accessible* newPos =
      pivot.Next(anchor, rule, (aArgc > 1) ? aIncludeStart : false);
  if (newPos && newPos->IsLocal()) {
    *aResult =
        MovePivotInternal(newPos->AsLocal(), nsIAccessiblePivot::REASON_NEXT,
                          (aArgc > 2) ? aIsFromUserInput : true);
  } else if (newPos && newPos->IsRemote()) {
    // we shouldn't ever end up with a proxy here, but if we do for some
    // reason something is wrong. we should still return OK if we're null
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MovePrevious(nsIAccessibleTraversalRule* aRule,
                                nsIAccessible* aAnchor, bool aIncludeStart,
                                bool aIsFromUserInput, uint8_t aArgc,
                                bool* aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);
  *aResult = false;

  LocalAccessible* anchor = mPosition;
  if (aArgc > 0 && aAnchor) anchor = aAnchor->ToInternalAccessible();

  if (anchor &&
      (anchor->IsDefunct() || !IsDescendantOf(anchor, GetActiveRoot()))) {
    return NS_ERROR_NOT_IN_TREE;
  }

  Pivot pivot(GetActiveRoot());
  RuleCache rule(aRule);
  Accessible* newPos =
      pivot.Prev(anchor, rule, (aArgc > 1) ? aIncludeStart : false);
  if (newPos && newPos->IsLocal()) {
    *aResult =
        MovePivotInternal(newPos->AsLocal(), nsIAccessiblePivot::REASON_PREV,
                          (aArgc > 2) ? aIsFromUserInput : true);
  } else if (newPos && newPos->IsRemote()) {
    // we shouldn't ever end up with a proxy here, but if we do for some
    // reason something is wrong. we should still return OK if we're null
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveFirst(nsIAccessibleTraversalRule* aRule,
                             bool aIsFromUserInput, uint8_t aArgc,
                             bool* aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  LocalAccessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  Pivot pivot(GetActiveRoot());
  RuleCache rule(aRule);
  Accessible* newPos = pivot.First(rule);
  if (newPos && newPos->IsLocal()) {
    *aResult =
        MovePivotInternal(newPos->AsLocal(), nsIAccessiblePivot::REASON_FIRST,
                          (aArgc > 0) ? aIsFromUserInput : true);
  } else if (newPos && newPos->IsRemote()) {
    // we shouldn't ever end up with a proxy here, but if we do for some
    // reason something is wrong. we should still return OK if we're null
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveLast(nsIAccessibleTraversalRule* aRule,
                            bool aIsFromUserInput, uint8_t aArgc,
                            bool* aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  LocalAccessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  Pivot pivot(root);
  RuleCache rule(aRule);
  Accessible* newPos = pivot.Last(rule);
  if (newPos && newPos->IsLocal()) {
    *aResult =
        MovePivotInternal(newPos->AsLocal(), nsIAccessiblePivot::REASON_LAST,
                          (aArgc > 0) ? aIsFromUserInput : true);
  } else if (newPos && newPos->IsRemote()) {
    // we shouldn't ever end up with a proxy here, but if we do for some
    // reason something is wrong. we should still return OK if we're null
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveToPoint(nsIAccessibleTraversalRule* aRule, int32_t aX,
                               int32_t aY, bool aIgnoreNoMatch,
                               bool aIsFromUserInput, uint8_t aArgc,
                               bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aRule);

  *aResult = false;

  LocalAccessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  RuleCache rule(aRule);
  Pivot pivot(root);

  Accessible* newPos = pivot.AtPoint(aX, aY, rule);
  if ((newPos && newPos->IsLocal()) ||
      !aIgnoreNoMatch) {  // TODO does this need a proxy check?
    *aResult = MovePivotInternal(newPos ? newPos->AsLocal() : nullptr,
                                 nsIAccessiblePivot::REASON_POINT,
                                 (aArgc > 0) ? aIsFromUserInput : true);
  } else if (newPos && newPos->IsRemote()) {
    // we shouldn't ever end up with a proxy here, but if we do for some
    // reason something is wrong. we should still return OK if we're null
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// Observer functions

NS_IMETHODIMP
nsAccessiblePivot::AddObserver(nsIAccessiblePivotObserver* aObserver) {
  NS_ENSURE_ARG(aObserver);

  mObservers.AppendElement(aObserver);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::RemoveObserver(nsIAccessiblePivotObserver* aObserver) {
  NS_ENSURE_ARG(aObserver);

  return mObservers.RemoveElement(aObserver) ? NS_OK : NS_ERROR_FAILURE;
}

// Private utility methods

bool nsAccessiblePivot::IsDescendantOf(LocalAccessible* aAccessible,
                                       LocalAccessible* aAncestor) {
  if (!aAncestor || aAncestor->IsDefunct()) return false;

  // XXX Optimize with IsInDocument() when appropriate. Blocked by bug 759875.
  LocalAccessible* accessible = aAccessible;
  do {
    if (accessible == aAncestor) return true;
  } while ((accessible = accessible->LocalParent()));

  return false;
}

bool nsAccessiblePivot::MovePivotInternal(LocalAccessible* aPosition,
                                          PivotMoveReason aReason,
                                          bool aIsFromUserInput) {
  RefPtr<LocalAccessible> oldPosition = std::move(mPosition);
  mPosition = aPosition;

  return NotifyOfPivotChange(oldPosition, aReason, aIsFromUserInput);
}

bool nsAccessiblePivot::NotifyOfPivotChange(LocalAccessible* aOldPosition,
                                            int16_t aReason,
                                            bool aIsFromUserInput) {
  if (aOldPosition == mPosition) {
    return false;
  }

  nsCOMPtr<nsIAccessible> xpcOldPos = ToXPC(aOldPosition);  // death grip
  for (nsIAccessiblePivotObserver* obs : mObservers.ForwardRange()) {
    obs->OnPivotChanged(this, xpcOldPos, ToXPC(mPosition), aReason,
                        aIsFromUserInput);
  }

  return true;
}

uint16_t RuleCache::Match(Accessible* aAcc) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (!mAcceptRoles) {
    mAcceptRoles.emplace();
    DebugOnly<nsresult> rv = mRule->GetMatchRoles(*mAcceptRoles);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = mRule->GetPreFilter(&mPreFilter);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (mPreFilter) {
    uint64_t state = aAcc->State();

    if ((nsIAccessibleTraversalRule::PREFILTER_PLATFORM_PRUNED & mPreFilter) &&
        nsAccUtils::MustPrune(aAcc)) {
      result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }

    if ((nsIAccessibleTraversalRule::PREFILTER_INVISIBLE & mPreFilter) &&
        (state & states::INVISIBLE)) {
      return result;
    }

    if ((nsIAccessibleTraversalRule::PREFILTER_OFFSCREEN & mPreFilter) &&
        (state & states::OFFSCREEN)) {
      return result;
    }

    if ((nsIAccessibleTraversalRule::PREFILTER_NOT_FOCUSABLE & mPreFilter) &&
        !(state & states::FOCUSABLE)) {
      return result;
    }

    if (nsIAccessibleTraversalRule::PREFILTER_TRANSPARENT & mPreFilter) {
      if (aAcc->Opacity() == 0.0f) {
        return result | nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
      }
    }
  }

  if (mAcceptRoles->Length() > 0) {
    uint32_t accessibleRole = aAcc->Role();
    bool matchesRole = false;
    for (uint32_t idx = 0; idx < mAcceptRoles->Length(); idx++) {
      matchesRole = mAcceptRoles->ElementAt(idx) == accessibleRole;
      if (matchesRole) break;
    }

    if (!matchesRole) {
      return result;
    }
  }

  uint16_t matchResult = nsIAccessibleTraversalRule::FILTER_IGNORE;

  DebugOnly<nsresult> rv = mRule->Match(ToXPC(aAcc), &matchResult);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return result | matchResult;
}
