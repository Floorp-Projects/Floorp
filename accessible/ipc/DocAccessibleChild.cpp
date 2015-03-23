/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleChild.h"

#include "Accessible-inl.h"
#include "ProxyAccessible.h"
#include "Relation.h"
#include "HyperTextAccessible-inl.h"
#include "ImageAccessible.h"
#include "nsIPersistentProperties2.h"
#include "nsISimpleEnumerator.h"

namespace mozilla {
namespace a11y {

static uint32_t
InterfacesFor(Accessible* aAcc)
{
  uint32_t interfaces = 0;
  if (aAcc->IsHyperText() && aAcc->AsHyperText()->IsTextRole())
    interfaces |= Interfaces::HYPERTEXT;

  return interfaces;
}

static void
SerializeTree(Accessible* aRoot, nsTArray<AccessibleData>& aTree)
{
  uint64_t id = reinterpret_cast<uint64_t>(aRoot->UniqueID());
  uint32_t role = aRoot->Role();
  uint32_t childCount = aRoot->ChildCount();
  uint32_t interfaces = InterfacesFor(aRoot);

  // OuterDocAccessibles are special because we don't want to serialize the
  // child doc here, we'll call PDocAccessibleConstructor in
  // NotificationController.
  if (childCount == 1 && aRoot->GetChildAt(0)->IsDoc())
    childCount = 0;

  aTree.AppendElement(AccessibleData(id, role, childCount, interfaces));
  for (uint32_t i = 0; i < childCount; i++)
    SerializeTree(aRoot->GetChildAt(i), aTree);
}

Accessible*
DocAccessibleChild::IdToAccessible(const uint64_t& aID) const
{
  return mDoc->GetAccessibleByUniqueID(reinterpret_cast<void*>(aID));
}

HyperTextAccessible*
DocAccessibleChild::IdToHyperTextAccessible(const uint64_t& aID) const
{
  Accessible* acc = IdToAccessible(aID);
  MOZ_ASSERT(!acc || acc->IsHyperText());
  return acc ? acc->AsHyperText() : nullptr;
}

ImageAccessible*
DocAccessibleChild::IdToImageAccessible(const uint64_t& aID) const
{
  Accessible* acc = IdToAccessible(aID);
  return (acc && acc->IsImage()) ? acc->AsImage() : nullptr;
}

void
DocAccessibleChild::ShowEvent(AccShowEvent* aShowEvent)
{
  Accessible* parent = aShowEvent->Parent();
  uint64_t parentID = parent->IsDoc() ? 0 : reinterpret_cast<uint64_t>(parent->UniqueID());
  uint32_t idxInParent = aShowEvent->GetAccessible()->IndexInParent();
  nsTArray<AccessibleData> shownTree;
  ShowEventData data(parentID, idxInParent, shownTree);
  SerializeTree(aShowEvent->GetAccessible(), data.NewTree());
  SendShowEvent(data);
}

bool
DocAccessibleChild::RecvState(const uint64_t& aID, uint64_t* aState)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    *aState = states::DEFUNCT;
    return true;
  }

  *aState = acc->State();

  return true;
}

bool
DocAccessibleChild::RecvName(const uint64_t& aID, nsString* aName)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc)
    return true;

  acc->Name(*aName);
  return true;
}

bool
DocAccessibleChild::RecvValue(const uint64_t& aID, nsString* aValue)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return true;
  }

  acc->Value(*aValue);
  return true;
}

bool
DocAccessibleChild::RecvDescription(const uint64_t& aID, nsString* aDesc)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc)
    return true;

  acc->Description(*aDesc);
  return true;
}

bool
DocAccessibleChild::RecvAttributes(const uint64_t& aID, nsTArray<Attribute>* aAttributes)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc)
    return true;

  nsCOMPtr<nsIPersistentProperties> props = acc->Attributes();
  return PersistentPropertiesToArray(props, aAttributes);
}

