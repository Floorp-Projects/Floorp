/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextRange-inl.h"

#include "LocalAccessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "mozilla/dom/Selection.h"
#include "nsAccUtils.h"

namespace mozilla {
namespace a11y {

////////////////////////////////////////////////////////////////////////////////
// TextPoint

bool TextPoint::operator<(const TextPoint& aPoint) const {
  if (mContainer == aPoint.mContainer) return mOffset < aPoint.mOffset;

  // Build the chain of parents
  LocalAccessible* p1 = mContainer;
  LocalAccessible* p2 = aPoint.mContainer;
  AutoTArray<LocalAccessible*, 30> parents1, parents2;
  do {
    parents1.AppendElement(p1);
    p1 = p1->LocalParent();
  } while (p1);
  do {
    parents2.AppendElement(p2);
    p2 = p2->LocalParent();
  } while (p2);

  // Find where the parent chain differs
  uint32_t pos1 = parents1.Length(), pos2 = parents2.Length();
  for (uint32_t len = std::min(pos1, pos2); len > 0; --len) {
    LocalAccessible* child1 = parents1.ElementAt(--pos1);
    LocalAccessible* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      return child1->IndexInParent() < child2->IndexInParent();
    }
  }

  if (pos1 != 0) {
    // If parents1 is a superset of parents2 then mContainer is a
    // descendant of aPoint.mContainer. The next element down in parents1
    // is mContainer's ancestor that is the child of aPoint.mContainer.
    // We compare its end offset in aPoint.mContainer with aPoint.mOffset.
    LocalAccessible* child = parents1.ElementAt(pos1 - 1);
    MOZ_ASSERT(child->LocalParent() == aPoint.mContainer);
    return child->EndOffset() < static_cast<uint32_t>(aPoint.mOffset);
  }

  if (pos2 != 0) {
    // If parents2 is a superset of parents1 then aPoint.mContainer is a
    // descendant of mContainer. The next element down in parents2
    // is aPoint.mContainer's ancestor that is the child of mContainer.
    // We compare its start offset in mContainer with mOffset.
    LocalAccessible* child = parents2.ElementAt(pos2 - 1);
    MOZ_ASSERT(child->LocalParent() == mContainer);
    return static_cast<uint32_t>(mOffset) < child->StartOffset();
  }

