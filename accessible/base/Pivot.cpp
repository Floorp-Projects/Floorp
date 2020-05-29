/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Pivot.h"

#include "AccIterator.h"
#include "Accessible.h"
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

Pivot::Pivot(Accessible* aRoot) : mRoot(aRoot) { MOZ_COUNT_CTOR(Pivot); }

Pivot::~Pivot() { MOZ_COUNT_DTOR(Pivot); }

Accessible* Pivot::AdjustStartPosition(Accessible* aAnchor, PivotRule& aRule,
                                       uint16_t* aFilterResult) {
  Accessible* matched = aAnchor;
  *aFilterResult = aRule.Match(aAnchor);

  if (aAnchor != mRoot) {
    for (Accessible* temp = aAnchor->Parent(); temp && temp != mRoot;
         temp = temp->Parent()) {
      uint16_t filtered = aRule.Match(temp);
      if (filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) {
        *aFilterResult = filtered;
        matched = temp;
      }
    }
  }

  return matched;
}

Accessible* Pivot::SearchBackward(Accessible* aAnchor, PivotRule& aRule,
                                  bool aSearchCurrent) {
  // Initial position could be unset, in that case return null.
  if (!aAnchor) {
    return nullptr;
  }

  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;

  Accessible* accessible = AdjustStartPosition(aAnchor, aRule, &filtered);

  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    return accessible;
  }

  while (accessible != mRoot) {
    Accessible* parent = accessible->Parent();
    int32_t idxInParent = accessible->IndexInParent();
    while (idxInParent > 0) {
      if (!(accessible = parent->GetChildAt(--idxInParent))) {
        continue;
      }

      filtered = aRule.Match(accessible);

      Accessible* lastChild = nullptr;
      while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
             (lastChild = accessible->LastChild())) {
        parent = accessible;
        accessible = lastChild;
        idxInParent = accessible->IndexInParent();
        filtered = aRule.Match(accessible);
      }

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
        return accessible;
      }
    }

    if (!(accessible = parent)) {
      break;
    }

    filtered = aRule.Match(accessible);

    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
      return accessible;
    }
  }

  return nullptr;
}

Accessible* Pivot::SearchForward(Accessible* aAnchor, PivotRule& aRule,
                                 bool aSearchCurrent) {
  // Initial position could be not set, in that case begin search from root.
  Accessible* accessible = aAnchor ? aAnchor : mRoot;

  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;
  accessible = AdjustStartPosition(accessible, aRule, &filtered);
  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    return accessible;
  }

  while (true) {
    Accessible* firstChild = nullptr;
    while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
           (firstChild = accessible->FirstChild())) {
      accessible = firstChild;
      filtered = aRule.Match(accessible);

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
        return accessible;
      }
    }

    Accessible* sibling = nullptr;
    Accessible* temp = accessible;
    do {
      if (temp == mRoot) {
        break;
      }

      sibling = temp->NextSibling();

      if (sibling) {
        break;
      }
    } while ((temp = temp->Parent()));

    if (!sibling) {
      break;
    }

    accessible = sibling;
    filtered = aRule.Match(accessible);
    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
      return accessible;
    }
  }

  return nullptr;
}