bool
DocAccessibleChild::PersistentPropertiesToArray(nsIPersistentProperties* aProps,
                                                nsTArray<Attribute>* aAttributes)
{
  if (!aProps) {
    return true;
  }
  nsCOMPtr<nsISimpleEnumerator> propEnum;
  nsresult rv = aProps->Enumerate(getter_AddRefs(propEnum));
  NS_ENSURE_SUCCESS(rv, false);

  bool hasMore;
  while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> sup;
    rv = propEnum->GetNext(getter_AddRefs(sup));
    NS_ENSURE_SUCCESS(rv, false);

    nsCOMPtr<nsIPropertyElement> propElem(do_QueryInterface(sup));
    NS_ENSURE_TRUE(propElem, false);

    nsAutoCString name;
    rv = propElem->GetKey(name);
    NS_ENSURE_SUCCESS(rv, false);

    nsAutoString value;
    rv = propElem->GetValue(value);
    NS_ENSURE_SUCCESS(rv, false);

    aAttributes->AppendElement(Attribute(name, value));
    }

  return true;
}

bool
DocAccessibleChild::RecvRelationByType(const uint64_t& aID,
                                       const uint32_t& aType,
                                       nsTArray<uint64_t>* aTargets)
{
  Accessible* acc = mDoc->GetAccessibleByUniqueID((void*)aID);
  if (!acc)
    return false;

  auto type = static_cast<RelationType>(aType);
  Relation rel = acc->RelationByType(type);
  while (Accessible* target = rel.Next())
    aTargets->AppendElement(reinterpret_cast<uintptr_t>(target));

  return true;
}

static void
AddRelation(Accessible* aAcc, RelationType aType,
            nsTArray<RelationTargets>* aTargets)
{
  Relation rel = aAcc->RelationByType(aType);
  nsTArray<uint64_t> targets;
  while (Accessible* target = rel.Next())
    targets.AppendElement(reinterpret_cast<uintptr_t>(target));

  if (!targets.IsEmpty()) {
    RelationTargets* newRelation =
      aTargets->AppendElement(RelationTargets(static_cast<uint32_t>(aType),
                                              nsTArray<uint64_t>()));
    newRelation->Targets().SwapElements(targets);
  }
}

bool
DocAccessibleChild::RecvRelations(const uint64_t& aID,
                                  nsTArray<RelationTargets>* aRelations)
{
  Accessible* acc = mDoc->GetAccessibleByUniqueID((void*)aID);
  if (!aID)
    return false;

#define RELATIONTYPE(gecko, s, a, m, i) AddRelation(acc, RelationType::gecko, aRelations);

#include "RelationTypeMap.h"
#undef RELATIONTYPE

  return true;
}

bool
DocAccessibleChild::RecvCaretOffset(const uint64_t& aID, int32_t* aOffset)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aOffset = acc && acc->IsTextRole() ? acc->CaretOffset() : 0;
  return true;
}

bool
DocAccessibleChild::RecvSetCaretOffset(const uint64_t& aID,
                                       const int32_t& aOffset,
                                       bool* aRetVal)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aRetVal = false;
  if (acc && acc->IsTextRole() && acc->IsValidOffset(aOffset)) {
    *aRetVal = true;
    acc->SetCaretOffset(aOffset);
  }
  return true;
}

bool
DocAccessibleChild::RecvCharacterCount(const uint64_t& aID, int32_t* aCount)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aCount = acc ? acc->CharacterCount() : 0;
  return true;
}

bool
DocAccessibleChild::RecvSelectionCount(const uint64_t& aID, int32_t* aCount)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aCount = acc ? acc->SelectionCount() : 0;
  return true;
}

bool
DocAccessibleChild::RecvTextSubstring(const uint64_t& aID,
                                      const int32_t& aStartOffset,
                                      const int32_t& aEndOffset,
                                      nsString* aText)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (!acc) {
    return true;
  }

  acc->TextSubstring(aStartOffset, aEndOffset, *aText);
  return true;
}

