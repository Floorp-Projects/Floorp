/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Pivot.h"

#include "AccIterator.h"
#include "LocalAccessible.h"
#include "RemoteAccessible.h"
#include "DocAccessible.h"
#include "nsAccUtils.h"

#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/HyperTextAccessibleBase.h"
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

  if (aAnchor && aAnchor != mRoot) {
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

  Accessible* acc = AdjustStartPosition(aAnchor, aRule, &filtered);

  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    return acc;
  }

  while (acc && acc != mRoot) {
    Accessible* parent = acc->Parent();
#if defined(ANDROID)
    MOZ_ASSERT(
        acc->IsLocal() || (acc->IsRemote() && parent->IsRemote()),
        "Pivot::SearchBackward climbed out of remote subtree in Android!");
#endif
    int32_t idxInParent = acc->IndexInParent();
    while (idxInParent > 0 && parent) {
      acc = parent->ChildAt(--idxInParent);
      if (!acc) {
        continue;
      }

      filtered = aRule.Match(acc);

      Accessible* lastChild = acc->LastChild();
      while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
             lastChild) {
        parent = acc;
        acc = lastChild;
        idxInParent = acc->IndexInParent();
        filtered = aRule.Match(acc);
        lastChild = acc->LastChild();
      }

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
        return acc;
      }
    }

    acc = parent;
    if (!acc) {
      break;
    }

    filtered = aRule.Match(acc);

    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
      return acc;
    }
  }

  return nullptr;
}

Accessible* Pivot::SearchForward(Accessible* aAnchor, PivotRule& aRule,
                                 bool aSearchCurrent) {
  // Initial position could be not set, in that case begin search from root.
  Accessible* acc = aAnchor ? aAnchor : mRoot;

  uint16_t filtered = nsIAccessibleTraversalRule::FILTER_IGNORE;
  acc = AdjustStartPosition(acc, aRule, &filtered);
  if (aSearchCurrent && (filtered & nsIAccessibleTraversalRule::FILTER_MATCH)) {
    return acc;
  }

  while (acc) {
    Accessible* firstChild = acc->FirstChild();
    while (!(filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) &&
           firstChild) {
      acc = firstChild;
      filtered = aRule.Match(acc);

      if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
        return acc;
      }
      firstChild = acc->FirstChild();
    }

    Accessible* sibling = nullptr;
    Accessible* temp = acc;
    do {
      if (temp == mRoot) {
        break;
      }

      sibling = temp->NextSibling();

      if (sibling) {
        break;
      }
      temp = temp->Parent();
#if defined(ANDROID)
      MOZ_ASSERT(
          acc->IsLocal() || (acc->IsRemote() && temp->IsRemote()),
          "Pivot::SearchForward climbed out of remote subtree in Android!");
#endif

    } while (temp);

    if (!sibling) {
      break;
    }

    acc = sibling;
    filtered = aRule.Match(acc);
    if (filtered & nsIAccessibleTraversalRule::FILTER_MATCH) {
      return acc;
    }
  }

  return nullptr;
}