HyperTextAccessible* Pivot::SearchForText(Accessible* aAnchor, bool aBackward) {
  Accessible* accessible = aAnchor;
  while (true) {
    Accessible* child = nullptr;

    while ((child = (aBackward ? accessible->LastChild()
                               : accessible->FirstChild()))) {
      accessible = child;
      if (child->IsHyperText()) {
        return child->AsHyperText();
      }
    }

    Accessible* sibling = nullptr;
    Accessible* temp = accessible;
    do {
      if (temp == mRoot) {
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

      sibling = aBackward ? temp->PrevSibling() : temp->NextSibling();
    } while ((temp = temp->Parent()));

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

Accessible* Pivot::Next(Accessible* aAnchor, PivotRule& aRule,
                        bool aIncludeStart) {
  return SearchForward(aAnchor, aRule, aIncludeStart);
}

Accessible* Pivot::Prev(Accessible* aAnchor, PivotRule& aRule,
                        bool aIncludeStart) {
  return SearchBackward(aAnchor, aRule, aIncludeStart);
}

Accessible* Pivot::First(PivotRule& aRule) {
  return SearchForward(mRoot, aRule, true);
}

Accessible* Pivot::Last(PivotRule& aRule) {
  Accessible* lastAccessible = mRoot;

  // First go to the last accessible in pre-order
  while (lastAccessible->HasChildren()) {
    lastAccessible = lastAccessible->LastChild();
  }

  // Search backwards from last accessible and find the last occurrence in the
  // doc
  return SearchBackward(lastAccessible, aRule, true);
}

Accessible* Pivot::NextText(Accessible* aAnchor, int32_t* aStartOffset,
                            int32_t* aEndOffset, int32_t aBoundaryType) {
  int32_t tempStart = *aStartOffset, tempEnd = *aEndOffset;
  Accessible* tempPosition = aAnchor;

  // if we're starting on a text leaf, translate the offsets to the
  // HyperTextAccessible parent and start from there.
  if (aAnchor->IsTextLeaf() && aAnchor->Parent() &&
      aAnchor->Parent()->IsHyperText()) {
    HyperTextAccessible* text = aAnchor->Parent()->AsHyperText();
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
    Accessible* curPosition = tempPosition;
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
      tempEnd =
          text == curPosition->Parent() ? text->GetChildOffset(curPosition) : 0;
    }

    // If there's no more text on the current node, try to find the next text
    // node; if there isn't one, bail out.
    if (tempEnd == static_cast<int32_t>(text->CharacterCount())) {
      if (tempPosition == mRoot) {
        return nullptr;
      }

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
    Accessible* childAtOffset = nullptr;
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

Accessible* Pivot::PrevText(Accessible* aAnchor, int32_t* aStartOffset,
                            int32_t* aEndOffset, int32_t aBoundaryType) {
  int32_t tempStart = *aStartOffset, tempEnd = *aEndOffset;
  Accessible* tempPosition = aAnchor;

  // if we're starting on a text leaf, translate the offsets to the
  // HyperTextAccessible parent and start from there.
  if (aAnchor->IsTextLeaf() && aAnchor->Parent() &&
      aAnchor->Parent()->IsHyperText()) {
    HyperTextAccessible* text = aAnchor->Parent()->AsHyperText();
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

    Accessible* curPosition = tempPosition;
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
      if (tempPosition != curPosition && text == curPosition->Parent()) {
        tempStart = text->GetChildOffset(curPosition) +
                    nsAccUtils::TextLength(curPosition);
      } else {
        tempStart = text->CharacterCount();
      }
    }

    // If there's no more text on the current node, try to find the previous
    // text node; if there isn't one, bail out.
    if (tempStart == 0) {
      if (tempPosition == mRoot) {
        return nullptr;
      }

      // If we're currently sitting on a link, try move to either the previous
      // sibling or the parent, whichever is closer to the current end
      // offset. Otherwise, do a forward search for the next node to land on
      // (we don't do this in the first case because we don't want to go to the
      // subtree).
      Accessible* sibling = tempPosition->PrevSibling();
      if (tempPosition->IsLink()) {
        if (sibling && sibling->IsLink()) {
          HyperTextAccessible* siblingText = sibling->AsHyperText();
          tempStart = tempEnd =
              siblingText ? siblingText->CharacterCount() : -1;
          tempPosition = sibling;
        } else {
          tempStart = tempPosition->StartOffset();
          tempEnd = tempPosition->EndOffset();
          tempPosition = tempPosition->Parent();
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

    *aStartOffset = tempStart;
    *aEndOffset = tempEnd;

    MOZ_ASSERT(tempPosition);
    return tempPosition;
  }
}

Accessible* Pivot::AtPoint(int32_t aX, int32_t aY, PivotRule& aRule) {
  Accessible* match = nullptr;
  Accessible* child = mRoot->ChildAtPoint(aX, aY, Accessible::eDeepestChild);
  while (child && mRoot != child) {
    uint16_t filtered = aRule.Match(child);

    // Ignore any matching nodes that were below this one
    if (filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) {
      match = nullptr;
    }

    // Match if no node below this is a match
    if ((filtered & nsIAccessibleTraversalRule::FILTER_MATCH) && !match) {
      nsIntRect childRect = child->Bounds();
      // Double-check child's bounds since the deepest child may have been out
      // of bounds. This assures we don't return a false positive.
      if (childRect.Contains(aX, aY)) {
        match = child;
      }
    }

    child = child->Parent();
  }

  return match;
}
