/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Pivot.h"

#include "AccIterator.h"
#include "LocalAccessible.h"
#include "DocAccessible.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"

#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Pivot
////////////////////////////////////////////////////////////////////////////////

Pivot::Pivot(const AccessibleOrProxy& aRoot) : mRoot(aRoot) {
  MOZ_COUNT_CTOR(Pivot);
}

Pivot::~Pivot() { MOZ_COUNT_DTOR(Pivot); }

AccessibleOrProxy Pivot::AdjustStartPosition(AccessibleOrProxy& aAnchor,
                                             PivotRule& aRule,
                                             uint16_t* aFilterResult) {
  AccessibleOrProxy matched = aAnchor;
  *aFilterResult = aRule.Match(aAnchor);

  if (aAnchor != mRoot) {
    for (AccessibleOrProxy temp = aAnchor.Parent();
         !temp.IsNull() && temp != mRoot; temp = temp.Parent()) {
      uint16_t filtered = aRule.Match(temp);
      if (filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) {
        *aFilterResult = filtered;
        matched = temp;
      }
    }
  }

  return matched;
}

AccessibleOrProxy Pivot::SearchBackward(AccessibleOrProxy& aAnchor,
                                        PivotRule& aRule, bool aSearchCurrent) {
  // Initial position could be unset, in that case return null AoP.
  if (aAnchor.IsNull()) {
    return aAnchor;
  }

  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;

  AccessibleOrProxy accOrProxy = AdjustStartPosition(aAnchor, aRule, &filtered);

  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    return accOrProxy;
  }

  while (accOrProxy != mRoot) {
    AccessibleOrProxy parent = accOrProxy.Parent();
    int32_t idxInParent = accOrProxy.IndexInParent();
    while (idxInParent > 0) {
      accOrProxy = parent.ChildAt(--idxInParent);
      if (accOrProxy.IsNull()) {
        continue;
      }

      filtered = aRule.Match(accOrProxy);

      AccessibleOrProxy lastChild = accOrProxy.LastChild();
      while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
             !lastChild.IsNull()) {
        parent = accOrProxy;
        accOrProxy = lastChild;
        idxInParent = accOrProxy.IndexInParent();
        filtered = aRule.Match(accOrProxy);
        lastChild = accOrProxy.LastChild();
      }

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
        return accOrProxy;
      }
    }

    accOrProxy = parent;
    if (accOrProxy.IsNull()) {
      break;
    }

    filtered = aRule.Match(accOrProxy);

    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
      return accOrProxy;
    }
  }

  return AccessibleOrProxy();
}

AccessibleOrProxy Pivot::SearchForward(AccessibleOrProxy& aAnchor,
                                       PivotRule& aRule, bool aSearchCurrent) {
  // Initial position could be not set, in that case begin search from root.
  AccessibleOrProxy accOrProxy = !aAnchor.IsNull() ? aAnchor : mRoot;

  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;
  accOrProxy = AdjustStartPosition(accOrProxy, aRule, &filtered);
  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    return accOrProxy;
  }

  while (true) {
    AccessibleOrProxy firstChild = accOrProxy.FirstChild();
    while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
           !firstChild.IsNull()) {
      accOrProxy = firstChild;
      filtered = aRule.Match(accOrProxy);

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
        return accOrProxy;
      }
      firstChild = accOrProxy.FirstChild();
    }

    AccessibleOrProxy sibling = AccessibleOrProxy();
    AccessibleOrProxy temp = accOrProxy;
    do {
      if (temp == mRoot) {
        break;
      }

      sibling = temp.NextSibling();

      if (!sibling.IsNull()) {
        break;
      }
      temp = temp.Parent();
    } while (!temp.IsNull());

    if (sibling.IsNull()) {
      break;
    }

    accOrProxy = sibling;
    filtered = aRule.Match(accOrProxy);
    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
      return accOrProxy;
    }
  }

  return AccessibleOrProxy();
}

// TODO: This method does not work for proxy accessibles
HyperTextAccessible* Pivot::SearchForText(LocalAccessible* aAnchor,
                                          bool aBackward) {
  if (!mRoot.IsAccessible()) {
    return nullptr;
  }
  LocalAccessible* accessible = aAnchor;
  while (true) {
    LocalAccessible* child = nullptr;

    while ((child = (aBackward ? accessible->LocalLastChild()
                               : accessible->LocalFirstChild()))) {
      accessible = child;
      if (child->IsHyperText()) {
        return child->AsHyperText();
      }
    }

    LocalAccessible* sibling = nullptr;
    LocalAccessible* temp = accessible;
    do {
      if (temp == mRoot.AsAccessible()) {
        break;
      }

      // Unlike traditional pre-order traversal we revisit the parent
      // nodes when we go up the tree. This is because our starting point
      // may be a subtree or a leaf. If it's parent matches, it should
      // take precedent over a sibling.
      if (temp != aAnchor && temp->IsHyperText()) {
        return temp->AsHyperText();
      }

      if (sibling) {
        break;
      }

      sibling = aBackward ? temp->LocalPrevSibling() : temp->LocalNextSibling();
    } while ((temp = temp->LocalParent()));

    if (!sibling) {
      break;
    }

    accessible = sibling;
    if (accessible->IsHyperText()) {
      return accessible->AsHyperText();
    }
  }

  return nullptr;
}