  NS_ERROR("Broken tree?!");
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// TextRange

TextRange::TextRange(HyperTextAccessible* aRoot,
                     HyperTextAccessible* aStartContainer, int32_t aStartOffset,
                     HyperTextAccessible* aEndContainer, int32_t aEndOffset)
    : mRoot(aRoot),
      mStartContainer(aStartContainer),
      mEndContainer(aEndContainer),
      mStartOffset(aStartOffset),
      mEndOffset(aEndOffset) {}

void TextRange::EmbeddedChildren(nsTArray<LocalAccessible*>* aChildren) const {
  if (mStartContainer == mEndContainer) {
    int32_t startIdx = mStartContainer->GetChildIndexAtOffset(mStartOffset);
    int32_t endIdx = mStartContainer->GetChildIndexAtOffset(mEndOffset);
    for (int32_t idx = startIdx; idx <= endIdx; idx++) {
      LocalAccessible* child = mStartContainer->LocalChildAt(idx);
      if (!child->IsText()) {
        aChildren->AppendElement(child);
      }
    }
    return;
  }

  LocalAccessible* p1 = mStartContainer->GetChildAtOffset(mStartOffset);
  LocalAccessible* p2 = mEndContainer->GetChildAtOffset(mEndOffset);

  uint32_t pos1 = 0, pos2 = 0;
  AutoTArray<LocalAccessible*, 30> parents1, parents2;
  LocalAccessible* container =
      CommonParent(p1, p2, &parents1, &pos1, &parents2, &pos2);

  // Traverse the tree up to the container and collect embedded objects.
  for (uint32_t idx = 0; idx < pos1 - 1; idx++) {
    LocalAccessible* parent = parents1[idx + 1];
    LocalAccessible* child = parents1[idx];
    uint32_t childCount = parent->ChildCount();
    for (uint32_t childIdx = child->IndexInParent(); childIdx < childCount;
         childIdx++) {
      LocalAccessible* next = parent->LocalChildAt(childIdx);
      if (!next->IsText()) {
        aChildren->AppendElement(next);
      }
    }
  }

  // Traverse through direct children in the container.
  int32_t endIdx = parents2[pos2 - 1]->IndexInParent();
  int32_t childIdx = parents1[pos1 - 1]->IndexInParent() + 1;
  for (; childIdx < endIdx; childIdx++) {
    LocalAccessible* next = container->LocalChildAt(childIdx);
    if (!next->IsText()) {
      aChildren->AppendElement(next);
    }
  }

  // Traverse down from the container to end point.
  for (int32_t idx = pos2 - 2; idx > 0; idx--) {
    LocalAccessible* parent = parents2[idx];
    LocalAccessible* child = parents2[idx - 1];
    int32_t endIdx = child->IndexInParent();
    for (int32_t childIdx = 0; childIdx < endIdx; childIdx++) {
      LocalAccessible* next = parent->LocalChildAt(childIdx);
      if (!next->IsText()) {
        aChildren->AppendElement(next);
      }
    }
  }
}

void TextRange::Text(nsAString& aText) const {
  LocalAccessible* current = mStartContainer->GetChildAtOffset(mStartOffset);
  uint32_t startIntlOffset =
      mStartOffset - mStartContainer->GetChildOffset(current);

  while (current && TextInternal(aText, current, startIntlOffset)) {
    current = current->LocalParent();
    if (!current) break;

    current = current->LocalNextSibling();
  }
}

void TextRange::Bounds(nsTArray<nsIntRect> aRects) const {}

void TextRange::Normalize(ETextUnit aUnit) {}

bool TextRange::Crop(LocalAccessible* aContainer) {
  uint32_t boundaryPos = 0, containerPos = 0;
  AutoTArray<LocalAccessible*, 30> boundaryParents, containerParents;

  // Crop the start boundary.
  LocalAccessible* container = nullptr;
  LocalAccessible* boundary = mStartContainer->GetChildAtOffset(mStartOffset);
  if (boundary != aContainer) {
    CommonParent(boundary, aContainer, &boundaryParents, &boundaryPos,
                 &containerParents, &containerPos);

    if (boundaryPos == 0) {
      if (containerPos != 0) {
        // The container is contained by the start boundary, reduce the range to
        // the point starting at the container.
        aContainer->ToTextPoint(mStartContainer.StartAssignment(),
                                &mStartOffset);
        static_cast<LocalAccessible*>(mStartContainer)->AddRef();
      } else {
        // The start boundary and the container are siblings.
        container = aContainer;
      }
    } else if (containerPos != 0) {
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
        container->ToTextPoint(mStartContainer.StartAssignment(),
                               &mStartOffset);
        mStartContainer.get()->AddRef();
      }
    }

    boundaryParents.SetLengthAndRetainStorage(0);
    containerParents.SetLengthAndRetainStorage(0);
  }

  boundary = mEndContainer->GetChildAtOffset(mEndOffset);
  if (boundary == aContainer) {
    return true;
  }

  // Crop the end boundary.
  container = nullptr;
  CommonParent(boundary, aContainer, &boundaryParents, &boundaryPos,
               &containerParents, &containerPos);

  if (boundaryPos == 0) {
    if (containerPos != 0) {
      aContainer->ToTextPoint(mEndContainer.StartAssignment(), &mEndOffset,
                              false);
      static_cast<LocalAccessible*>(mEndContainer)->AddRef();
    } else {
      container = aContainer;
    }
  } else if (containerPos != 0) {
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
    container->ToTextPoint(mEndContainer.StartAssignment(), &mEndOffset, false);
    static_cast<LocalAccessible*>(mEndContainer)->AddRef();
  }