bool
DocAccessibleChild::RecvGetTextAfterOffset(const uint64_t& aID,
                                           const int32_t& aOffset,
                                           const int32_t& aBoundaryType,
                                           nsString* aText,
                                           int32_t* aStartOffset,
                                           int32_t* aEndOffset)
{
  *aStartOffset = 0;
  *aEndOffset = 0;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->TextAfterOffset(aOffset, aBoundaryType,
                         aStartOffset, aEndOffset, *aText);
  }
  return true;
}

bool
DocAccessibleChild::RecvGetTextAtOffset(const uint64_t& aID,
                                        const int32_t& aOffset,
                                        const int32_t& aBoundaryType,
                                        nsString* aText,
                                        int32_t* aStartOffset,
                                        int32_t* aEndOffset)
{
  *aStartOffset = 0;
  *aEndOffset = 0;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->TextAtOffset(aOffset, aBoundaryType,
                      aStartOffset, aEndOffset, *aText);
  }
  return true;
}

bool
DocAccessibleChild::RecvGetTextBeforeOffset(const uint64_t& aID,
                                            const int32_t& aOffset,
                                            const int32_t& aBoundaryType,
                                            nsString* aText,
                                            int32_t* aStartOffset,
                                            int32_t* aEndOffset)
{
  *aStartOffset = 0;
  *aEndOffset = 0;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->TextBeforeOffset(aOffset, aBoundaryType,
                          aStartOffset, aEndOffset, *aText);
  }
  return true;
}

bool
DocAccessibleChild::RecvCharAt(const uint64_t& aID,
                               const int32_t& aOffset,
                               uint16_t* aChar)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aChar = acc && acc->IsTextRole() ?
    static_cast<uint16_t>(acc->CharAt(aOffset)) : 0;
  return true;
}

bool
DocAccessibleChild::RecvTextAttributes(const uint64_t& aID,
                                       const bool& aIncludeDefAttrs,
                                       const int32_t& aOffset,
                                       nsTArray<Attribute>* aAttributes,
                                       int32_t* aStartOffset,
                                       int32_t* aEndOffset)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (!acc || !acc->IsTextRole()) {
    return true;
  }

  nsCOMPtr<nsIPersistentProperties> props =
    acc->TextAttributes(aIncludeDefAttrs, aOffset, aStartOffset, aEndOffset);
  return PersistentPropertiesToArray(props, aAttributes);
}

bool
DocAccessibleChild::RecvDefaultTextAttributes(const uint64_t& aID,
                                              nsTArray<Attribute> *aAttributes)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (!acc || !acc->IsTextRole()) {
    return true;
  }

  nsCOMPtr<nsIPersistentProperties> props = acc->DefaultTextAttributes();
  return PersistentPropertiesToArray(props, aAttributes);
}

bool
DocAccessibleChild::RecvTextBounds(const uint64_t& aID,
                                   const int32_t& aStartOffset,
                                   const int32_t& aEndOffset,
                                   const uint32_t& aCoordType,
                                   nsIntRect* aRetVal)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aRetVal = acc->TextBounds(aStartOffset, aEndOffset, aCoordType);
  }

  return true;
}

bool
DocAccessibleChild::RecvCharBounds(const uint64_t& aID,
                                   const int32_t& aOffset,
                                   const uint32_t& aCoordType,
                                   nsIntRect* aRetVal)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aRetVal = acc->CharBounds(aOffset, aCoordType);
  }

  return true;
}

bool
DocAccessibleChild::RecvOffsetAtPoint(const uint64_t& aID,
                                      const int32_t& aX,
                                      const int32_t& aY,
                                      const uint32_t& aCoordType,
                                      int32_t* aRetVal)
{
  *aRetVal = -1;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aRetVal = acc->OffsetAtPoint(aX, aY, aCoordType);
  }
  return true;
}