AccessibleOrProxy Pivot::Next(AccessibleOrProxy& aAnchor, PivotRule& aRule,
                              bool aIncludeStart) {
  return SearchForward(aAnchor, aRule, aIncludeStart);
}

AccessibleOrProxy Pivot::Prev(AccessibleOrProxy& aAnchor, PivotRule& aRule,
                              bool aIncludeStart) {
  return SearchBackward(aAnchor, aRule, aIncludeStart);
}

AccessibleOrProxy Pivot::First(PivotRule& aRule) {
  return SearchForward(mRoot, aRule, true);
}

AccessibleOrProxy Pivot::Last(PivotRule& aRule) {
  AccessibleOrProxy lastAccOrProxy = mRoot;

  // First go to the last accessible in pre-order
  while (lastAccOrProxy.HasChildren()) {
    lastAccOrProxy = lastAccOrProxy.LastChild();
  }

  // Search backwards from last accessible and find the last occurrence in the
  // doc
  return SearchBackward(lastAccOrProxy, aRule, true);
}

// TODO: This method does not work for proxy accessibles
LocalAccessible* Pivot::NextText(LocalAccessible* aAnchor,
                                 int32_t* aStartOffset, int32_t* aEndOffset,
                                 int32_t aBoundaryType) {
  if (!mRoot.IsAccessible()) {
    return nullptr;
  }

  int32_t tempStart = *aStartOffset, tempEnd = *aEndOffset;
  LocalAccessible* tempPosition = aAnchor;

  // if we're starting on a text leaf, translate the offsets to the
  // HyperTextAccessible parent and start from there.
  if (aAnchor->IsTextLeaf() && aAnchor->LocalParent() &&
      aAnchor->LocalParent()->IsHyperText()) {
    HyperTextAccessible* text = aAnchor->LocalParent()->AsHyperText();
    tempPosition = text;
    int32_t childOffset = text->GetChildOffset(aAnchor);
    if (tempEnd == -1) {
      tempStart = 0;
      tempEnd = 0;
    }
    tempStart += childOffset;
    tempEnd += childOffset;
  }

  while (true) {
    MOZ_ASSERT(tempPosition);
    LocalAccessible* curPosition = tempPosition;
    HyperTextAccessible* text = nullptr;
    // Find the nearest text node using a preorder traversal starting from
    // the current node.
    if (!(text = tempPosition->AsHyperText())) {
      text = SearchForText(tempPosition, false);
      if (!text) {
        return nullptr;
      }

      if (text != curPosition) {
        tempStart = tempEnd = -1;
      }
      tempPosition = text;
    }

    // If the search led to the parent of the node we started on (e.g. when
    // starting on a text leaf), start the text movement from the end of that
    // node, otherwise we just default to 0.
    if (tempEnd == -1) {
      tempEnd = text == curPosition->LocalParent()
                    ? text->GetChildOffset(curPosition)
                    : 0;
    }

    // If there's no more text on the current node, try to find the next text
    // node; if there isn't one, bail out.
    if (tempEnd == static_cast<int32_t>(text->CharacterCount())) {
      if (tempPosition == mRoot.AsAccessible()) {
        return nullptr;
      }

      // If we're currently sitting on a link, try move to either the next
      // sibling or the parent, whichever is closer to the current end
      // offset. Otherwise, do a forward search for the next node to land on
      // (we don't do this in the first case because we don't want to go to the
      // subtree).
      LocalAccessible* sibling = tempPosition->LocalNextSibling();
      if (tempPosition->IsLink()) {
        if (sibling && sibling->IsLink()) {
          tempStart = tempEnd = -1;
          tempPosition = sibling;
        } else {
          tempStart = tempPosition->StartOffset();
          tempEnd = tempPosition->EndOffset();
          tempPosition = tempPosition->LocalParent();
        }
      } else {
        tempPosition = SearchForText(tempPosition, false);
        if (!tempPosition) {
          return nullptr;
        }

        tempStart = tempEnd = -1;
      }
      continue;
    }

    AccessibleTextBoundary startBoundary, endBoundary;
    switch (aBoundaryType) {
      case nsIAccessiblePivot::CHAR_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_CHAR;
        endBoundary = nsIAccessibleText::BOUNDARY_CHAR;
        break;
      case nsIAccessiblePivot::WORD_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_WORD_START;
        endBoundary = nsIAccessibleText::BOUNDARY_WORD_END;
        break;
      case nsIAccessiblePivot::LINE_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_LINE_START;
        endBoundary = nsIAccessibleText::BOUNDARY_LINE_END;
        break;
      default:
        return nullptr;
    }

    nsAutoString unusedText;
    int32_t newStart = 0, newEnd = 0, currentEnd = tempEnd;
    text->TextAtOffset(tempEnd, endBoundary, &newStart, &tempEnd, unusedText);
    text->TextBeforeOffset(tempEnd, startBoundary, &newStart, &newEnd,
                           unusedText);
    int32_t potentialStart = newEnd == tempEnd ? newStart : newEnd;
    tempStart = potentialStart > tempStart ? potentialStart : currentEnd;

    // The offset range we've obtained might have embedded characters in it,
    // limit the range to the start of the first occurrence of an embedded
    // character.
    LocalAccessible* childAtOffset = nullptr;
    for (int32_t i = tempStart; i < tempEnd; i++) {
      childAtOffset = text->GetChildAtOffset(i);
      if (childAtOffset && childAtOffset->IsHyperText()) {
        tempEnd = i;
        break;
      }
    }
    // If there's an embedded character at the very start of the range, we
    // instead want to traverse into it. So restart the movement with
    // the child as the starting point.
    if (childAtOffset && childAtOffset->IsHyperText() &&
        tempStart == static_cast<int32_t>(childAtOffset->StartOffset())) {
      tempPosition = childAtOffset;
      tempStart = tempEnd = -1;
      continue;
    }

    *aStartOffset = tempStart;
    *aEndOffset = tempEnd;

    MOZ_ASSERT(tempPosition);
    return tempPosition;
  }
}