  return true;
}

void TextRange::FindText(const nsAString& aText, EDirection aDirection,
                         nsCaseTreatment aCaseSensitive,
                         TextRange* aFoundRange) const {}

void TextRange::FindAttr(EAttr aAttr, nsIVariant* aValue, EDirection aDirection,
                         TextRange* aFoundRange) const {}

void TextRange::AddToSelection() const {}

void TextRange::RemoveFromSelection() const {}

bool TextRange::SetSelectionAt(int32_t aSelectionNum) const {
  RefPtr<dom::Selection> domSel = mRoot->DOMSelection();
  if (!domSel) {
    return false;
  }

  RefPtr<nsRange> range = nsRange::Create(mRoot->GetContent());
  uint32_t rangeCount = domSel->RangeCount();
  if (aSelectionNum == static_cast<int32_t>(rangeCount)) {
    range = nsRange::Create(mRoot->GetContent());
  } else {
    range = domSel->GetRangeAt(aSelectionNum);
  }

  if (!range) {
    return false;
  }

  bool reversed;
  AssignDOMRange(range, &reversed);

  // If this is not a new range, notify selection listeners that the existing
  // selection range has changed. Otherwise, just add the new range.
  if (aSelectionNum != static_cast<int32_t>(rangeCount)) {
    domSel->RemoveRangeAndUnselectFramesAndNotifyListeners(*range,
                                                           IgnoreErrors());
  }

  IgnoredErrorResult err;
  domSel->AddRangeAndSelectFramesAndNotifyListeners(*range, err);
  if (!err.Failed()) {
    // Changing the direction of the selection assures that the caret
    // will be at the logical end of the selection.
    domSel->SetDirection(reversed ? eDirPrevious : eDirNext);
    return true;
  }

  return false;
}

void TextRange::ScrollIntoView(uint32_t aScrollType) const {
  RefPtr<nsRange> range = nsRange::Create(mRoot->GetContent());
  if (AssignDOMRange(range)) {
    nsCoreUtils::ScrollSubstringTo(mStartContainer->GetFrame(), range,
                                   aScrollType);
  }
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
  bool reversed = EndPoint() < StartPoint();
  if (aReversed) {
    *aReversed = reversed;
  }

  DOMPoint startPoint = reversed
                            ? mEndContainer->OffsetToDOMPoint(mEndOffset)
                            : mStartContainer->OffsetToDOMPoint(mStartOffset);
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

  DOMPoint endPoint = reversed ? mStartContainer->OffsetToDOMPoint(mStartOffset)
                               : mEndContainer->OffsetToDOMPoint(mEndOffset);
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

  for (uint32_t idx = 0; idx < aSelection->RangeCount(); idx++) {
    const nsRange* DOMRange = aSelection->GetRangeAt(idx);
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

void TextRange::Set(HyperTextAccessible* aRoot,
                    HyperTextAccessible* aStartContainer, int32_t aStartOffset,
                    HyperTextAccessible* aEndContainer, int32_t aEndOffset) {
  mRoot = aRoot;
  mStartContainer = aStartContainer;
  mEndContainer = aEndContainer;
  mStartOffset = aStartOffset;
  mEndOffset = aEndOffset;
}

bool TextRange::TextInternal(nsAString& aText, LocalAccessible* aCurrent,
                             uint32_t aStartIntlOffset) const {
  bool moveNext = true;
  int32_t endIntlOffset = -1;
  if (aCurrent->LocalParent() == mEndContainer &&
      mEndContainer->GetChildAtOffset(mEndOffset) == aCurrent) {
    uint32_t currentStartOffset = mEndContainer->GetChildOffset(aCurrent);
    endIntlOffset = mEndOffset - currentStartOffset;
    if (endIntlOffset == 0) return false;

    moveNext = false;
  }

  if (aCurrent->IsTextLeaf()) {
    aCurrent->AppendTextTo(aText, aStartIntlOffset,
                           endIntlOffset - aStartIntlOffset);
    if (!moveNext) return false;
  }

  LocalAccessible* next = aCurrent->LocalFirstChild();
  if (next) {
    if (!TextInternal(aText, next, 0)) return false;
  }

  next = aCurrent->LocalNextSibling();
  if (next) {
    if (!TextInternal(aText, next, 0)) return false;
  }

  return moveNext;
}

void TextRange::MoveInternal(ETextUnit aUnit, int32_t aCount,
                             HyperTextAccessible& aContainer, int32_t aOffset,
                             HyperTextAccessible* aStopContainer,
                             int32_t aStopOffset) {}

LocalAccessible* TextRange::CommonParent(LocalAccessible* aAcc1,
                                         LocalAccessible* aAcc2,
                                         nsTArray<LocalAccessible*>* aParents1,
                                         uint32_t* aPos1,
                                         nsTArray<LocalAccessible*>* aParents2,
                                         uint32_t* aPos2) const {
  if (aAcc1 == aAcc2) {
    return aAcc1;
  }

  MOZ_ASSERT(aParents1->Length() == 0 || aParents2->Length() == 0,
             "Wrong arguments");

  // Build the chain of parents.
  LocalAccessible* p1 = aAcc1;
  LocalAccessible* p2 = aAcc2;
  do {
    aParents1->AppendElement(p1);
    p1 = p1->LocalParent();
  } while (p1);
  do {
    aParents2->AppendElement(p2);
    p2 = p2->LocalParent();
  } while (p2);

  // Find where the parent chain differs
  *aPos1 = aParents1->Length();
  *aPos2 = aParents2->Length();
  LocalAccessible* parent = nullptr;
  uint32_t len = 0;
  for (len = std::min(*aPos1, *aPos2); len > 0; --len) {
    LocalAccessible* child1 = aParents1->ElementAt(--(*aPos1));
    LocalAccessible* child2 = aParents2->ElementAt(--(*aPos2));
    if (child1 != child2) break;

    parent = child1;
  }

  return parent;
}

}  // namespace a11y
}  // namespace mozilla
