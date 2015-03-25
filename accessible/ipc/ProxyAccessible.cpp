/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAccessible.h"
#include "DocAccessibleParent.h"
#include "mozilla/unused.h"
#include "mozilla/a11y/Platform.h"
#include "RelationType.h"
#include "mozilla/a11y/Role.h"

namespace mozilla {
namespace a11y {

void
ProxyAccessible::Shutdown()
{
  NS_ASSERTION(!mOuterDoc, "Why do we still have a child doc?");

  // XXX Ideally  this wouldn't be necessary, but it seems OuterDoc accessibles
  // can be destroyed before the doc they own.
  if (!mOuterDoc) {
    uint32_t childCount = mChildren.Length();
    for (uint32_t idx = 0; idx < childCount; idx++)
      mChildren[idx]->Shutdown();
  } else {
    if (mChildren.Length() != 1)
      MOZ_CRASH("outer doc doesn't own adoc!");

    static_cast<DocAccessibleParent*>(mChildren[0])->Unbind();
  }

  mChildren.Clear();
  ProxyDestroyed(this);
  mDoc->RemoveAccessible(this);
}

void
ProxyAccessible::SetChildDoc(DocAccessibleParent* aParent)
{
  if (aParent) {
    MOZ_ASSERT(mChildren.IsEmpty());
    mChildren.AppendElement(aParent);
    mOuterDoc = true;
  } else {
    MOZ_ASSERT(mChildren.Length() == 1);
    mChildren.Clear();
    mOuterDoc = false;
  }
}

bool
ProxyAccessible::MustPruneChildren() const
{
  // this is the equivalent to nsAccUtils::MustPrune for proxies and should be
  // kept in sync with that.
  if (mRole != roles::MENUITEM && mRole != roles::COMBOBOX_OPTION
      && mRole != roles::OPTION && mRole != roles::ENTRY
      && mRole != roles::FLAT_EQUATION && mRole != roles::PASSWORD_TEXT
      && mRole != roles::PUSHBUTTON && mRole != roles::TOGGLE_BUTTON
      && mRole != roles::GRAPHIC && mRole != roles::SLIDER
      && mRole != roles::PROGRESSBAR && mRole != roles::SEPARATOR)
    return false;

  if (mChildren.Length() != 1)
    return false;

  return mChildren[0]->Role() == roles::TEXT_LEAF
    || mChildren[0]->Role() == roles::STATICTEXT;
}

uint64_t
ProxyAccessible::State() const
{
  uint64_t state = 0;
  unused << mDoc->SendState(mID, &state);
  return state;
}

void
ProxyAccessible::Name(nsString& aName) const
{
  unused << mDoc->SendName(mID, &aName);
}

void
ProxyAccessible::Value(nsString& aValue) const
{
  unused << mDoc->SendValue(mID, &aValue);
}

void
ProxyAccessible::Description(nsString& aDesc) const
{
  unused << mDoc->SendDescription(mID, &aDesc);
}

void
ProxyAccessible::Attributes(nsTArray<Attribute> *aAttrs) const
{
  unused << mDoc->SendAttributes(mID, aAttrs);
}

nsTArray<ProxyAccessible*>
ProxyAccessible::RelationByType(RelationType aType) const
{
  nsTArray<uint64_t> targetIDs;
  unused << mDoc->SendRelationByType(mID, static_cast<uint32_t>(aType),
                                     &targetIDs);

  size_t targetCount = targetIDs.Length();
  nsTArray<ProxyAccessible*> targets(targetCount);
  for (size_t i = 0; i < targetCount; i++)
    if (ProxyAccessible* proxy = mDoc->GetAccessible(targetIDs[i]))
      targets.AppendElement(proxy);

  return Move(targets);
}

void
ProxyAccessible::Relations(nsTArray<RelationType>* aTypes,
                           nsTArray<nsTArray<ProxyAccessible*>>* aTargetSets)
  const
{
  nsTArray<RelationTargets> ipcRelations;
  unused << mDoc->SendRelations(mID, &ipcRelations);

  size_t relationCount = ipcRelations.Length();
  aTypes->SetCapacity(relationCount);
  aTargetSets->SetCapacity(relationCount);
  for (size_t i = 0; i < relationCount; i++) {
    uint32_t type = ipcRelations[i].Type();
    if (type > static_cast<uint32_t>(RelationType::LAST))
      continue;

    size_t targetCount = ipcRelations[i].Targets().Length();
    nsTArray<ProxyAccessible*> targets(targetCount);
    for (size_t j = 0; j < targetCount; j++)
      if (ProxyAccessible* proxy = mDoc->GetAccessible(ipcRelations[i].Targets()[j]))
        targets.AppendElement(proxy);

    if (targets.IsEmpty())
      continue;

    aTargetSets->AppendElement(Move(targets));
    aTypes->AppendElement(static_cast<RelationType>(type));
  }
}

int32_t
ProxyAccessible::CaretOffset()
{
  int32_t offset = 0;
  unused << mDoc->SendCaretOffset(mID, &offset);
  return offset;
}

bool
ProxyAccessible::SetCaretOffset(int32_t aOffset)
{
  bool valid = false;
  unused << mDoc->SendSetCaretOffset(mID, aOffset, &valid);
  return valid;
}

int32_t
ProxyAccessible::CharacterCount()
{
  int32_t count = 0;
  unused << mDoc->SendCharacterCount(mID, &count);
  return count;
}

int32_t
ProxyAccessible::SelectionCount()
{
  int32_t count = 0;
  unused << mDoc->SendSelectionCount(mID, &count);
  return count;
}

void
ProxyAccessible::TextSubstring(int32_t aStartOffset, int32_t aEndOfset,
                               nsString& aText) const
{
  unused << mDoc->SendTextSubstring(mID, aStartOffset, aEndOfset, &aText);
}

void
ProxyAccessible::GetTextAfterOffset(int32_t aOffset,
                                    AccessibleTextBoundary aBoundaryType,
                                    nsString& aText, int32_t* aStartOffset,
                                    int32_t* aEndOffset)
{
  unused << mDoc->SendGetTextAfterOffset(mID, aOffset, aBoundaryType,
                                         &aText, aStartOffset, aEndOffset);
}

void
ProxyAccessible::GetTextAtOffset(int32_t aOffset,
                                 AccessibleTextBoundary aBoundaryType,
                                 nsString& aText, int32_t* aStartOffset,
                                 int32_t* aEndOffset)
{
  unused << mDoc->SendGetTextAtOffset(mID, aOffset, aBoundaryType,
                                      &aText, aStartOffset, aEndOffset);
}

void
ProxyAccessible::GetTextBeforeOffset(int32_t aOffset,
                                     AccessibleTextBoundary aBoundaryType,
                                     nsString& aText, int32_t* aStartOffset,
                                     int32_t* aEndOffset)
{
  unused << mDoc->SendGetTextBeforeOffset(mID, aOffset, aBoundaryType,
                                          &aText, aStartOffset, aEndOffset);
}

char16_t
ProxyAccessible::CharAt(int32_t aOffset)
{
  uint16_t retval = 0;
  unused << mDoc->SendCharAt(mID, aOffset, &retval);
  return static_cast<char16_t>(retval);
}

void
ProxyAccessible::TextAttributes(bool aIncludeDefAttrs,
                                int32_t aOffset,
                                nsTArray<Attribute>* aAttributes,
                                int32_t* aStartOffset,
                                int32_t* aEndOffset)
{
  unused << mDoc->SendTextAttributes(mID, aIncludeDefAttrs, aOffset,
                                     aAttributes, aStartOffset, aEndOffset);
}

void
ProxyAccessible::DefaultTextAttributes(nsTArray<Attribute>* aAttrs)
{
  unused << mDoc->SendDefaultTextAttributes(mID, aAttrs);
}

nsIntRect
ProxyAccessible::TextBounds(int32_t aStartOffset, int32_t aEndOffset,
                            uint32_t aCoordType)
{
  nsIntRect rect;
  unused <<
    mDoc->SendTextBounds(mID, aStartOffset, aEndOffset, aCoordType, &rect);
  return rect;
}

nsIntRect
ProxyAccessible::CharBounds(int32_t aOffset, uint32_t aCoordType)
{
  nsIntRect rect;
  unused <<
    mDoc->SendCharBounds(mID, aOffset, aCoordType, &rect);
  return rect;
}

int32_t
ProxyAccessible::OffsetAtPoint(int32_t aX, int32_t aY, uint32_t aCoordType)
{
  int32_t retVal = -1;
  unused << mDoc->SendOffsetAtPoint(mID, aX, aY, aCoordType, &retVal);
  return retVal;
}

bool
ProxyAccessible::SelectionBoundsAt(int32_t aSelectionNum,
                                   nsString& aData,
                                   int32_t* aStartOffset,
                                   int32_t* aEndOffset)
{
  bool retVal = false;
  unused << mDoc->SendSelectionBoundsAt(mID, aSelectionNum, &retVal, &aData,
                                        aStartOffset, aEndOffset);
  return retVal;
}

bool
ProxyAccessible::SetSelectionBoundsAt(int32_t aSelectionNum,
                                      int32_t aStartOffset,
                                      int32_t aEndOffset)
{
  bool retVal = false;
  unused << mDoc->SendSetSelectionBoundsAt(mID, aSelectionNum, aStartOffset,
                                           aEndOffset, &retVal);
  return retVal;
}

bool
ProxyAccessible::AddToSelection(int32_t aStartOffset,
                                int32_t aEndOffset)
{
  bool retVal = false;
  unused << mDoc->SendAddToSelection(mID, aStartOffset, aEndOffset, &retVal);
  return retVal;
}

bool
ProxyAccessible::RemoveFromSelection(int32_t aSelectionNum)
{
  bool retVal = false;
  unused << mDoc->SendRemoveFromSelection(mID, aSelectionNum, &retVal);
  return retVal;
}

void
ProxyAccessible::ScrollSubstringTo(int32_t aStartOffset, int32_t aEndOffset,
                                   uint32_t aScrollType)
{
  unused << mDoc->SendScrollSubstringTo(mID, aStartOffset, aEndOffset, aScrollType);
}

void
ProxyAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                        int32_t aEndOffset,
                                        uint32_t aCoordinateType,
                                        int32_t aX, int32_t aY)
{
  unused << mDoc->SendScrollSubstringToPoint(mID, aStartOffset, aEndOffset,
                                             aCoordinateType, aX, aY);
}

void
ProxyAccessible::ReplaceText(const nsString& aText)
{
  unused << mDoc->SendReplaceText(mID, aText);
}

void
ProxyAccessible::InsertText(const nsString& aText, int32_t aPosition)
{
  unused << mDoc->SendInsertText(mID, aText, aPosition);
}

void
ProxyAccessible::CopyText(int32_t aStartPos, int32_t aEndPos)
{
  unused << mDoc->SendCopyText(mID, aStartPos, aEndPos);
}

void
ProxyAccessible::CutText(int32_t aStartPos, int32_t aEndPos)
{
  unused << mDoc->SendCutText(mID, aStartPos, aEndPos);
}

void
ProxyAccessible::DeleteText(int32_t aStartPos, int32_t aEndPos)
{
  unused << mDoc->SendDeleteText(mID, aStartPos, aEndPos);
}

void
ProxyAccessible::PasteText(int32_t aPosition)
{
  unused << mDoc->SendPasteText(mID, aPosition);
}

nsIntPoint
ProxyAccessible::ImagePosition(uint32_t aCoordType)
{
  nsIntPoint retVal;
  unused << mDoc->SendImagePosition(mID, aCoordType, &retVal);
  return retVal;
}

nsIntSize
ProxyAccessible::ImageSize()
{
  nsIntSize retVal;
  unused << mDoc->SendImageSize(mID, &retVal);
  return retVal;
}

}
}
