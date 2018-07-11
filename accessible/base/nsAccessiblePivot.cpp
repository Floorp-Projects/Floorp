/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessiblePivot.h"

#include "HyperTextAccessible.h"
#include "nsAccUtils.h"
#include "States.h"
#include "xpcAccessibleDocument.h"

using namespace mozilla::a11y;


/**
 * An object that stores a given traversal rule during the pivot movement.
 */
class RuleCache
{
public:
  explicit RuleCache(nsIAccessibleTraversalRule* aRule) :
    mRule(aRule), mAcceptRoles(nullptr),
    mAcceptRolesLength{0}, mPreFilter{0} { }
  ~RuleCache () {
    if (mAcceptRoles)
      free(mAcceptRoles);
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

nsAccessiblePivot::~nsAccessiblePivot()
{
}

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
nsAccessiblePivot::GetRoot(nsIAccessible** aRoot)
{
  NS_ENSURE_ARG_POINTER(aRoot);

  NS_IF_ADDREF(*aRoot = ToXPC(mRoot));

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetPosition(nsIAccessible** aPosition)
{
  NS_ENSURE_ARG_POINTER(aPosition);

  NS_IF_ADDREF(*aPosition = ToXPC(mPosition));

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetPosition(nsIAccessible* aPosition)
{
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
                      nsIAccessiblePivot::REASON_NONE, false);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::GetModalRoot(nsIAccessible** aModalRoot)
{
  NS_ENSURE_ARG_POINTER(aModalRoot);

  NS_IF_ADDREF(*aModalRoot = ToXPC(mModalRoot));

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::SetModalRoot(nsIAccessible* aModalRoot)
{
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
                                int32_t aStartOffset, int32_t aEndOffset,
                                bool aIsFromUserInput, uint8_t aArgc)
{
  NS_ENSURE_ARG(aTextAccessible);

  // Check that start offset is smaller than end offset, and that if a value is
  // smaller than 0, both should be -1.
  NS_ENSURE_TRUE(aStartOffset <= aEndOffset &&
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
  NotifyOfPivotChange(acc, oldStart, oldEnd,
                      nsIAccessiblePivot::REASON_TEXT,
                      (aArgc > 0) ? aIsFromUserInput : true);

  return NS_OK;
}

// Traversal functions

NS_IMETHODIMP
nsAccessiblePivot::MoveNext(nsIAccessibleTraversalRule* aRule,
                            nsIAccessible* aAnchor, bool aIncludeStart,
                            bool aIsFromUserInput, uint8_t aArgc, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);
  *aResult = false;

  Accessible* anchor = mPosition;
  if (aArgc > 0 && aAnchor)
    anchor = aAnchor->ToInternalAccessible();

  if (anchor && (anchor->IsDefunct() || !IsDescendantOf(anchor, GetActiveRoot())))
    return NS_ERROR_NOT_IN_TREE;

  nsresult rv = NS_OK;
  Accessible* accessible =
    SearchForward(anchor, aRule, (aArgc > 1) ? aIncludeStart : false, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (accessible)
    *aResult = MovePivotInternal(accessible, nsIAccessiblePivot::REASON_NEXT,
                                 (aArgc > 2) ? aIsFromUserInput : true);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MovePrevious(nsIAccessibleTraversalRule* aRule,
                                nsIAccessible* aAnchor,
                                bool aIncludeStart, bool aIsFromUserInput,
                                uint8_t aArgc, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);
  *aResult = false;

  Accessible* anchor = mPosition;
  if (aArgc > 0 && aAnchor)
    anchor = aAnchor->ToInternalAccessible();

  if (anchor && (anchor->IsDefunct() || !IsDescendantOf(anchor, GetActiveRoot())))
    return NS_ERROR_NOT_IN_TREE;

  nsresult rv = NS_OK;
  Accessible* accessible =
    SearchBackward(anchor, aRule, (aArgc > 1) ? aIncludeStart : false, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (accessible)
    *aResult = MovePivotInternal(accessible, nsIAccessiblePivot::REASON_PREV,
                                 (aArgc > 2) ? aIsFromUserInput : true);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveFirst(nsIAccessibleTraversalRule* aRule,
                             bool aIsFromUserInput,
                             uint8_t aArgc, bool* aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aRule);

  Accessible* root = GetActiveRoot();
  NS_ENSURE_TRUE(root && !root->IsDefunct(), NS_ERROR_NOT_IN_TREE);

  nsresult rv = NS_OK;
  Accessible* accessible = SearchForward(root, aRule, true, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (accessible)
    *aResult = MovePivotInternal(accessible, nsIAccessiblePivot::REASON_FIRST,
                                 (aArgc > 0) ? aIsFromUserInput : true);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveLast(nsIAccessibleTraversalRule* aRule,
                            bool aIsFromUserInput,
                            uint8_t aArgc, bool* aResult)
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
    *aResult = MovePivotInternal(accessible, nsAccessiblePivot::REASON_LAST,
                                 (aArgc > 0) ? aIsFromUserInput : true);

  return NS_OK;
}

NS_IMETHODIMP
nsAccessiblePivot::MoveNextByText(TextBoundaryType aBoundary,
                                  bool aIsFromUserInput, uint8_t aArgc,
                                  bool* aResult)
{
  NS_ENSURE_ARG(aResult);

  *aResult = false;

  int32_t tempStart = mStartOffset, tempEnd = mEndOffset;
  Accessible* tempPosition = mPosition;
  Accessible* root = GetActiveRoot();
  while (true) {
    NS_ENSURE_TRUE(tempPosition, NS_ERROR_UNEXPECTED);
    Accessible* curPosition = tempPosition;
    HyperTextAccessible* text = nullptr;
    // Find the nearest text node using a preorder traversal starting from
    // the current node.
    if (!(text = tempPosition->AsHyperText())) {
      text = SearchForText(tempPosition, false);
      if (!text)
        return NS_OK;
      if (text != curPosition)
        tempStart = tempEnd = -1;
      tempPosition = text;
    }

    // If the search led to the parent of the node we started on (e.g. when
    // starting on a text leaf), start the text movement from the end of that
    // node, otherwise we just default to 0.
    if (tempEnd == -1)
      tempEnd = text == curPosition->Parent() ?
                text->GetChildOffset(curPosition) : 0;

    // If there's no more text on the current node, try to find the next text
    // node; if there isn't one, bail out.
    if (tempEnd == static_cast<int32_t>(text->CharacterCount())) {
      if (tempPosition == root)
        return NS_OK;

      // If we're currently sitting on a link, try move to either the next
      // sibling or the parent, whichever is closer to the current end
      // offset. Otherwise, do a forward search for the next node to land on
      // (we don't do this in the first case because we don't want to go to the
      // subtree).
      Accessible* sibling = tempPosition->NextSibling();
      if (tempPosition->IsLink()) {
        if (sibling && sibling->IsLink()) {
          tempStart = tempEnd = -1;
          tempPosition = sibling;
        } else {
          tempStart = tempPosition->StartOffset();
          tempEnd = tempPosition->EndOffset();
          tempPosition = tempPosition->Parent();
        }
      } else {
        tempPosition = SearchForText(tempPosition, false);
        if (!tempPosition)
          return NS_OK;
        tempStart = tempEnd = -1;
      }
      continue;
    }

    AccessibleTextBoundary startBoundary, endBoundary;
    switch (aBoundary) {
      case CHAR_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_CHAR;
        endBoundary = nsIAccessibleText::BOUNDARY_CHAR;
        break;
      case WORD_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_WORD_START;
        endBoundary = nsIAccessibleText::BOUNDARY_WORD_END;
        break;
      default:
        return NS_ERROR_INVALID_ARG;
    }

    nsAutoString unusedText;
    int32_t newStart = 0, newEnd = 0, currentEnd = tempEnd;
    text->TextAtOffset(tempEnd, endBoundary, &newStart, &tempEnd, unusedText);
    text->TextBeforeOffset(tempEnd, startBoundary, &newStart, &newEnd, unusedText);
    int32_t potentialStart = newEnd == tempEnd ? newStart : newEnd;
    tempStart = potentialStart > tempStart ? potentialStart : currentEnd;

    // The offset range we've obtained might have embedded characters in it,
    // limit the range to the start of the first occurrence of an embedded
    // character.
    Accessible* childAtOffset = nullptr;
    for (int32_t i = tempStart; i < tempEnd; i++) {
      childAtOffset = text->GetChildAtOffset(i);
      if (childAtOffset && !childAtOffset->IsText()) {
        tempEnd = i;
        break;
      }
    }
    // If there's an embedded character at the very start of the range, we
    // instead want to traverse into it. So restart the movement with
    // the child as the starting point.
    if (childAtOffset && !childAtOffset->IsText() &&
        tempStart == static_cast<int32_t>(childAtOffset->StartOffset())) {
      tempPosition = childAtOffset;
      tempStart = tempEnd = -1;
      continue;
    }

    *aResult = true;

    Accessible* startPosition = mPosition;
    int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
    mPosition = tempPosition;
    mStartOffset = tempStart;
    mEndOffset = tempEnd;
    NotifyOfPivotChange(startPosition, oldStart, oldEnd,
                        nsIAccessiblePivot::REASON_TEXT,
                        (aArgc > 0) ? aIsFromUserInput : true);
    return NS_OK;
  }
}

NS_IMETHODIMP
nsAccessiblePivot::MovePreviousByText(TextBoundaryType aBoundary,
                                      bool aIsFromUserInput, uint8_t aArgc,
                                      bool* aResult)
{
  NS_ENSURE_ARG(aResult);

  *aResult = false;

  int32_t tempStart = mStartOffset, tempEnd = mEndOffset;
  Accessible* tempPosition = mPosition;
  Accessible* root = GetActiveRoot();
  while (true) {
    NS_ENSURE_TRUE(tempPosition, NS_ERROR_UNEXPECTED);
    Accessible* curPosition = tempPosition;
    HyperTextAccessible* text;
    // Find the nearest text node using a reverse preorder traversal starting
    // from the current node.
    if (!(text = tempPosition->AsHyperText())) {
      text = SearchForText(tempPosition, true);
      if (!text)
        return NS_OK;
      if (text != curPosition)
        tempStart = tempEnd = -1;
      tempPosition = text;
    }

    // If the search led to the parent of the node we started on (e.g. when
    // starting on a text leaf), start the text movement from the end offset
    // of that node. Otherwise we just default to the last offset in the parent.
    if (tempStart == -1) {
      if (tempPosition != curPosition && text == curPosition->Parent())
        tempStart = text->GetChildOffset(curPosition) + nsAccUtils::TextLength(curPosition);
      else
        tempStart = text->CharacterCount();
    }

    // If there's no more text on the current node, try to find the previous
    // text node; if there isn't one, bail out.
    if (tempStart == 0) {
      if (tempPosition == root)
        return NS_OK;

      // If we're currently sitting on a link, try move to either the previous
      // sibling or the parent, whichever is closer to the current end
      // offset. Otherwise, do a forward search for the next node to land on
      // (we don't do this in the first case because we don't want to go to the
      // subtree).
      Accessible* sibling = tempPosition->PrevSibling();
      if (tempPosition->IsLink()) {
        if (sibling && sibling->IsLink()) {
          HyperTextAccessible* siblingText = sibling->AsHyperText();
          tempStart = tempEnd = siblingText ?
                                siblingText->CharacterCount() : -1;
          tempPosition = sibling;
        } else {
          tempStart = tempPosition->StartOffset();
          tempEnd = tempPosition->EndOffset();
          tempPosition = tempPosition->Parent();
        }
      } else {
        HyperTextAccessible* tempText = SearchForText(tempPosition, true);
        if (!tempText)
          return NS_OK;
        tempPosition = tempText;
        tempStart = tempEnd = tempText->CharacterCount();
      }
      continue;
    }

    AccessibleTextBoundary startBoundary, endBoundary;
    switch (aBoundary) {
      case CHAR_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_CHAR;
        endBoundary = nsIAccessibleText::BOUNDARY_CHAR;
        break;
      case WORD_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_WORD_START;
        endBoundary = nsIAccessibleText::BOUNDARY_WORD_END;
        break;
      default:
        return NS_ERROR_INVALID_ARG;
    }

    nsAutoString unusedText;
    int32_t newStart = 0, newEnd = 0, currentStart = tempStart, potentialEnd = 0;
    text->TextBeforeOffset(tempStart, startBoundary, &newStart, &newEnd, unusedText);
    if (newStart < tempStart)
      tempStart = newEnd >= currentStart ? newStart : newEnd;
    else // XXX: In certain odd cases newStart is equal to tempStart
      text->TextBeforeOffset(tempStart - 1, startBoundary, &newStart,
                             &tempStart, unusedText);
    text->TextAtOffset(tempStart, endBoundary, &newStart, &potentialEnd,
                       unusedText);
    tempEnd = potentialEnd < tempEnd ? potentialEnd : currentStart;

    // The offset range we've obtained might have embedded characters in it,
    // limit the range to the start of the last occurrence of an embedded
    // character.
    Accessible* childAtOffset = nullptr;
    for (int32_t i = tempEnd - 1; i >= tempStart; i--) {
      childAtOffset = text->GetChildAtOffset(i);
      if (childAtOffset && !childAtOffset->IsText()) {
        tempStart = childAtOffset->EndOffset();
        break;
      }
    }
    // If there's an embedded character at the very end of the range, we
    // instead want to traverse into it. So restart the movement with
    // the child as the starting point.
    if (childAtOffset && !childAtOffset->IsText() &&
        tempEnd == static_cast<int32_t>(childAtOffset->EndOffset())) {
      tempPosition = childAtOffset;
      tempStart = tempEnd = childAtOffset->AsHyperText()->CharacterCount();
      continue;
    }

    *aResult = true;

    Accessible* startPosition = mPosition;
    int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
    mPosition = tempPosition;
    mStartOffset = tempStart;
    mEndOffset = tempEnd;

    NotifyOfPivotChange(startPosition, oldStart, oldEnd,
                        nsIAccessiblePivot::REASON_TEXT,
                        (aArgc > 0) ? aIsFromUserInput : true);
    return NS_OK;
  }
}

NS_IMETHODIMP
nsAccessiblePivot::MoveToPoint(nsIAccessibleTraversalRule* aRule,
                               int32_t aX, int32_t aY, bool aIgnoreNoMatch,
                               bool aIsFromUserInput, uint8_t aArgc,
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
      nsIntRect childRect = child->Bounds();
      // Double-check child's bounds since the deepest child may have been out
      // of bounds. This assures we don't return a false positive.
      if (childRect.Contains(aX, aY))
        match = child;
    }

    child = child->Parent();
  }

  if (match || !aIgnoreNoMatch)
    *aResult = MovePivotInternal(match, nsIAccessiblePivot::REASON_POINT,
                                 (aArgc > 0) ? aIsFromUserInput : true);

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
                                     PivotMoveReason aReason,
                                     bool aIsFromUserInput)
{
  RefPtr<Accessible> oldPosition = mPosition.forget();
  mPosition = aPosition;
  int32_t oldStart = mStartOffset, oldEnd = mEndOffset;
  mStartOffset = mEndOffset = -1;

  return NotifyOfPivotChange(oldPosition, oldStart, oldEnd, aReason,
                             aIsFromUserInput);
}

Accessible*
nsAccessiblePivot::AdjustStartPosition(Accessible* aAccessible,
                                       RuleCache& aCache,
                                       uint16_t* aFilterResult,
                                       nsresult* aResult)
{
  Accessible* matched = aAccessible;
  *aResult = aCache.ApplyFilter(aAccessible, aFilterResult);

  if (aAccessible != mRoot && aAccessible != mModalRoot) {
    for (Accessible* temp = aAccessible->Parent();
         temp && temp != mRoot && temp != mModalRoot; temp = temp->Parent()) {
      uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;
      *aResult = aCache.ApplyFilter(temp, &filtered);
      NS_ENSURE_SUCCESS(*aResult, nullptr);
      if (filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) {
        *aFilterResult = filtered;
        matched = temp;
      }
    }
  }

  if (aAccessible == mPosition && mStartOffset != -1 && mEndOffset != -1) {
    HyperTextAccessible* text = aAccessible->AsHyperText();
    if (text) {
      matched = text->GetChildAtOffset(mStartOffset);
    }
  }

  return matched;
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
  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;
  Accessible* accessible = AdjustStartPosition(aAccessible, cache,
                                               &filtered, aResult);
  NS_ENSURE_SUCCESS(*aResult, nullptr);

  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)) {
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
  accessible = AdjustStartPosition(accessible, cache, &filtered, aResult);
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

HyperTextAccessible*
nsAccessiblePivot::SearchForText(Accessible* aAccessible, bool aBackward)
{
  Accessible* root = GetActiveRoot();
  Accessible* accessible = aAccessible;
  while (true) {
    Accessible* child = nullptr;

    while ((child = (aBackward ? accessible->LastChild() :
                                 accessible->FirstChild()))) {
      accessible = child;
      if (child->IsHyperText())
        return child->AsHyperText();
    }

    Accessible* sibling = nullptr;
    Accessible* temp = accessible;
    do {
      if (temp == root)
        break;

      // Unlike traditional pre-order traversal we revisit the parent
      // nodes when we go up the tree. This is because our starting point
      // may be a subtree or a leaf. If it's parent matches, it should
      // take precedent over a sibling.
      if (temp != aAccessible && temp->IsHyperText())
        return temp->AsHyperText();

      if (sibling)
        break;

      sibling = aBackward ? temp->PrevSibling() : temp->NextSibling();
    } while ((temp = temp->Parent()));

    if (!sibling)
      break;

    accessible = sibling;
    if (accessible->IsHyperText())
      return accessible->AsHyperText();
  }

  return nullptr;
}


bool
nsAccessiblePivot::NotifyOfPivotChange(Accessible* aOldPosition,
                                       int32_t aOldStart, int32_t aOldEnd,
                                       int16_t aReason, bool aIsFromUserInput)
{
  if (aOldPosition == mPosition &&
      aOldStart == mStartOffset && aOldEnd == mEndOffset)
    return false;

  nsCOMPtr<nsIAccessible> xpcOldPos = ToXPC(aOldPosition); // death grip
  nsTObserverArray<nsCOMPtr<nsIAccessiblePivotObserver> >::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    nsIAccessiblePivotObserver* obs = iter.GetNext();
    obs->OnPivotChanged(this,
                        xpcOldPos, aOldStart, aOldEnd,
                        ToXPC(mPosition), mStartOffset, mEndOffset,
                        aReason, aIsFromUserInput);
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

    if ((nsIAccessibleTraversalRule::PREFILTER_TRANSPARENT & mPreFilter) &&
        !(state & states::OPAQUE1)) {
      nsIFrame* frame = aAccessible->GetFrame();
      if (frame->StyleEffects()->mOpacity == 0.0f) {
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

  return mRule->Match(ToXPC(aAccessible), aResult);
}
