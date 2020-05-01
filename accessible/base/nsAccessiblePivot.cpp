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

  virtual uint16_t Match(Accessible* aAccessible) override;

 private:
  nsCOMPtr<nsIAccessibleTraversalRule> mRule;
  Maybe<nsTArray<uint32_t>> mAcceptRoles;
  uint32_t mPreFilter;
};

////////////////////////////////////////////////////////////////////////////////
// nsAccessiblePivot

nsAccessiblePivot::nsAccessiblePivot(Accessible* aRoot)
    : mRoot(aRoot),
      mModalRoot(nullptr),
      mPosition(nullptr),
      mStartOffset(-1),
      mEndOffset(-1) {
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
  RefPtr<Accessible> position = nullptr;

  if (aPosition) {
    position = aPosition->ToInternalAccessible();
    if (!position || !IsDescendantOf(position, GetActiveRoot()))
      return NS_ERROR_INVALID_ARG;
  }

  // Swap old position with new position, saves us an AddRef/Release.
  mPosition.swap(position);
  int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = mEndOffset = -1;
  NotifyOfPivotChange(position, oldStart, oldEnd,
                      nsIAccessiblePivot::REASON_NONE,
                      nsIAccessiblePivot::NO_BOUNDARY, false);

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
  Accessible* modalRoot = nullptr;

  if (aModalRoot) {
    modalRoot = aModalRoot->ToInternalAccessible();
    if (!modalRoot || !IsDescendantOf(modalRoot, mRoot))
      return NS_ERROR_INVALID_ARG;
  }

  mModalRoot = modalRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetStartOffset(int32_t* aStartOffset) {
  NS_ENSURE_ARG_POINTER(aStartOffset);

  *aStartOffset = mStartOffset;

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetEndOffset(int32_t* aEndOffset) {
  NS_ENSURE_ARG_POINTER(aEndOffset);

  *aEndOffset = mEndOffset;

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetTextRange(nsIAccessibleText* aTextAccessible,
                                int32_t aStartOffset, int32_t aEndOffset,
                                bool aIsFromUserInput, uint8_t aArgc) {
  NS_ENSURE_ARG(aTextAccessible);

  // Check that start offset is smaller than end offset, and that if a value is
  // smaller than 0, both should be -1.
  NS_ENSURE_TRUE(
      aStartOffset <= aEndOffset &&
          (aStartOffset >= 0 || (aStartOffset != -1 && aEndOffset != -1)),
      NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIAccessible> xpcAcc = do_QueryInterface(aTextAccessible);
  NS_ENSURE_ARG(xpcAcc);

  RefPtr<Accessible> acc = xpcAcc->ToInternalAccessible();
  NS_ENSURE_ARG(acc);

  HyperTextAccessible* position = acc->AsHyperText();
  if (!position || !IsDescendantOf(position, GetActiveRoot()))
    return NS_ERROR_INVALID_ARG;

  // Make sure the given offsets don't exceed the character count.
  if (aEndOffset > static_cast<int32_t>(position->CharacterCount()))
    return NS_ERROR_FAILURE;

  int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = aStartOffset;
  mEndOffset = aEndOffset;

  mPosition.swap(acc);
  NotifyOfPivotChange(acc, oldStart, oldEnd, nsIAccessiblePivot::REASON_NONE,
                      nsIAccessiblePivot::NO_BOUNDARY,
                      (aArgc > 0) ? aIsFromUserInput : true);

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

  Accessible* anchor = mPosition;
  if (aArgc > 0 && aAnchor) anchor = aAnchor->ToInternalAccessible();

  if (anchor &&
      (anchor->IsDefunct() || !IsDescendantOf(anchor, GetActiveRoot())))
    return NS_ERROR_NOT_IN_TREE;

  Pivot pivot(GetActiveRoot());
  RuleCache rule(aRule);

  if (Accessible* newPos =
          pivot.Next(anchor, rule, (aArgc > 1) ? aIncludeStart : false)) {
    *aResult = MovePivotInternal(newPos, nsIAccessiblePivot::REASON_NEXT,
                                 (aArgc > 2) ? aIsFromUserInput : true);
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

  Accessible* anchor = mPosition;
  if (aArgc > 0 && aAnchor) anchor = aAnchor->ToInternalAccessible();

  if (anchor &&
      (anchor->IsDefunct() || !IsDescendantOf(anchor, GetActiveRoot())))
    return NS_ERROR_NOT_IN_TREE;

  Pivot pivot(GetActiveRoot());
  RuleCache rule(aRule);

  if (Accessible* newPos =
          pivot.Prev(anchor, rule, (aArgc > 1) ? aIncludeStart : false)) {
    *aResult = MovePivotInternal(newPos, nsIAccessiblePivot::REASON_PREV,
                                 (aArgc > 2) ? aIsFromUserInput : true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveFirst(nsIAccessibleTraversalRule* aRule,
                             bool aIsFromUserInput, uint8_t aArgc,
                             bool* aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  Accessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  Pivot pivot(GetActiveRoot());
  RuleCache rule(aRule);

  if (Accessible* newPos = pivot.First(rule)) {
    *aResult = MovePivotInternal(newPos, nsIAccessiblePivot::REASON_FIRST,
                                 (aArgc > 0) ? aIsFromUserInput : true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveLast(nsIAccessibleTraversalRule* aRule,
                            bool aIsFromUserInput, uint8_t aArgc,
                            bool* aResult) {
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  Accessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  Pivot pivot(root);
  RuleCache rule(aRule);

  if (Accessible* newPos = pivot.Last(rule)) {
    *aResult = MovePivotInternal(newPos, nsIAccessiblePivot::REASON_LAST,
                                 (aArgc > 0) ? aIsFromUserInput : true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveNextByText(TextBoundaryType aBoundary,
                                  bool aIsFromUserInput, uint8_t aArgc,
                                  bool* aResult) {
  NS_ENSURE_ARG(aResult);

  *aResult = false;

  Pivot pivot(GetActiveRoot());

  int32_t newStart = mStartOffset, newEnd = mEndOffset;
  if (Accessible* newPos =
          pivot.NextText(mPosition, &newStart, &newEnd, aBoundary)) {
    *aResult = true;
    int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
    Accessible* oldPos = mPosition;
    mStartOffset = newStart;
    mEndOffset = newEnd;
    mPosition = newPos;
    NotifyOfPivotChange(oldPos, oldStart, oldEnd,
                        nsIAccessiblePivot::REASON_NEXT, aBoundary,
                        (aArgc > 0) ? aIsFromUserInput : true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MovePreviousByText(TextBoundaryType aBoundary,
                                      bool aIsFromUserInput, uint8_t aArgc,
                                      bool* aResult) {
  NS_ENSURE_ARG(aResult);

  *aResult = false;

  Pivot pivot(GetActiveRoot());

  int32_t newStart = mStartOffset, newEnd = mEndOffset;
  if (Accessible* newPos =
          pivot.PrevText(mPosition, &newStart, &newEnd, aBoundary)) {
    *aResult = true;
    int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
    Accessible* oldPos = mPosition;
    mStartOffset = newStart;
    mEndOffset = newEnd;
    mPosition = newPos;
    NotifyOfPivotChange(oldPos, oldStart, oldEnd,
                        nsIAccessiblePivot::REASON_PREV, aBoundary,
                        (aArgc > 0) ? aIsFromUserInput : true);
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

  Accessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  RuleCache rule(aRule);
  Pivot pivot(root);

  Accessible* newPos = pivot.AtPoint(aX, aY, rule);
  if (newPos || !aIgnoreNoMatch) {
    *aResult = MovePivotInternal(newPos, nsIAccessiblePivot::REASON_POINT,
                                 (aArgc > 0) ? aIsFromUserInput : true);
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

bool nsAccessiblePivot::IsDescendantOf(Accessible* aAccessible,
                                       Accessible* aAncestor) {
  if (!aAncestor || aAncestor->IsDefunct()) return false;

  // XXX Optimize with IsInDocument() when appropriate. Blocked by bug 759875.
  Accessible* accessible = aAccessible;
  do {
    if (accessible == aAncestor) return true;
  } while ((accessible = accessible->Parent()));

  return false;
}

bool nsAccessiblePivot::MovePivotInternal(Accessible* aPosition,
                                          PivotMoveReason aReason,
                                          bool aIsFromUserInput) {
  RefPtr<Accessible> oldPosition = std::move(mPosition);
  mPosition = aPosition;
  int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = mEndOffset = -1;

  return NotifyOfPivotChange(oldPosition, oldStart, oldEnd, aReason,
                             nsIAccessiblePivot::NO_BOUNDARY, aIsFromUserInput);
}

bool nsAccessiblePivot::NotifyOfPivotChange(Accessible* aOldPosition,
                                            int32_t aOldStart, int32_t aOldEnd,
                                            int16_t aReason,
                                            int16_t aBoundaryType,
                                            bool aIsFromUserInput) {
  if (aOldPosition == mPosition && aOldStart == mStartOffset &&
      aOldEnd == mEndOffset)
    return false;

  nsCOMPtr<nsIAccessible> xpcOldPos = ToXPC(aOldPosition);  // death grip
  nsTObserverArray<nsCOMPtr<nsIAccessiblePivotObserver>>::ForwardIterator iter(
      mObservers);
  while (iter.HasMore()) {
    nsIAccessiblePivotObserver* obs = iter.GetNext();
    obs->OnPivotChanged(this, xpcOldPos, aOldStart, aOldEnd, ToXPC(mPosition),
                        mStartOffset, mEndOffset, aReason, aBoundaryType,
                        aIsFromUserInput);
  }

  return true;
}

uint16_t RuleCache::Match(Accessible* aAccessible) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (!mAcceptRoles) {
    mAcceptRoles.emplace();
    DebugOnly<nsresult> rv = mRule->GetMatchRoles(*mAcceptRoles);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = mRule->GetPreFilter(&mPreFilter);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (mPreFilter) {
    uint64_t state = aAccessible->State();

    if ((nsIAccessibleTraversalRule::PREFILTER_PLATFORM_PRUNED & mPreFilter) &&
        nsAccUtils::MustPrune(aAccessible)) {
      result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }

    if ((nsIAccessibleTraversalRule::PREFILTER_INVISIBLE & mPreFilter) &&
        (state & states::INVISIBLE))
      return result;

    if ((nsIAccessibleTraversalRule::PREFILTER_OFFSCREEN & mPreFilter) &&
        (state & states::OFFSCREEN))
      return result;

    if ((nsIAccessibleTraversalRule::PREFILTER_NOT_FOCUSABLE & mPreFilter) &&
        !(state & states::FOCUSABLE))
      return result;

    if ((nsIAccessibleTraversalRule::PREFILTER_TRANSPARENT & mPreFilter) &&
        !(state & states::OPAQUE1)) {
      nsIFrame* frame = aAccessible->GetFrame();
      if (frame->StyleEffects()->mOpacity == 0.0f) {
        return result | nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
      }
    }
  }

  if (mAcceptRoles->Length() > 0) {
    uint32_t accessibleRole = aAccessible->Role();
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
  DebugOnly<nsresult> rv = mRule->Match(ToXPC(aAccessible), &matchResult);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return result | matchResult;
}
