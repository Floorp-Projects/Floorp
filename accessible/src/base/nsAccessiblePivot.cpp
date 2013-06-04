/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessiblePivot.h"

#include "DocAccessible.h"
#include "HyperTextAccessible.h"
#include "nsAccUtils.h"
#include "States.h"

#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsISupportsPrimitives.h"

using namespace mozilla::a11y;


/**
 * An object that stores a given traversal rule during 
 */
class RuleCache
{
public:
  RuleCache(nsIAccessibleTraversalRule* aRule) : mRule(aRule),
                                                 mAcceptRoles(nullptr) { }
  ~RuleCache () {
    if (mAcceptRoles)
      nsMemory::Free(mAcceptRoles);
  }

  nsresult ApplyFilter(Accessible* aAccessible, uint16_t* aResult);

private:
  nsCOMPtr<nsIAccessibleTraversalRule> mRule;
  uint32_t* mAcceptRoles;
  uint32_t mAcceptRolesLength;
  uint32_t mPreFilter;
};

////////////////////////////////////////////////////////////////////////////////
// nsAccessiblePivot

nsAccessiblePivot::nsAccessiblePivot(Accessible* aRoot) :
  mRoot(aRoot), mModalRoot(nullptr), mPosition(nullptr),
  mStartOffset(-1), mEndOffset(-1)
{
  NS_ASSERTION(aRoot, "A root accessible is required");
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_CYCLE_COLLECTION_3(nsAccessiblePivot, mRoot, mPosition, mObservers)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY(nsIAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY(nsAccessiblePivot)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessiblePivot)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAccessiblePivot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAccessiblePivot)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessiblePivot

NS_IMETHODIMP
nsAccessiblePivot::GetRoot(nsIAccessible** aRoot)
{
  NS_ENSURE_ARG_POINTER(aRoot);

  NS_IF_ADDREF(*aRoot = mRoot);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetPosition(nsIAccessible** aPosition)
{
  NS_ENSURE_ARG_POINTER(aPosition);

  NS_IF_ADDREF(*aPosition = mPosition);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetPosition(nsIAccessible* aPosition)
{
  nsRefPtr<Accessible> secondPosition;

  if (aPosition) {
    secondPosition = do_QueryObject(aPosition);
    if (!secondPosition || !IsDescendantOf(secondPosition, GetActiveRoot()))
      return NS_ERROR_INVALID_ARG;
  }

  // Swap old position with new position, saves us an AddRef/Release.
  mPosition.swap(secondPosition);
  int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = mEndOffset = -1;
  NotifyOfPivotChange(secondPosition, oldStart, oldEnd,
                      nsIAccessiblePivot::REASON_NONE);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetModalRoot(nsIAccessible** aModalRoot)
{
  NS_ENSURE_ARG_POINTER(aModalRoot);

  NS_IF_ADDREF(*aModalRoot = mModalRoot);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetModalRoot(nsIAccessible* aModalRoot)
{
  nsRefPtr<Accessible> modalRoot;

  if (aModalRoot) {
    modalRoot = do_QueryObject(aModalRoot);
    if (!modalRoot || !IsDescendantOf(modalRoot, mRoot))
      return NS_ERROR_INVALID_ARG;
  }

  mModalRoot.swap(modalRoot);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetStartOffset(int32_t* aStartOffset)
{
  NS_ENSURE_ARG_POINTER(aStartOffset);

  *aStartOffset = mStartOffset;

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetEndOffset(int32_t* aEndOffset)
{
  NS_ENSURE_ARG_POINTER(aEndOffset);

  *aEndOffset = mEndOffset;

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetTextRange(nsIAccessibleText* aTextAccessible,
                                int32_t aStartOffset, int32_t aEndOffset)
{
  NS_ENSURE_ARG(aTextAccessible);

  // Check that start offset is smaller than end offset, and that if a value is
  // smaller than 0, both should be -1.
  NS_ENSURE_TRUE(aStartOffset <= aEndOffset &&
                 (aStartOffset >= 0 || (aStartOffset != -1 && aEndOffset != -1)),
                 NS_ERROR_INVALID_ARG);

  nsRefPtr<Accessible> acc(do_QueryObject(aTextAccessible));
  if (!acc)
    return NS_ERROR_INVALID_ARG;

  HyperTextAccessible* newPosition = acc->AsHyperText();
  if (!newPosition || !IsDescendantOf(newPosition, GetActiveRoot()))
    return NS_ERROR_INVALID_ARG;

  // Make sure the given offsets don't exceed the character count.
  int32_t charCount = newPosition->CharacterCount();

  if (aEndOffset > charCount)
    return NS_ERROR_FAILURE;

  int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = aStartOffset;
  mEndOffset = aEndOffset;

  nsRefPtr<Accessible> oldPosition = mPosition.forget();
  mPosition = newPosition;

  NotifyOfPivotChange(oldPosition, oldStart, oldEnd,
                      nsIAccessiblePivot::REASON_TEXT);

  return NS_OK;
}

// Traversal functions

NS_IMETHODIMP
nsAccessiblePivot::MoveNext(nsIAccessibleTraversalRule* aRule,
                            nsIAccessible* aAnchor, bool aIncludeStart,
                            uint8_t aArgc, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  *aResult = false;

  Accessible* root = GetActiveRoot();
  nsRefPtr<Accessible> anchor =
    (aArgc > 0) ? do_QueryObject(aAnchor) : mPosition;
  if (anchor && (anchor->IsDefunct() || !IsDescendantOf(anchor, root)))
    return NS_ERROR_NOT_IN_TREE;

  nsresult rv = NS_OK;
  Accessible* accessible =
    SearchForward(anchor, aRule, (aArgc > 1) ? aIncludeStart : false, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (accessible)
    *aResult = MovePivotInternal(accessible, nsIAccessiblePivot::REASON_NEXT);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MovePrevious(nsIAccessibleTraversalRule* aRule,
                                nsIAccessible* aAnchor,
                                bool aIncludeStart,
                                uint8_t aArgc, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  *aResult = false;

  Accessible* root = GetActiveRoot();
  nsRefPtr<Accessible> anchor =
    (aArgc > 0) ? do_QueryObject(aAnchor) : mPosition;
  if (anchor && (anchor->IsDefunct() || !IsDescendantOf(anchor, root)))
    return NS_ERROR_NOT_IN_TREE;

  nsresult rv = NS_OK;
  Accessible* accessible =
    SearchBackward(anchor, aRule, (aArgc > 1) ? aIncludeStart : false, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (accessible)
    *aResult = MovePivotInternal(accessible, nsIAccessiblePivot::REASON_PREV);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveFirst(nsIAccessibleTraversalRule* aRule, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  Accessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  nsresult rv = NS_OK;
  Accessible* accessible = SearchForward(root, aRule, true, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (accessible)
    *aResult = MovePivotInternal(accessible, nsIAccessiblePivot::REASON_FIRST);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveLast(nsIAccessibleTraversalRule* aRule,
                            bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  Accessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  *aResult = false;
  nsresult rv = NS_OK;
  Accessible* lastAccessible = root;
  Accessible* accessible = nullptr;

  // First go to the last accessible in pre-order
  while (lastAccessible->HasChildren())
    lastAccessible = lastAccessible->LastChild();

  // Search backwards from last accessible and find the last occurrence in the doc
  accessible = SearchBackward(lastAccessible, aRule, true, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (accessible)
    *aResult = MovePivotInternal(accessible, nsAccessiblePivot::REASON_LAST);

  return NS_OK;
}

// TODO: Implement
NS_IMETHODIMP
nsAccessiblePivot::MoveNextByText(TextBoundaryType aBoundary, bool* aResult)
{
  NS_ENSURE_ARG(aResult);

  *aResult = false;

  return NS_ERROR_NOT_IMPLEMENTED;
}

// TODO: Implement
NS_IMETHODIMP
nsAccessiblePivot::MovePreviousByText(TextBoundaryType aBoundary, bool* aResult)
{
  NS_ENSURE_ARG(aResult);

  *aResult = false;

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveToPoint(nsIAccessibleTraversalRule* aRule,
                               int32_t aX, int32_t aY, bool aIgnoreNoMatch,
                               bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aRule);

  *aResult = false;

  Accessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  RuleCache cache(aRule);
  Accessible* match = nullptr;
  Accessible* child = root->ChildAtPoint(aX, aY, Accessible::eDeepestChild);
  while (child && root != child) {
    uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;
    nsresult rv = cache.ApplyFilter(child, &filtered);
    NS_ENSURE_SUCCESS(rv, rv);

    // Ignore any matching nodes that were below this one
    if (filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE)
      match = nullptr;

    // Match if no node below this is a match
    if ((filtered & nsIAccessibleTraversalRule::FILTER_MATCH) && !match) {
      int32_t childX, childY, childWidth, childHeight;
      child->GetBounds(&childX, &childY, &childWidth, &childHeight);
      // Double-check child's bounds since the deepest child may have been out
      // of bounds. This assures we don't return a false positive.
      if (aX >= childX && aX < childX + childWidth &&
          aY >= childY && aY < childY + childHeight)
        match = child;
    }

    child = child->Parent();
  }

  if (match || !aIgnoreNoMatch)
    *aResult = MovePivotInternal(match, nsIAccessiblePivot::REASON_POINT);

  return NS_OK;
}

// Observer functions

NS_IMETHODIMP
nsAccessiblePivot::AddObserver(nsIAccessiblePivotObserver* aObserver)
{
  NS_ENSURE_ARG(aObserver);

  mObservers.AppendElement(aObserver);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::RemoveObserver(nsIAccessiblePivotObserver* aObserver)
{
  NS_ENSURE_ARG(aObserver);

  return mObservers.RemoveElement(aObserver) ? NS_OK : NS_ERROR_FAILURE;
}

// Private utility methods

bool
nsAccessiblePivot::IsDescendantOf(Accessible* aAccessible, Accessible* aAncestor)
{
  if (!aAncestor || aAncestor->IsDefunct())
    return false;

  // XXX Optimize with IsInDocument() when appropriate. Blocked by bug 759875.
  Accessible* accessible = aAccessible;
  do {
    if (accessible == aAncestor)
      return true;
  } while ((accessible = accessible->Parent()));

  return false;
}

bool
nsAccessiblePivot::MovePivotInternal(Accessible* aPosition,
                                     PivotMoveReason aReason)
{
  nsRefPtr<Accessible> oldPosition = mPosition.forget();
  mPosition = aPosition;
  int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = mEndOffset = -1;

  return NotifyOfPivotChange(oldPosition, oldStart, oldEnd, aReason);
}

Accessible*
nsAccessiblePivot::SearchBackward(Accessible* aAccessible,
                                  nsIAccessibleTraversalRule* aRule,
                                  bool aSearchCurrent,
                                  nsresult* aResult)
{
  *aResult = NS_OK;

  // Initial position could be unset, in that case return null.
  if (!aAccessible)
    return nullptr;

  RuleCache cache(aRule);
  Accessible* accessible = aAccessible;

  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (aSearchCurrent) {
    *aResult = cache.ApplyFilter(accessible, &filtered);
    NS_ENSURE_SUCCESS(*aResult, nullptr);
    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
      return accessible;
  }

  Accessible* root = GetActiveRoot();
  while (accessible != root) {
    Accessible* parent = accessible->Parent();
    int32_t idxInParent = accessible->IndexInParent();
    while (idxInParent > 0) {
      if (!(accessible = parent->GetChildAt(--idxInParent)))
        continue;

      *aResult = cache.ApplyFilter(accessible, &filtered);
      NS_ENSURE_SUCCESS(*aResult, nullptr);

      Accessible* lastChild = nullptr;
      while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
             (lastChild = accessible->LastChild())) {
        parent = accessible;
        accessible = lastChild;
        idxInParent = accessible->IndexInParent();
        *aResult = cache.ApplyFilter(accessible, &filtered);
        NS_ENSURE_SUCCESS(*aResult, nullptr);
      }

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
        return accessible;
    }

    if (!(accessible = parent))
      break;

    *aResult = cache.ApplyFilter(accessible, &filtered);
    NS_ENSURE_SUCCESS(*aResult, nullptr);

    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
      return accessible;
  }

  return nullptr;
}

Accessible*
nsAccessiblePivot::SearchForward(Accessible* aAccessible,
                                 nsIAccessibleTraversalRule* aRule,
                                 bool aSearchCurrent,
                                 nsresult* aResult)
{
  *aResult = NS_OK;

  // Initial position could be not set, in that case begin search from root.
  Accessible* root = GetActiveRoot();
  Accessible* accessible = (!aAccessible) ? root : aAccessible;

  RuleCache cache(aRule);

  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;
  *aResult = cache.ApplyFilter(accessible, &filtered);
  NS_ENSURE_SUCCESS(*aResult, nullptr);
  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH))
    return accessible;

  while (true) {
    Accessible* firstChild = nullptr;
    while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
           (firstChild = accessible->FirstChild())) {
      accessible = firstChild;
      *aResult = cache.ApplyFilter(accessible, &filtered);
      NS_ENSURE_SUCCESS(*aResult, nullptr);

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
        return accessible;
    }

    Accessible* sibling = nullptr;
    Accessible* temp = accessible;
    do {
      if (temp == root)
        break;

      sibling = temp->NextSibling();

      if (sibling)
        break;
    } while ((temp = temp->Parent()));

    if (!sibling)
      break;

    accessible = sibling;
    *aResult = cache.ApplyFilter(accessible, &filtered);
    NS_ENSURE_SUCCESS(*aResult, nullptr);

    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)
      return accessible;
  }

  return nullptr;
}

bool
nsAccessiblePivot::NotifyOfPivotChange(Accessible* aOldPosition,
                                       int32_t aOldStart, int32_t aOldEnd,
                                       int16_t aReason)
{
  if (aOldPosition == mPosition &&
      aOldStart == mStartOffset && aOldEnd == mEndOffset)
    return false;

  nsTObserverArray<nsCOMPtr<nsIAccessiblePivotObserver> >::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    nsIAccessiblePivotObserver* obs = iter.GetNext();
    obs->OnPivotChanged(this, aOldPosition, aOldStart, aOldEnd, aReason);
  }

  return true;
}

nsresult
RuleCache::ApplyFilter(Accessible* aAccessible, uint16_t* aResult)
{
  *aResult = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (!mAcceptRoles) {
    nsresult rv = mRule->GetMatchRoles(&mAcceptRoles, &mAcceptRolesLength);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mRule->GetPreFilter(&mPreFilter);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mPreFilter) {
    uint64_t state = aAccessible->State();

    if ((nsIAccessibleTraversalRule::PREFILTER_INVISIBLE & mPreFilter) &&
        (state & states::INVISIBLE))
      return NS_OK;

    if ((nsIAccessibleTraversalRule::PREFILTER_OFFSCREEN & mPreFilter) &&
        (state & states::OFFSCREEN))
      return NS_OK;

    if ((nsIAccessibleTraversalRule::PREFILTER_NOT_FOCUSABLE & mPreFilter) &&
        !(state & states::FOCUSABLE))
      return NS_OK;

    if (nsIAccessibleTraversalRule::PREFILTER_ARIA_HIDDEN & mPreFilter) {
      nsIContent* content = aAccessible->GetContent();
      if (content &&
          nsAccUtils::HasDefinedARIAToken(content, nsGkAtoms::aria_hidden) &&
          !content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_hidden,
                                nsGkAtoms::_false, eCaseMatters)) {
        *aResult |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
        return NS_OK;
      }
    }
  }

  if (mAcceptRolesLength > 0) {
    uint32_t accessibleRole = aAccessible->Role();
    bool matchesRole = false;
    for (uint32_t idx = 0; idx < mAcceptRolesLength; idx++) {
      matchesRole = mAcceptRoles[idx] == accessibleRole;
      if (matchesRole)
        break;
    }
    if (!matchesRole)
      return NS_OK;
  }

  return mRule->Match(aAccessible, aResult);
}