// TODO: This method does not work for proxy accessibles
LocalAccessible* Pivot::PrevText(LocalAccessible* aAnchor,
                                 int32_t* aStartOffset, int32_t* aEndOffset,
                                 int32_t aBoundaryType) {
  if (!mRoot.IsAccessible()) {
    return nullptr;
  }

  int32_t tempStart = *aStartOffset, tempEnd = *aEndOffset;
  LocalAccessible* tempPosition = aAnchor;

  // if we're starting on a text leaf, translate the offsets to the
  // HyperTextAccessible parent and start from there.
  if (aAnchor->IsTextLeaf() && aAnchor->LocalParent() &&
      aAnchor->LocalParent()->IsHyperText()) {
    HyperTextAccessible* text = aAnchor->LocalParent()->AsHyperText();
    tempPosition = text;
    int32_t childOffset = text->GetChildOffset(aAnchor);
    if (tempStart == -1) {
      tempStart = nsAccUtils::TextLength(aAnchor);
      tempEnd = tempStart;
    }
    tempStart += childOffset;
    tempEnd += childOffset;
  }

  while (true) {
    MOZ_ASSERT(tempPosition);

    LocalAccessible* curPosition = tempPosition;
    HyperTextAccessible* text;
    // Find the nearest text node using a reverse preorder traversal starting
    // from the current node.
    if (!(text = tempPosition->AsHyperText())) {
      text = SearchForText(tempPosition, true);
      if (!text) {
        return nullptr;
      }

      if (text != curPosition) {
        tempStart = tempEnd = -1;
      }
      tempPosition = text;
    }

    // If the search led to the parent of the node we started on (e.g. when
    // starting on a text leaf), start the text movement from the end offset
    // of that node. Otherwise we just default to the last offset in the parent.
    if (tempStart == -1) {
      if (tempPosition != curPosition && text == curPosition->LocalParent()) {
        tempStart = text->GetChildOffset(curPosition) +
                    nsAccUtils::TextLength(curPosition);
      } else {
        tempStart = text->CharacterCount();
      }
    }

    // If there's no more text on the current node, try to find the previous
    // text node; if there isn't one, bail out.
    if (tempStart == 0) {
      if (tempPosition == mRoot.AsAccessible()) {
        return nullptr;
      }

      // If we're currently sitting on a link, try move to either the previous
      // sibling or the parent, whichever is closer to the current end
      // offset. Otherwise, do a forward search for the next node to land on
      // (we don't do this in the first case because we don't want to go to the
      // subtree).
      LocalAccessible* sibling = tempPosition->LocalPrevSibling();
      if (tempPosition->IsLink()) {
        if (sibling && sibling->IsLink()) {
          HyperTextAccessible* siblingText = sibling->AsHyperText();
          tempStart = tempEnd =
              siblingText ? siblingText->CharacterCount() : -1;
          tempPosition = sibling;
        } else {
          tempStart = tempPosition->StartOffset();
          tempEnd = tempPosition->EndOffset();
          tempPosition = tempPosition->LocalParent();
        }
      } else {
        HyperTextAccessible* tempText = SearchForText(tempPosition, true);
        if (!tempText) {
          return nullptr;
        }

        tempPosition = tempText;
        tempStart = tempEnd = tempText->CharacterCount();
      }
      continue;
    }

    AccessibleTextBoundary startBoundary, endBoundary;
    switch (aBoundaryType) {
      case nsIAccessiblePivot::CHAR_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_CHAR;
        endBoundary = nsIAccessibleText::BOUNDARY_CHAR;
        break;
      case nsIAccessiblePivot::WORD_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_WORD_START;
        endBoundary = nsIAccessibleText::BOUNDARY_WORD_END;
        break;
      case nsIAccessiblePivot::LINE_BOUNDARY:
        startBoundary = nsIAccessibleText::BOUNDARY_LINE_START;
        endBoundary = nsIAccessibleText::BOUNDARY_LINE_END;
        break;
      default:
        return nullptr;
    }

    nsAutoString unusedText;
    int32_t newStart = 0, newEnd = 0, currentStart = tempStart,
            potentialEnd = 0;
    text->TextBeforeOffset(tempStart, startBoundary, &newStart, &newEnd,
                           unusedText);
    if (newStart < tempStart) {
      tempStart = newEnd >= currentStart ? newStart : newEnd;
    } else {
      // XXX: In certain odd cases newStart is equal to tempStart
      text->TextBeforeOffset(tempStart - 1, startBoundary, &newStart,
                             &tempStart, unusedText);
    }
    text->TextAtOffset(tempStart, endBoundary, &newStart, &potentialEnd,
                       unusedText);
    tempEnd = potentialEnd < tempEnd ? potentialEnd : currentStart;

    // The offset range we've obtained might have embedded characters in it,
    // limit the range to the start of the last occurrence of an embedded
    // character.
    LocalAccessible* childAtOffset = nullptr;
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

    *aStartOffset = tempStart;
    *aEndOffset = tempEnd;

    MOZ_ASSERT(tempPosition);
    return tempPosition;
  }
}

