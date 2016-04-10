/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextRange-inl.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"

namespace mozilla {
namespace a11y {

////////////////////////////////////////////////////////////////////////////////
// TextPoint

bool
TextPoint::operator <(const TextPoint& aPoint) const
{
  if (mContainer == aPoint.mContainer)
    return mOffset < aPoint.mOffset;

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
    if (child1 != child2)
      return child1->IndexInParent() < child2->IndexInParent();
  }

  NS_ERROR("Broken tree?!");
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// TextRange

TextRange::TextRange(HyperTextAccessible* aRoot,
                     HyperTextAccessible* aStartContainer, int32_t aStartOffset,
                     HyperTextAccessible* aEndContainer, int32_t aEndOffset) :
  mRoot(aRoot), mStartContainer(aStartContainer), mEndContainer(aEndContainer),
  mStartOffset(aStartOffset), mEndOffset(aEndOffset)
{
}

void
TextRange::EmbeddedChildren(nsTArray<Accessible*>* aChildren) const
{
  if (mStartContainer == mEndContainer) {
    int32_t startIdx = mStartContainer->GetChildIndexAtOffset(mStartOffset);
    int32_t endIdx = mStartContainer->GetChildIndexAtOffset(mEndOffset);
    for (int32_t idx = startIdx; idx <= endIdx; idx++) {
      Accessible* child = mStartContainer->GetChildAt(idx);
      if (!child->IsText()) {
        aChildren->AppendElement(child);
      }
    }
    return;
  }

  Accessible* p1 = mStartContainer->GetChildAtOffset(mStartOffset);
  Accessible* p2 = mEndContainer->GetChildAtOffset(mEndOffset);

  uint32_t pos1 = 0, pos2 = 0;
  AutoTArray<Accessible*, 30> parents1, parents2;
  Accessible* container =
    CommonParent(p1, p2, &parents1, &pos1, &parents2, &pos2);

  // Traverse the tree up to the container and collect embedded objects.
  for (uint32_t idx = 0; idx < pos1 - 1; idx++) {
    Accessible* parent = parents1[idx + 1];
    Accessible* child = parents1[idx];
    uint32_t childCount = parent->ChildCount();
    for (uint32_t childIdx = child->IndexInParent(); childIdx < childCount; childIdx++) {
      Accessible* next = parent->GetChildAt(childIdx);
      if (!next->IsText()) {
        aChildren->AppendElement(next);
      }
    }
  }

  // Traverse through direct children in the container.
  int32_t endIdx = parents2[pos2 - 1]->IndexInParent();
  int32_t childIdx = parents1[pos1 - 1]->IndexInParent() + 1;
  for (; childIdx < endIdx; childIdx++) {
    Accessible* next = container->GetChildAt(childIdx);
    if (!next->IsText()) {
      aChildren->AppendElement(next);
    }
  }

  // Traverse down from the container to end point.
  for (int32_t idx = pos2 - 2; idx > 0; idx--) {
    Accessible* parent = parents2[idx];
    Accessible* child = parents2[idx - 1];
    int32_t endIdx = child->IndexInParent();
    for (int32_t childIdx = 0; childIdx < endIdx; childIdx++) {
      Accessible* next = parent->GetChildAt(childIdx);
      if (!next->IsText()) {
        aChildren->AppendElement(next);
      }
    }
  }
}

void
TextRange::Text(nsAString& aText) const
{
  Accessible* current = mStartContainer->GetChildAtOffset(mStartOffset);
  uint32_t startIntlOffset =
    mStartOffset - mStartContainer->GetChildOffset(current);

  while (current && TextInternal(aText, current, startIntlOffset)) {
    current = current->Parent();
    if (!current)
      break;

    current = current->NextSibling();
  }
}

void
TextRange::Bounds(nsTArray<nsIntRect> aRects) const
{

}

void
TextRange::Normalize(ETextUnit aUnit)
{

}

bool
TextRange::Crop(Accessible* aContainer)
{
  uint32_t boundaryPos = 0, containerPos = 0;
  AutoTArray<Accessible*, 30> boundaryParents, containerParents;

  // Crop the start boundary.
  Accessible* container = nullptr;
  Accessible* boundary = mStartContainer->GetChildAtOffset(mStartOffset);
  if (boundary != aContainer) {
    CommonParent(boundary, aContainer, &boundaryParents, &boundaryPos,
                 &containerParents, &containerPos);

    if (boundaryPos == 0) {
      if (containerPos != 0) {
        // The container is contained by the start boundary, reduce the range to
        // the point starting at the container.
        aContainer->ToTextPoint(mStartContainer.StartAssignment(), &mStartOffset);
        static_cast<Accessible*>(mStartContainer)->AddRef();
      }
      else {
        // The start boundary and the container are siblings.
        container = aContainer;
      }
    }
    else if (containerPos != 0) {
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
        container->ToTextPoint(mStartContainer.StartAssignment(), &mStartOffset);
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
      aContainer->ToTextPoint(mEndContainer.StartAssignment(), &mEndOffset, false);
      static_cast<Accessible*>(mEndContainer)->AddRef();
    }
    else {
      container = aContainer;
    }
  }
  else if (containerPos != 0) {
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
    static_cast<Accessible*>(mEndContainer)->AddRef();
  }

  return true;
}

void
TextRange::FindText(const nsAString& aText, EDirection aDirection,
                    nsCaseTreatment aCaseSensitive, TextRange* aFoundRange) const
{

}

void
TextRange::FindAttr(EAttr aAttr, nsIVariant* aValue, EDirection aDirection,
                    TextRange* aFoundRange) const
{

}

void
TextRange::AddToSelection() const
{

}

void
TextRange::RemoveFromSelection() const
{

}

void
TextRange::Select() const
{
}

void
TextRange::ScrollIntoView(EHowToAlign aHow) const
{

}

////////////////////////////////////////////////////////////////////////////////
// pivate

void
TextRange::Set(HyperTextAccessible* aRoot,
               HyperTextAccessible* aStartContainer, int32_t aStartOffset,
               HyperTextAccessible* aEndContainer, int32_t aEndOffset)
{
  mRoot = aRoot;
  mStartContainer = aStartContainer;
  mEndContainer = aEndContainer;
  mStartOffset = aStartOffset;
  mEndOffset = aEndOffset;
}

bool
TextRange::TextInternal(nsAString& aText, Accessible* aCurrent,
                        uint32_t aStartIntlOffset) const
{
  bool moveNext = true;
  int32_t endIntlOffset = -1;
  if (aCurrent->Parent() == mEndContainer &&
      mEndContainer->GetChildAtOffset(mEndOffset) == aCurrent) {

    uint32_t currentStartOffset = mEndContainer->GetChildOffset(aCurrent);
    endIntlOffset = mEndOffset - currentStartOffset;
    if (endIntlOffset == 0)
      return false;

    moveNext = false;
  }

  if (aCurrent->IsTextLeaf()) {
    aCurrent->AppendTextTo(aText, aStartIntlOffset,
                           endIntlOffset - aStartIntlOffset);
    if (!moveNext)
      return false;
  }

  Accessible* next = aCurrent->FirstChild();
  if (next) {
    if (!TextInternal(aText, next, 0))
      return false;
  }

  next = aCurrent->NextSibling();
  if (next) {
    if (!TextInternal(aText, next, 0))
      return false;
  }

  return moveNext;
}


void
TextRange::MoveInternal(ETextUnit aUnit, int32_t aCount,
                        HyperTextAccessible& aContainer, int32_t aOffset,
                        HyperTextAccessible* aStopContainer, int32_t aStopOffset)
{

}

Accessible*
TextRange::CommonParent(Accessible* aAcc1, Accessible* aAcc2,
                        nsTArray<Accessible*>* aParents1, uint32_t* aPos1,
                        nsTArray<Accessible*>* aParents2, uint32_t* aPos2) const
{
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
    if (child1 != child2)
      break;

    parent = child1;
  }

  return parent;
}

} // namespace a11y
} // namespace mozilla