Accessible* Pivot::SearchForText(Accessible* aAnchor, bool aBackward) {
  Accessible* accessible = aAnchor;
  while (true) {
    Accessible* child = nullptr;

    while ((child = (aBackward ? accessible->LastChild()
                               : accessible->FirstChild()))) {
      accessible = child;
      if (child->IsHyperText()) {
        return child;
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
        return temp;
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
      return accessible;
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
  Accessible* lastAcc = mRoot;

  // First go to the last accessible in pre-order
  while (lastAcc && lastAcc->HasChildren()) {
    lastAcc = lastAcc->LastChild();
  }

  // Search backwards from last accessible and find the last occurrence in the
  // doc
  return SearchBackward(lastAcc, aRule, true);
}

Accessible* Pivot::NextText(Accessible* aAnchor, int32_t* aStartOffset,
                            int32_t* aEndOffset, int32_t aBoundaryType) {
  int32_t tempStart = *aStartOffset, tempEnd = *aEndOffset;
  Accessible* tempPosition = aAnchor;

  // if we're starting on a text leaf, translate the offsets to the
  // HyperTextAccessible parent and start from there.
  if (aAnchor->IsTextLeaf() && aAnchor->Parent() &&
      aAnchor->Parent()->IsHyperText()) {
    tempPosition = aAnchor->Parent();
    HyperTextAccessibleBase* text = tempPosition->AsHyperTextBase();
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
    HyperTextAccessibleBase* text = nullptr;
    // Find the nearest text node using a preorder traversal starting from
    // the current node.
    if (!(text = tempPosition->AsHyperTextBase())) {
      tempPosition = SearchForText(tempPosition, false);
      if (!tempPosition) {
        return nullptr;
      }

      if (tempPosition != curPosition) {
        tempStart = tempEnd = -1;
      }
      text = tempPosition->AsHyperTextBase();
    }

    // If the search led to the parent of the node we started on (e.g. when
    // starting on a text leaf), start the text movement from the end of that
    // node, otherwise we just default to 0.
    if (tempEnd == -1) {
      tempEnd = tempPosition == curPosition->Parent()
                    ? text->GetChildOffset(curPosition)
                    : 0;
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
    tempPosition = aAnchor->Parent();
    HyperTextAccessibleBase* text = tempPosition->AsHyperTextBase();
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
    HyperTextAccessibleBase* text;
    // Find the nearest text node using a reverse preorder traversal starting
    // from the current node.
    if (!(text = tempPosition->AsHyperTextBase())) {
      tempPosition = SearchForText(tempPosition, true);
      if (!tempPosition) {
        return nullptr;
      }

      if (tempPosition != curPosition) {
        tempStart = tempEnd = -1;
      }
      text = tempPosition->AsHyperTextBase();
    }

    // If the search led to the parent of the node we started on (e.g. when
    // starting on a text leaf), start the text movement from the end offset
    // of that node. Otherwise we just default to the last offset in the parent.
    if (tempStart == -1) {
      if (tempPosition != curPosition &&
          tempPosition == curPosition->Parent()) {
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
          HyperTextAccessibleBase* siblingText = sibling->AsHyperTextBase();
          tempStart = tempEnd =
              siblingText ? siblingText->CharacterCount() : -1;
          tempPosition = sibling;
        } else {
          tempStart = tempPosition->StartOffset();
          tempEnd = tempPosition->EndOffset();
          tempPosition = tempPosition->Parent();
        }
      } else {
        tempPosition = SearchForText(tempPosition, true);
        if (!tempPosition) {
          return nullptr;
        }

        HyperTextAccessibleBase* tempText = tempPosition->AsHyperTextBase();
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
      if (childAtOffset && childAtOffset->IsHyperText()) {
        tempStart = childAtOffset->EndOffset();
        break;
      }
    }
    // If there's an embedded character at the very end of the range, we
    // instead want to traverse into it. So restart the movement with
    // the child as the starting point.
    if (childAtOffset && childAtOffset->IsHyperText() &&
        tempEnd == static_cast<int32_t>(childAtOffset->EndOffset())) {
      tempPosition = childAtOffset;
      tempStart = tempEnd = static_cast<int32_t>(
          childAtOffset->AsHyperTextBase()->CharacterCount());
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
  Accessible* child =
      mRoot ? mRoot->ChildAtPoint(aX, aY,
                                  Accessible::EWhichChildAtPoint::DeepestChild)
            : nullptr;
  while (child && (mRoot != child)) {
    uint16_t filtered = aRule.Match(child);

    // Ignore any matching nodes that were below this one
    if (filtered & nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE) {
      match = nullptr;
    }

    // Match if no node below this is a match
    if ((filtered & nsIAccessibleTraversalRule::FILTER_MATCH) && !match) {
      LayoutDeviceIntRect childRect = child->IsLocal()
                                          ? child->AsLocal()->Bounds()
                                          : child->AsRemote()->Bounds();
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

// Role Rule

PivotRoleRule::PivotRoleRule(mozilla::a11y::role aRole)
    : mRole(aRole), mDirectDescendantsFrom(nullptr) {}

PivotRoleRule::PivotRoleRule(mozilla::a11y::role aRole,
                             Accessible* aDirectDescendantsFrom)
    : mRole(aRole), mDirectDescendantsFrom(aDirectDescendantsFrom) {}

uint16_t PivotRoleRule::Match(Accessible* aAcc) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAcc)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (mDirectDescendantsFrom && (aAcc != mDirectDescendantsFrom)) {
    // If we've specified mDirectDescendantsFrom, we should ignore
    // non-direct descendants of from the specified AoP. Because
    // pivot performs a preorder traversal, the first aAcc
    // object(s) that don't equal mDirectDescendantsFrom will be
    // mDirectDescendantsFrom's children. We'll process them, but ignore
    // their subtrees thereby processing direct descendants of
    // mDirectDescendantsFrom only.
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (aAcc && aAcc->Role() == mRole) {
    result |= nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  return result;
}

// State Rule

PivotStateRule::PivotStateRule(uint64_t aState) : mState(aState) {}

uint16_t PivotStateRule::Match(Accessible* aAcc) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAcc)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (aAcc && (aAcc->State() & mState)) {
    result = nsIAccessibleTraversalRule::FILTER_MATCH |
             nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  return result;
}

// LocalAccInSameDocRule

uint16_t LocalAccInSameDocRule::Match(Accessible* aAcc) {
  LocalAccessible* acc = aAcc ? aAcc->AsLocal() : nullptr;
  if (!acc) {
    return nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }
  if (acc->IsOuterDoc()) {
    return nsIAccessibleTraversalRule::FILTER_MATCH |
           nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }
  return nsIAccessibleTraversalRule::FILTER_MATCH;
}

// Radio Button Name Rule

PivotRadioNameRule::PivotRadioNameRule(const nsString& aName) : mName(aName) {}

uint16_t PivotRadioNameRule::Match(Accessible* aAcc) {
  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;
  RemoteAccessible* remote = aAcc->AsRemote();
  if (!remote) {
    // We need the cache to be able to fetch the name attribute below.
    return result;
  }

  if (nsAccUtils::MustPrune(aAcc) || aAcc->IsOuterDoc()) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (remote->IsHTMLRadioButton()) {
    nsString currName = remote->GetCachedHTMLNameAttribute();
    if (!currName.IsEmpty() && mName.Equals(currName)) {
      result |= nsIAccessibleTraversalRule::FILTER_MATCH;
    }
  }

  return result;
}

// MustPruneSameDocRule

uint16_t MustPruneSameDocRule::Match(Accessible* aAcc) {
  if (!aAcc) {
    return nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  if (nsAccUtils::MustPrune(aAcc) || aAcc->IsOuterDoc()) {
    return nsIAccessibleTraversalRule::FILTER_MATCH |
           nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  return nsIAccessibleTraversalRule::FILTER_MATCH;
}
