/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextRange-inl.h"

#include "LocalAccessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/dom/Selection.h"
#include "nsAccUtils.h"

namespace mozilla {
namespace a11y {

/**
 * Returns a text point for aAcc within aContainer.
 */
static void ToTextPoint(Accessible* aAcc, Accessible** aContainer,
                        int32_t* aOffset, bool aIsBefore = true) {
  if (aAcc->IsHyperText()) {
    *aContainer = aAcc;
    *aOffset =
        aIsBefore
            ? 0
            : static_cast<int32_t>(aAcc->AsHyperTextBase()->CharacterCount());
    return;
  }

  Accessible* child = nullptr;
  Accessible* parent = aAcc;
  do {
    child = parent;
    parent = parent->Parent();
  } while (parent && !parent->IsHyperText());

  if (parent) {
    *aContainer = parent;
    *aOffset = parent->AsHyperTextBase()->GetChildOffset(
        child->IndexInParent() + static_cast<int32_t>(!aIsBefore));
  }
}

////////////////////////////////////////////////////////////////////////////////
// TextPoint

bool TextPoint::operator<(const TextPoint& aPoint) const {
  if (mContainer == aPoint.mContainer) return mOffset < aPoint.mOffset;

  // Build the chain of parents
  Accessible* p1 = mContainer;
  Accessible* p2 = aPoint.mContainer;
  AutoTArray<Accessible*, 30> parents1, parents2;
  do {
    parents1.AppendElement(p1);
    p1 = p1->Parent();
  } while (p1);
  do {
    parents2.AppendElement(p2);
    p2 = p2->Parent();
  } while (p2);

  // Find where the parent chain differs
  uint32_t pos1 = parents1.Length(), pos2 = parents2.Length();
  for (uint32_t len = std::min(pos1, pos2); len > 0; --len) {
    Accessible* child1 = parents1.ElementAt(--pos1);
    Accessible* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      return child1->IndexInParent() < child2->IndexInParent();
    }
  }

  if (pos1 != 0) {
    // If parents1 is a superset of parents2 then mContainer is a
    // descendant of aPoint.mContainer. The next element down in parents1
    // is mContainer's ancestor that is the child of aPoint.mContainer.
    // We compare its end offset in aPoint.mContainer with aPoint.mOffset.
    Accessible* child = parents1.ElementAt(pos1 - 1);
    MOZ_ASSERT(child->Parent() == aPoint.mContainer);
    return child->EndOffset() < static_cast<uint32_t>(aPoint.mOffset);
  }

  if (pos2 != 0) {
    // If parents2 is a superset of parents1 then aPoint.mContainer is a
    // descendant of mContainer. The next element down in parents2
    // is aPoint.mContainer's ancestor that is the child of mContainer.
    // We compare its start offset in mContainer with mOffset.
    Accessible* child = parents2.ElementAt(pos2 - 1);
    MOZ_ASSERT(child->Parent() == mContainer);
    return static_cast<uint32_t>(mOffset) < child->StartOffset();
  }