AccessibleOrProxy Pivot::AtPoint(int32_t aX, int32_t aY, PivotRule& aRule) {
  AccessibleOrProxy match = AccessibleOrProxy();
  AccessibleOrProxy child =
      mRoot.ChildAtPoint(aX, aY, Accessible::EWhichChildAtPoint::DeepestChild);
  while (!child.IsNull() && (mRoot != child)) {
    uint16_t filtered = aRule.Match(child);

    // Ignore any matching nodes that were below this one
    if (filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) {
      match = AccessibleOrProxy();
    }

    // Match if no node below this is a match
    if ((filtered & nsIAccessibleTraversalRule::FILTER_MATCH) &&
        match.IsNull()) {
      nsIntRect childRect = child.IsAccessible()
                                ? child.AsAccessible()->Bounds()
                                : child.AsProxy()->Bounds();
      // Double-check child's bounds since the deepest child may have been out
      // of bounds. This assures we don't return a false positive.
      if (childRect.Contains(aX, aY)) {
        match = child;
      }
    }

    child = child.Parent();
  }

  return match;
}

// Role Rule

PivotRoleRule::PivotRoleRule(mozilla::a11y::role aRole)
    : mRole(aRole), mDirectDescendantsFrom(nullptr) {}

PivotRoleRule::PivotRoleRule(mozilla::a11y::role aRole,
                             AccessibleOrProxy& aDirectDescendantsFrom)
    : mRole(aRole), mDirectDescendantsFrom(aDirectDescendantsFrom) {}

uint16_t PivotRoleRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAccOrProxy)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (!mDirectDescendantsFrom.IsNull() &&
      (aAccOrProxy != mDirectDescendantsFrom)) {
    // If we've specified mDirectDescendantsFrom, we should ignore
    // non-direct descendants of from the specified AoP. Because
    // pivot performs a preorder traversal, the first aAccOrProxy
    // object(s) that don't equal mDirectDescendantsFrom will be
    // mDirectDescendantsFrom's children. We'll process them, but ignore
    // their subtrees thereby processing direct descendants of
    // mDirectDescendantsFrom only.
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (aAccOrProxy.Role() == mRole) {
    result |= nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  return result;
}

// LocalAccInSameDocRule

uint16_t LocalAccInSameDocRule::Match(const AccessibleOrProxy& aAccOrProxy) {
  LocalAccessible* acc = aAccOrProxy.AsAccessible();
  if (!acc) {
    return nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }
  if (acc->IsOuterDoc()) {
    return nsIAccessibleTraversalRule::FILTER_MATCH |
           nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }
  return nsIAccessibleTraversalRule::FILTER_MATCH;
}