bool
DocAccessibleChild::RecvSelectionBoundsAt(const uint64_t& aID,
                                          const int32_t& aSelectionNum,
                                          bool* aSucceeded,
                                          nsString* aData,
                                          int32_t* aStartOffset,
                                          int32_t* aEndOffset)
{
  *aSucceeded = false;
  *aStartOffset = 0;
  *aEndOffset = 0;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aSucceeded =
      acc->SelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset);
    if (*aSucceeded) {
      acc->TextSubstring(*aStartOffset, *aEndOffset, *aData);
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvSetSelectionBoundsAt(const uint64_t& aID,
                                             const int32_t& aSelectionNum,
                                             const int32_t& aStartOffset,
                                             const int32_t& aEndOffset,
                                             bool* aSucceeded)
{
  *aSucceeded = false;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aSucceeded =
      acc->SetSelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset);
  }

  return true;
}

bool
DocAccessibleChild::RecvAddToSelection(const uint64_t& aID,
                                       const int32_t& aStartOffset,
                                       const int32_t& aEndOffset,
                                       bool* aSucceeded)
{
  *aSucceeded = false;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aSucceeded = acc->AddToSelection(aStartOffset, aEndOffset);
  }

  return true;
}

bool
DocAccessibleChild::RecvRemoveFromSelection(const uint64_t& aID,
                                            const int32_t& aSelectionNum,
                                            bool* aSucceeded)
{
  *aSucceeded = false;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aSucceeded = acc->RemoveFromSelection(aSelectionNum);
  }

  return true;
}

bool
DocAccessibleChild::RecvScrollSubstringTo(const uint64_t& aID,
                                          const int32_t& aStartOffset,
                                          const int32_t& aEndOffset,
                                          const uint32_t& aScrollType)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->ScrollSubstringTo(aStartOffset, aEndOffset, aScrollType);
  }

  return true;
}

bool
DocAccessibleChild::RecvScrollSubstringToPoint(const uint64_t& aID,
                                               const int32_t& aStartOffset,
                                               const int32_t& aEndOffset,
                                               const uint32_t& aCoordinateType,
                                               const int32_t& aX,
                                               const int32_t& aY)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->ScrollSubstringToPoint(aStartOffset, aEndOffset, aCoordinateType,
                                aX, aY);
  }

  return true;
}


bool
DocAccessibleChild::RecvReplaceText(const uint64_t& aID,
                                    const nsString& aText)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    acc->ReplaceText(aText);
  }

  return true;
}

bool
DocAccessibleChild::RecvInsertText(const uint64_t& aID,
                                   const nsString& aText,
                                   const int32_t& aPosition)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    acc->InsertText(aText, aPosition);
  }

  return true;
}

bool
DocAccessibleChild::RecvCopyText(const uint64_t& aID,
                                 const int32_t& aStartPos,
                                 const int32_t& aEndPos)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    acc->CopyText(aStartPos, aEndPos);
  }

  return true;
}

bool
DocAccessibleChild::RecvCutText(const uint64_t& aID,
                                const int32_t& aStartPos,
                                const int32_t& aEndPos)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    acc->CutText(aStartPos, aEndPos);
  }

  return true;
}

bool
DocAccessibleChild::RecvDeleteText(const uint64_t& aID,
                                   const int32_t& aStartPos,
                                   const int32_t& aEndPos)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    acc->DeleteText(aStartPos, aEndPos);
  }

  return true;
}

bool
DocAccessibleChild::RecvPasteText(const uint64_t& aID,
                                  const int32_t& aPosition)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    acc->PasteText(aPosition);
  }

  return true;
}

bool
DocAccessibleChild::RecvImagePosition(const uint64_t& aID,
                                      const uint32_t& aCoordType,
                                      nsIntPoint* aRetVal)
{
  ImageAccessible* acc = IdToImageAccessible(aID);
  if (acc) {
    *aRetVal = acc->Position(aCoordType);
  }

  return true;
}

bool
DocAccessibleChild::RecvImageSize(const uint64_t& aID,
                                  nsIntSize* aRetVal)
{

  ImageAccessible* acc = IdToImageAccessible(aID);
  if (acc) {
    *aRetVal = acc->Size();
  }

  return true;
}

}
}