  NS_ERROR("Broken tree?!");
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// TextRange

TextRange::TextRange(Accessible* aRoot, Accessible* aStartContainer,
                     int32_t aStartOffset, Accessible* aEndContainer,
                     int32_t aEndOffset)
    : mRoot(aRoot),
      mStartContainer(aStartContainer),
      mEndContainer(aEndContainer),
      mStartOffset(aStartOffset),
      mEndOffset(aEndOffset) {}

bool TextRange::Crop(Accessible* aContainer) {
  uint32_t boundaryPos = 0, containerPos = 0;
  AutoTArray<Accessible*, 30> boundaryParents, containerParents;

  // Crop the start boundary.
  Accessible* container = nullptr;
  HyperTextAccessibleBase* startHyper = mStartContainer->AsHyperTextBase();
  Accessible* boundary = startHyper->GetChildAtOffset(mStartOffset);
  if (boundary != aContainer) {
    CommonParent(boundary, aContainer, &boundaryParents, &boundaryPos,
                 &containerParents, &containerPos);

    if (boundaryPos == 0) {
      if (containerPos != 0) {
        // The container is contained by the start boundary, reduce the range to
        // the point starting at the container.
        ToTextPoint(aContainer, &mStartContainer, &mStartOffset);
      } else {
        // The start boundary and the container are siblings.
        container = aContainer;
      }
    } else {
      // The container does not contain the start boundary.
      boundary = boundaryParents[boundaryPos];
      container = containerParents[containerPos];
    }

    if (container) {
      // If the range start is after the container, then make the range invalid.
      if (boundary->IndexInParent() > container->IndexInParent()) {
        return !!(mRoot = nullptr);
      }

      // If the range starts before the container, then reduce the range to
      // the point starting at the container.
      if (boundary->IndexInParent() < container->IndexInParent()) {
        ToTextPoint(container, &mStartContainer, &mStartOffset);
      }
    }

    boundaryParents.SetLengthAndRetainStorage(0);
    containerParents.SetLengthAndRetainStorage(0);
  }

  HyperTextAccessibleBase* endHyper = mEndContainer->AsHyperTextBase();
  boundary = endHyper->GetChildAtOffset(mEndOffset);
  if (boundary == aContainer) {
    return true;
  }

  // Crop the end boundary.
  container = nullptr;
  CommonParent(boundary, aContainer, &boundaryParents, &boundaryPos,
               &containerParents, &containerPos);

  if (boundaryPos == 0) {
    if (containerPos != 0) {
      ToTextPoint(aContainer, &mEndContainer, &mEndOffset, false);
    } else {
      container = aContainer;
    }
  } else {
    boundary = boundaryParents[boundaryPos];
    container = containerParents[containerPos];
  }

  if (!container) {
    return true;
  }

  if (boundary->IndexInParent() < container->IndexInParent()) {
    return !!(mRoot = nullptr);
  }

  if (boundary->IndexInParent() > container->IndexInParent()) {
    ToTextPoint(container, &mEndContainer, &mEndOffset, false);
  }

  return true;
}

/**
 * Convert the given DOM point to a DOM point in non-generated contents.
 *
 * If aDOMPoint is in ::before, the result is immediately after it.
 * If aDOMPoint is in ::after, the result is immediately before it.
 */
static DOMPoint ClosestNotGeneratedDOMPoint(const DOMPoint& aDOMPoint,
                                            nsIContent* aElementContent) {
  MOZ_ASSERT(aDOMPoint.node, "The node must not be null");

  // ::before pseudo element
  if (aElementContent &&
      aElementContent->IsGeneratedContentContainerForBefore()) {
    MOZ_ASSERT(aElementContent->GetParent(),
               "::before must have parent element");
    // The first child of its parent (i.e., immediately after the ::before) is
    // good point for a DOM range.
    return DOMPoint(aElementContent->GetParent(), 0);
  }

  // ::after pseudo element
  if (aElementContent &&
      aElementContent->IsGeneratedContentContainerForAfter()) {
    MOZ_ASSERT(aElementContent->GetParent(),
               "::after must have parent element");
    // The end of its parent (i.e., immediately before the ::after) is good
    // point for a DOM range.
    return DOMPoint(aElementContent->GetParent(),
                    aElementContent->GetParent()->GetChildCount());
  }

  return aDOMPoint;
}

/**
 * GetElementAsContentOf() returns a content representing an element which is
 * or includes aNode.
 *
 * XXX This method is enough to retrieve ::before or ::after pseudo element.
 *     So, if you want to use this for other purpose, you might need to check
 *     ancestors too.
 */
static nsIContent* GetElementAsContentOf(nsINode* aNode) {
  if (auto* element = dom::Element::FromNode(aNode)) {
    return element;
  }
  return aNode->GetParentElement();
}

bool TextRange::AssignDOMRange(nsRange* aRange, bool* aReversed) const {
  MOZ_ASSERT(mRoot->IsLocal(), "Not supported for RemoteAccessible");
  bool reversed = EndPoint() < StartPoint();
  if (aReversed) {
    *aReversed = reversed;
  }

  HyperTextAccessible* startHyper = mStartContainer->AsLocal()->AsHyperText();
  HyperTextAccessible* endHyper = mEndContainer->AsLocal()->AsHyperText();
  DOMPoint startPoint = reversed ? endHyper->OffsetToDOMPoint(mEndOffset)
                                 : startHyper->OffsetToDOMPoint(mStartOffset);
  if (!startPoint.node) {
    return false;
  }

  // HyperTextAccessible manages pseudo elements generated by ::before or
  // ::after.  However, contents of them are not in the DOM tree normally.
  // Therefore, they are not selectable and editable.  So, when this creates
  // a DOM range, it should not start from nor end in any pseudo contents.

  nsIContent* container = GetElementAsContentOf(startPoint.node);
  DOMPoint startPointForDOMRange =
      ClosestNotGeneratedDOMPoint(startPoint, container);
  aRange->SetStart(startPointForDOMRange.node, startPointForDOMRange.idx);

  // If the caller wants collapsed range, let's collapse the range to its start.
  if (mEndContainer == mStartContainer && mEndOffset == mStartOffset) {
    aRange->Collapse(true);
    return true;
  }

  DOMPoint endPoint = reversed ? startHyper->OffsetToDOMPoint(mStartOffset)
                               : endHyper->OffsetToDOMPoint(mEndOffset);
  if (!endPoint.node) {
    return false;
  }

  if (startPoint.node != endPoint.node) {
    container = GetElementAsContentOf(endPoint.node);
  }

  DOMPoint endPointForDOMRange =
      ClosestNotGeneratedDOMPoint(endPoint, container);
  aRange->SetEnd(endPointForDOMRange.node, endPointForDOMRange.idx);
  return true;
}

void TextRange::TextRangesFromSelection(dom::Selection* aSelection,
                                        nsTArray<TextRange>* aRanges) {
  MOZ_ASSERT(aRanges->Length() == 0, "TextRange array supposed to be empty");

  aRanges->SetCapacity(aSelection->RangeCount());

  const uint32_t rangeCount = aSelection->RangeCount();
  for (const uint32_t idx : IntegerRange(rangeCount)) {
    MOZ_ASSERT(aSelection->RangeCount() == rangeCount);
    const nsRange* DOMRange = aSelection->GetRangeAt(idx);
    MOZ_ASSERT(DOMRange);
    HyperTextAccessible* startContainer =
        nsAccUtils::GetTextContainer(DOMRange->GetStartContainer());
    HyperTextAccessible* endContainer =
        nsAccUtils::GetTextContainer(DOMRange->GetEndContainer());
    HyperTextAccessible* commonAncestor = nsAccUtils::GetTextContainer(
        DOMRange->GetClosestCommonInclusiveAncestor());
    if (!startContainer || !endContainer) {
      continue;
    }

    int32_t startOffset = startContainer->DOMPointToOffset(
        DOMRange->GetStartContainer(), DOMRange->StartOffset(), false);
    int32_t endOffset = endContainer->DOMPointToOffset(
        DOMRange->GetEndContainer(), DOMRange->EndOffset(), true);

    TextRange tr(commonAncestor && commonAncestor->IsTextField()
                     ? commonAncestor
                     : startContainer->Document(),
                 startContainer, startOffset, endContainer, endOffset);
    *(aRanges->AppendElement()) = std::move(tr);
  }
}

////////////////////////////////////////////////////////////////////////////////
// pivate

void TextRange::Set(Accessible* aRoot, Accessible* aStartContainer,
                    int32_t aStartOffset, Accessible* aEndContainer,
                    int32_t aEndOffset) {
  mRoot = aRoot;
  mStartContainer = aStartContainer;
  mEndContainer = aEndContainer;
  mStartOffset = aStartOffset;
  mEndOffset = aEndOffset;
}

Accessible* TextRange::CommonParent(Accessible* aAcc1, Accessible* aAcc2,
                                    nsTArray<Accessible*>* aParents1,
                                    uint32_t* aPos1,
                                    nsTArray<Accessible*>* aParents2,
                                    uint32_t* aPos2) const {
  if (aAcc1 == aAcc2) {
    return aAcc1;
  }

  MOZ_ASSERT(aParents1->Length() == 0 || aParents2->Length() == 0,
             "Wrong arguments");

  // Build the chain of parents.
  Accessible* p1 = aAcc1;
  Accessible* p2 = aAcc2;
  do {
    aParents1->AppendElement(p1);
    p1 = p1->Parent();
  } while (p1);
  do {
    aParents2->AppendElement(p2);
    p2 = p2->Parent();
  } while (p2);

  // Find where the parent chain differs
  *aPos1 = aParents1->Length();
  *aPos2 = aParents2->Length();
  Accessible* parent = nullptr;
  uint32_t len = 0;
  for (len = std::min(*aPos1, *aPos2); len > 0; --len) {
    Accessible* child1 = aParents1->ElementAt(--(*aPos1));
    Accessible* child2 = aParents2->ElementAt(--(*aPos2));
    if (child1 != child2) break;

    parent = child1;
  }

  return parent;
}

}  // namespace a11y
}  // namespace mozilla
