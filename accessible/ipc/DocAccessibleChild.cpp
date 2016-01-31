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
#include "TextLeafAccessible.h"
#include "ImageAccessible.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "nsIPersistentProperties2.h"
#include "nsISimpleEnumerator.h"
#include "nsAccUtils.h"
#ifdef MOZ_ACCESSIBILITY_ATK
#include "AccessibleWrap.h"
#endif

namespace mozilla {
namespace a11y {

static uint32_t
InterfacesFor(Accessible* aAcc)
{
  uint32_t interfaces = 0;
  if (aAcc->IsHyperText() && aAcc->AsHyperText()->IsTextRole())
    interfaces |= Interfaces::HYPERTEXT;

  if (aAcc->IsLink())
    interfaces |= Interfaces::HYPERLINK;

  if (aAcc->HasNumericValue())
    interfaces |= Interfaces::VALUE;

  if (aAcc->IsImage())
    interfaces |= Interfaces::IMAGE;

  if (aAcc->IsTableCell())
    interfaces |= Interfaces::TABLECELL;

  if (aAcc->IsDoc())
    interfaces |= Interfaces::DOCUMENT;

  if (aAcc->IsSelect()) {
    interfaces |= Interfaces::SELECTION;
  }

  if (aAcc->ActionCount()) {
    interfaces |= Interfaces::ACTION;
  }

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
  MOZ_ASSERT(!aRoot->IsDoc(), "documents shouldn't be serialized");
  if (aRoot->IsOuterDoc())
    childCount = 0;

  aTree.AppendElement(AccessibleData(id, role, childCount, interfaces));
  for (uint32_t i = 0; i < childCount; i++)
    SerializeTree(aRoot->GetChildAt(i), aTree);
}

Accessible*
DocAccessibleChild::IdToAccessible(const uint64_t& aID) const
{
  if (!aID)
    return mDoc;

  if (!mDoc)
    return nullptr;

  return mDoc->GetAccessibleByUniqueID(reinterpret_cast<void*>(aID));
}

Accessible*
DocAccessibleChild::IdToAccessibleLink(const uint64_t& aID) const
{
  Accessible* acc = IdToAccessible(aID);
  return acc && acc->IsLink() ? acc : nullptr;
}

Accessible*
DocAccessibleChild::IdToAccessibleSelect(const uint64_t& aID) const
{
  Accessible* acc = IdToAccessible(aID);
  return acc && acc->IsSelect() ? acc : nullptr;
}

HyperTextAccessible*
DocAccessibleChild::IdToHyperTextAccessible(const uint64_t& aID) const
{
  Accessible* acc = IdToAccessible(aID);
  return acc && acc->IsHyperText() ? acc->AsHyperText() : nullptr;
}

TextLeafAccessible*
DocAccessibleChild::IdToTextLeafAccessible(const uint64_t& aID) const
{
  Accessible* acc = IdToAccessible(aID);
  return acc && acc->IsTextLeaf() ? acc->AsTextLeaf() : nullptr;
}

ImageAccessible*
DocAccessibleChild::IdToImageAccessible(const uint64_t& aID) const
{
  Accessible* acc = IdToAccessible(aID);
  return (acc && acc->IsImage()) ? acc->AsImage() : nullptr;
}

TableCellAccessible*
DocAccessibleChild::IdToTableCellAccessible(const uint64_t& aID) const
{
  Accessible* acc = IdToAccessible(aID);
  return (acc && acc->IsTableCell()) ? acc->AsTableCell() : nullptr;
}

TableAccessible*
DocAccessibleChild::IdToTableAccessible(const uint64_t& aID) const
{
  Accessible* acc = IdToAccessible(aID);
  return (acc && acc->IsTable()) ? acc->AsTable() : nullptr;
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
DocAccessibleChild::RecvNativeState(const uint64_t& aID, uint64_t* aState)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    *aState = states::DEFUNCT;
    return true;
  }

  *aState = acc->NativeState();

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
DocAccessibleChild::RecvHelp(const uint64_t& aID, nsString* aHelp)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return true;
  }

  acc->Help(*aHelp);
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
  Accessible* acc = IdToAccessible(aID);
  if (!acc)
    return true;

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
  Accessible* acc = IdToAccessible(aID);
  if (!acc)
    return true;

#define RELATIONTYPE(gecko, s, a, m, i) AddRelation(acc, RelationType::gecko, aRelations);

#include "RelationTypeMap.h"
#undef RELATIONTYPE

  return true;
}

bool
DocAccessibleChild::RecvIsSearchbox(const uint64_t& aID, bool* aRetVal)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc)
    return true;

  *aRetVal = acc->IsSearchbox();
  return true;
}

bool
DocAccessibleChild::RecvLandmarkRole(const uint64_t& aID, nsString* aLandmark)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return true;
  }

  if (nsIAtom* roleAtom = acc->LandmarkRole()) {
    roleAtom->ToString(*aLandmark);
  }

  return true;
}

bool
DocAccessibleChild::RecvARIARoleAtom(const uint64_t& aID, nsString* aRole)
{
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return true;
  }

  if (nsRoleMapEntry* roleMap = acc->ARIARoleMap()) {
    if (nsIAtom* roleAtom = *(roleMap->roleAtom)) {
      roleAtom->ToString(*aRole);
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvGetLevelInternal(const uint64_t& aID, int32_t* aLevel)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aLevel = acc->GetLevelInternal();
  }
  return true;
}

bool
DocAccessibleChild::RecvScrollTo(const uint64_t& aID,
                                 const uint32_t& aScrollType)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    nsCoreUtils::ScrollTo(acc->Document()->PresShell(), acc->GetContent(),
                          aScrollType);
  }

  return true;
}

bool
DocAccessibleChild::RecvScrollToPoint(const uint64_t& aID, const uint32_t& aScrollType, const int32_t& aX, const int32_t& aY)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->ScrollToPoint(aScrollType, aX, aY);
  }

  return true;
}

bool
DocAccessibleChild::RecvCaretLineNumber(const uint64_t& aID, int32_t* aLineNumber)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aLineNumber = acc && acc->IsTextRole() ? acc->CaretLineNumber() : 0;
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
                                       const int32_t& aOffset)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole() && acc->IsValidOffset(aOffset)) {
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
                                      nsString* aText, bool* aValid)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (!acc) {
    return true;
  }

  *aValid = acc->IsValidRange(aStartOffset, aEndOffset);
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
DocAccessibleChild::RecvText(const uint64_t& aID,
                             nsString* aText)
{
  TextLeafAccessible* acc = IdToTextLeafAccessible(aID);
  if (acc) {
    *aText = acc->Text();
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
                                   const int32_t& aPosition, bool* aValid)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aValid = acc->IsValidOffset(aPosition);
    acc->InsertText(aText, aPosition);
  }

  return true;
}

bool
DocAccessibleChild::RecvCopyText(const uint64_t& aID,
                                 const int32_t& aStartPos,
                                 const int32_t& aEndPos, bool* aValid)
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
                                const int32_t& aEndPos, bool* aValid)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aValid = acc->IsValidRange(aStartPos, aEndPos);
    acc->CutText(aStartPos, aEndPos);
  }

  return true;
}

bool
DocAccessibleChild::RecvDeleteText(const uint64_t& aID,
                                   const int32_t& aStartPos,
                                   const int32_t& aEndPos, bool* aValid)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aValid = acc->IsValidRange(aStartPos, aEndPos);
    acc->DeleteText(aStartPos, aEndPos);
  }

  return true;
}

bool
DocAccessibleChild::RecvPasteText(const uint64_t& aID,
                                  const int32_t& aPosition, bool* aValid)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aValid = acc->IsValidOffset(aPosition);
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

bool
DocAccessibleChild::RecvStartOffset(const uint64_t& aID,
                                    uint32_t* aRetVal,
                                    bool* aOk)
{
  Accessible* acc = IdToAccessibleLink(aID);
  if (acc) {
    *aRetVal = acc->StartOffset();
    *aOk = true;
  } else {
    *aRetVal = 0;
    *aOk = false;
  }

  return true;
}

bool
DocAccessibleChild::RecvEndOffset(const uint64_t& aID,
                                  uint32_t* aRetVal,
                                  bool* aOk)
{
  Accessible* acc = IdToAccessibleLink(aID);
  if (acc) {
    *aRetVal = acc->EndOffset();
    *aOk = true;
  } else {
    *aRetVal = 0;
    *aOk = false;
  }

  return true;
}

bool
DocAccessibleChild::RecvIsLinkValid(const uint64_t& aID,
                                    bool* aRetVal)
{
  Accessible* acc = IdToAccessibleLink(aID);
  if (acc) {
    *aRetVal = acc->IsLinkValid();
  } else {
    *aRetVal = false;
  }

  return true;
}

bool
DocAccessibleChild::RecvAnchorCount(const uint64_t& aID,
                                    uint32_t* aRetVal,
                                    bool* aOk)
{
  Accessible* acc = IdToAccessibleLink(aID);
  if (acc) {
    *aRetVal = acc->AnchorCount();
    *aOk = true;
  } else {
    *aRetVal = 0;
    *aOk = false;
  }

  return true;
}

bool
DocAccessibleChild::RecvAnchorURIAt(const uint64_t& aID,
                                    const uint32_t& aIndex,
                                    nsCString* aURI,
                                    bool* aOk)
{
  Accessible* acc = IdToAccessibleLink(aID);
  *aOk = false;
  if (acc) {
    nsCOMPtr<nsIURI> uri = acc->AnchorURIAt(aIndex);
    if (uri) {
      uri->GetSpec(*aURI);
      *aOk = true;
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvAnchorAt(const uint64_t& aID,
                                 const uint32_t& aIndex,
                                 uint64_t* aIDOfAnchor,
                                 bool* aOk)
{
  *aIDOfAnchor = 0;
  *aOk = false;
  Accessible* acc = IdToAccessibleLink(aID);
  if (acc) {
    Accessible* anchor = acc->AnchorAt(aIndex);
    if (anchor) {
      *aIDOfAnchor = reinterpret_cast<uint64_t>(anchor->UniqueID());
      *aOk = true;
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvLinkCount(const uint64_t& aID,
                                  uint32_t* aCount)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aCount = acc ? acc->LinkCount() : 0;
  return true;
}

bool
DocAccessibleChild::RecvLinkAt(const uint64_t& aID,
                               const uint32_t& aIndex,
                               uint64_t* aIDOfLink,
                               bool* aOk)
{
  *aIDOfLink = 0;
  *aOk = false;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    Accessible* link = acc->LinkAt(aIndex);
    if (link) {
      *aIDOfLink = reinterpret_cast<uint64_t>(link->UniqueID());
      *aOk = true;
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvLinkIndexOf(const uint64_t& aID,
                                    const uint64_t& aLinkID,
                                    int32_t* aIndex)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  Accessible* link = IdToAccessible(aLinkID);
  *aIndex = -1;
  if (acc && link) {
    *aIndex = acc->LinkIndexOf(link);
  }

  return true;
}

bool
DocAccessibleChild::RecvLinkIndexAtOffset(const uint64_t& aID,
                                          const uint32_t& aOffset,
                                          int32_t* aIndex)
{
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aIndex = acc ? acc->LinkIndexAtOffset(aOffset) : -1;
  return true;
}

bool
DocAccessibleChild::RecvTableOfACell(const uint64_t& aID,
                                     uint64_t* aTableID,
                                     bool* aOk)
{
  *aTableID = 0;
  *aOk = false;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    TableAccessible* table = acc->Table();
    if (table) {
      *aTableID = reinterpret_cast<uint64_t>(table->AsAccessible()->UniqueID());
      *aOk = true;
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvColIdx(const uint64_t& aID,
                               uint32_t* aIndex)
{
  *aIndex = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aIndex = acc->ColIdx();
  }

  return true;
}

bool
DocAccessibleChild::RecvRowIdx(const uint64_t& aID,
                               uint32_t* aIndex)
{
  *aIndex = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aIndex = acc->RowIdx();
  }

  return true;
}

bool
DocAccessibleChild::RecvColExtent(const uint64_t& aID,
                                  uint32_t* aExtent)
{
  *aExtent = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aExtent = acc->ColExtent();
  }

  return true;
}

bool
DocAccessibleChild::RecvRowExtent(const uint64_t& aID,
                                  uint32_t* aExtent)
{
  *aExtent = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aExtent = acc->RowExtent();
  }

  return true;
}

bool
DocAccessibleChild::RecvColHeaderCells(const uint64_t& aID,
                                       nsTArray<uint64_t>* aCells)
{
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    nsAutoTArray<Accessible*, 10> headerCells;
    acc->ColHeaderCells(&headerCells);
    aCells->SetCapacity(headerCells.Length());
    for (uint32_t i = 0; i < headerCells.Length(); ++i) {
      aCells->AppendElement(
        reinterpret_cast<uint64_t>(headerCells[i]->UniqueID()));
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvRowHeaderCells(const uint64_t& aID,
                                       nsTArray<uint64_t>* aCells)
{
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    nsAutoTArray<Accessible*, 10> headerCells;
    acc->RowHeaderCells(&headerCells);
    aCells->SetCapacity(headerCells.Length());
    for (uint32_t i = 0; i < headerCells.Length(); ++i) {
      aCells->AppendElement(
        reinterpret_cast<uint64_t>(headerCells[i]->UniqueID()));
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvIsCellSelected(const uint64_t& aID,
                                       bool* aSelected)
{
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  *aSelected = acc && acc->Selected();
  return true;
}

bool
DocAccessibleChild::RecvTableCaption(const uint64_t& aID,
                                     uint64_t* aCaptionID,
                                     bool* aOk)
{
  *aCaptionID = 0;
  *aOk = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    Accessible* caption = acc->Caption();
    if (caption) {
      *aCaptionID = reinterpret_cast<uint64_t>(caption->UniqueID());
      *aOk = true;
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSummary(const uint64_t& aID,
                                     nsString* aSummary)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->Summary(*aSummary);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableColumnCount(const uint64_t& aID,
                                         uint32_t* aColCount)
{
  *aColCount = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aColCount = acc->ColCount();
  }

  return true;
}

bool
DocAccessibleChild::RecvTableRowCount(const uint64_t& aID,
                                      uint32_t* aRowCount)
{
  *aRowCount = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aRowCount = acc->RowCount();
  }

  return true;
}

bool
DocAccessibleChild::RecvTableCellAt(const uint64_t& aID,
                                    const uint32_t& aRow,
                                    const uint32_t& aCol,
                                    uint64_t* aCellID,
                                    bool* aOk)
{
  *aCellID = 0;
  *aOk = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    Accessible* cell = acc->CellAt(aRow, aCol);
    if (cell) {
      *aCellID = reinterpret_cast<uint64_t>(cell->UniqueID());
      *aOk = true;
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvTableCellIndexAt(const uint64_t& aID,
                                         const uint32_t& aRow,
                                         const uint32_t& aCol,
                                         int32_t* aIndex)
{
  *aIndex = -1;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aIndex = acc->CellIndexAt(aRow, aCol);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableColumnIndexAt(const uint64_t& aID,
                                           const uint32_t& aCellIndex,
                                           int32_t* aCol)
{
  *aCol = -1;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aCol = acc->ColIndexAt(aCellIndex);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableRowIndexAt(const uint64_t& aID,
                                        const uint32_t& aCellIndex,
                                        int32_t* aRow)
{
  *aRow = -1;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aRow = acc->RowIndexAt(aCellIndex);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableRowAndColumnIndicesAt(const uint64_t& aID,
                                                  const uint32_t& aCellIndex,
                                                  int32_t* aRow,
                                                  int32_t* aCol)
{
  *aRow = -1;
  *aCol = -1;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->RowAndColIndicesAt(aCellIndex, aRow, aCol);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableColumnExtentAt(const uint64_t& aID,
                                            const uint32_t& aRow,
                                            const uint32_t& aCol,
                                            uint32_t* aExtent)
{
  *aExtent = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aExtent = acc->ColExtentAt(aRow, aCol);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableRowExtentAt(const uint64_t& aID,
                                         const uint32_t& aRow,
                                         const uint32_t& aCol,
                                         uint32_t* aExtent)
{
  *aExtent = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aExtent = acc->RowExtentAt(aRow, aCol);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableColumnDescription(const uint64_t& aID,
                                               const uint32_t& aCol,
                                               nsString* aDescription)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->ColDescription(aCol, *aDescription);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableRowDescription(const uint64_t& aID,
                                            const uint32_t& aRow,
                                            nsString* aDescription)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->RowDescription(aRow, *aDescription);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableColumnSelected(const uint64_t& aID,
                                            const uint32_t& aCol,
                                            bool* aSelected)
{
  *aSelected = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelected = acc->IsColSelected(aCol);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableRowSelected(const uint64_t& aID,
                                         const uint32_t& aRow,
                                         bool* aSelected)
{
  *aSelected = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelected = acc->IsRowSelected(aRow);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableCellSelected(const uint64_t& aID,
                                          const uint32_t& aRow,
                                          const uint32_t& aCol,
                                          bool* aSelected)
{
  *aSelected = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelected = acc->IsCellSelected(aRow, aCol);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSelectedCellCount(const uint64_t& aID,
                                               uint32_t* aSelectedCells)
{
  *aSelectedCells = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelectedCells = acc->SelectedCellCount();
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSelectedColumnCount(const uint64_t& aID,
                                                 uint32_t* aSelectedColumns)
{
  *aSelectedColumns = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelectedColumns = acc->SelectedColCount();
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSelectedRowCount(const uint64_t& aID,
                                              uint32_t* aSelectedRows)
{
  *aSelectedRows = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelectedRows = acc->SelectedRowCount();
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSelectedCells(const uint64_t& aID,
                                           nsTArray<uint64_t>* aCellIDs)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    nsAutoTArray<Accessible*, 30> cells;
    acc->SelectedCells(&cells);
    aCellIDs->SetCapacity(cells.Length());
    for (uint32_t i = 0; i < cells.Length(); ++i) {
      aCellIDs->AppendElement(
        reinterpret_cast<uint64_t>(cells[i]->UniqueID()));
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSelectedCellIndices(const uint64_t& aID,
                                                 nsTArray<uint32_t>* aCellIndices)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectedCellIndices(aCellIndices);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSelectedColumnIndices(const uint64_t& aID,
                                                   nsTArray<uint32_t>* aColumnIndices)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectedColIndices(aColumnIndices);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSelectedRowIndices(const uint64_t& aID,
                                                nsTArray<uint32_t>* aRowIndices)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectedRowIndices(aRowIndices);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSelectColumn(const uint64_t& aID,
                                          const uint32_t& aCol)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectCol(aCol);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableSelectRow(const uint64_t& aID,
                                       const uint32_t& aRow)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectRow(aRow);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableUnselectColumn(const uint64_t& aID,
                                            const uint32_t& aCol)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->UnselectCol(aCol);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableUnselectRow(const uint64_t& aID,
                                         const uint32_t& aRow)
{
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->UnselectRow(aRow);
  }

  return true;
}

bool
DocAccessibleChild::RecvTableIsProbablyForLayout(const uint64_t& aID,
                                                 bool* aForLayout)
{
  *aForLayout = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aForLayout = acc->IsProbablyLayoutTable();
  }

  return true;
}

bool
DocAccessibleChild::RecvAtkTableColumnHeader(const uint64_t& aID,
                                             const int32_t& aCol,
                                             uint64_t* aHeader,
                                             bool* aOk)
{
  *aHeader = 0;
  *aOk = false;

#ifdef MOZ_ACCESSIBILITY_ATK
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    Accessible* header = AccessibleWrap::GetColumnHeader(acc, aCol);
    if (header) {
      *aHeader = reinterpret_cast<uint64_t>(header->UniqueID());
      *aOk = true;
    }
  }
#endif

  return true;
}

bool
DocAccessibleChild::RecvAtkTableRowHeader(const uint64_t& aID,
                                          const int32_t& aRow,
                                          uint64_t* aHeader,
                                          bool* aOk)
{
  *aHeader = 0;
  *aOk = false;

#ifdef MOZ_ACCESSIBILITY_ATK
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    Accessible* header = AccessibleWrap::GetRowHeader(acc, aRow);
    if (header) {
      *aHeader = reinterpret_cast<uint64_t>(header->UniqueID());
      *aOk = true;
    }
  }
#endif

  return true;
}

bool
DocAccessibleChild::RecvSelectedItems(const uint64_t& aID,
                                      nsTArray<uint64_t>* aSelectedItemIDs)
{
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    nsAutoTArray<Accessible*, 10> selectedItems;
    acc->SelectedItems(&selectedItems);
    aSelectedItemIDs->SetCapacity(selectedItems.Length());
    for (size_t i = 0; i < selectedItems.Length(); ++i) {
      aSelectedItemIDs->AppendElement(
        reinterpret_cast<uint64_t>(selectedItems[i]->UniqueID()));
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvSelectedItemCount(const uint64_t& aID,
                                          uint32_t* aCount)
{
  *aCount = 0;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aCount = acc->SelectedItemCount();
  }

  return true;
}

bool
DocAccessibleChild::RecvGetSelectedItem(const uint64_t& aID,
                                        const uint32_t& aIndex,
                                        uint64_t* aSelected,
                                        bool* aOk)
{
  *aSelected = 0;
  *aOk = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    Accessible* item = acc->GetSelectedItem(aIndex);
    if (item) {
      *aSelected = reinterpret_cast<uint64_t>(item->UniqueID());
      *aOk = true;
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvIsItemSelected(const uint64_t& aID,
                                       const uint32_t& aIndex,
                                       bool* aSelected)
{
  *aSelected = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSelected = acc->IsItemSelected(aIndex);
  }

  return true;
}

bool
DocAccessibleChild::RecvAddItemToSelection(const uint64_t& aID,
                                           const uint32_t& aIndex,
                                           bool* aSuccess)
{
  *aSuccess = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSuccess = acc->AddItemToSelection(aIndex);
  }

  return true;
}

bool
DocAccessibleChild::RecvRemoveItemFromSelection(const uint64_t& aID,
                                                const uint32_t& aIndex,
                                                bool* aSuccess)
{
  *aSuccess = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSuccess = acc->RemoveItemFromSelection(aIndex);
  }

  return true;
}

bool
DocAccessibleChild::RecvSelectAll(const uint64_t& aID,
                                  bool* aSuccess)
{
  *aSuccess = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSuccess = acc->SelectAll();
  }

  return true;
}

bool
DocAccessibleChild::RecvUnselectAll(const uint64_t& aID,
                                    bool* aSuccess)
{
  *aSuccess = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSuccess = acc->UnselectAll();
  }

  return true;
}

bool
DocAccessibleChild::RecvTakeSelection(const uint64_t& aID)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->TakeSelection();
  }

  return true;
}

bool
DocAccessibleChild::RecvSetSelected(const uint64_t& aID, const bool& aSelect)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->SetSelected(aSelect);
  }

  return true;
}

bool
DocAccessibleChild::RecvDoAction(const uint64_t& aID,
                                 const uint8_t& aIndex,
                                 bool* aSuccess)
{
  *aSuccess = false;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aSuccess = acc->DoAction(aIndex);
  }

  return true;
}

bool
DocAccessibleChild::RecvActionCount(const uint64_t& aID,
                                    uint8_t* aCount)
{
  *aCount = 0;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aCount = acc->ActionCount();
  }

  return true;
}

bool
DocAccessibleChild::RecvActionDescriptionAt(const uint64_t& aID,
                                            const uint8_t& aIndex,
                                            nsString* aDescription)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->ActionDescriptionAt(aIndex, *aDescription);
  }

  return true;
}

bool
DocAccessibleChild::RecvActionNameAt(const uint64_t& aID,
                                     const uint8_t& aIndex,
                                     nsString* aName)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->ActionNameAt(aIndex, *aName);
  }

  return true;
}

bool
DocAccessibleChild::RecvAccessKey(const uint64_t& aID,
                                  uint32_t* aKey,
                                  uint32_t* aModifierMask)
{
  *aKey = 0;
  *aModifierMask = 0;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    KeyBinding kb = acc->AccessKey();
    *aKey = kb.Key();
    *aModifierMask = kb.ModifierMask();
  }

  return true;
}

bool
DocAccessibleChild::RecvKeyboardShortcut(const uint64_t& aID,
                                         uint32_t* aKey,
                                         uint32_t* aModifierMask)
{
  *aKey = 0;
  *aModifierMask = 0;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    KeyBinding kb = acc->KeyboardShortcut();
    *aKey = kb.Key();
    *aModifierMask = kb.ModifierMask();
  }

  return true;
}

bool
DocAccessibleChild::RecvAtkKeyBinding(const uint64_t& aID,
                                      nsString* aResult)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    AccessibleWrap::GetKeyBinding(acc, *aResult);
  }
#endif
  return true;
}

bool
DocAccessibleChild::RecvCurValue(const uint64_t& aID,
                                 double* aValue)
{
  *aValue = UnspecifiedNaN<double>();
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aValue = acc->CurValue();
  }

  return true;
}

bool
DocAccessibleChild::RecvSetCurValue(const uint64_t& aID,
                                    const double& aValue,
                                    bool* aRetVal)
{
  *aRetVal = false;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aRetVal = acc->SetCurValue(aValue);
  }

  return true;
}

bool
DocAccessibleChild::RecvMinValue(const uint64_t& aID,
                                 double* aValue)
{
  *aValue = UnspecifiedNaN<double>();
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aValue = acc->MinValue();
  }

  return true;
}

bool
DocAccessibleChild::RecvMaxValue(const uint64_t& aID,
                                 double* aValue)
{
  *aValue = UnspecifiedNaN<double>();
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aValue = acc->MaxValue();
  }

  return true;
}

bool
DocAccessibleChild::RecvStep(const uint64_t& aID,
                             double* aStep)
{
  *aStep = UnspecifiedNaN<double>();
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aStep = acc->Step();
  }

  return true;
}

bool
DocAccessibleChild::RecvTakeFocus(const uint64_t& aID)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->TakeFocus();
  }

  return true;
}

bool
DocAccessibleChild::RecvEmbeddedChildCount(const uint64_t& aID,
                                           uint32_t* aCount)
{
  *aCount = 0;

  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return true;
  }

  *aCount = acc->EmbeddedChildCount();

  return true;
}

bool
DocAccessibleChild::RecvIndexOfEmbeddedChild(const uint64_t& aID,
                                             const uint64_t& aChildID,
                                             uint32_t* aChildIdx)
{
  *aChildIdx = 0;

  Accessible* parent = IdToAccessible(aID);
  Accessible* child = IdToAccessible(aChildID);
  if (!parent || !child)
    return true;

  *aChildIdx = parent->GetIndexOfEmbeddedChild(child);
  return true;
}

bool
DocAccessibleChild::RecvEmbeddedChildAt(const uint64_t& aID,
                                        const uint32_t& aIdx,
                                        uint64_t* aChildID)
{
  *aChildID = 0;

  Accessible* acc = IdToAccessible(aID);
  if (!acc)
    return true;

  *aChildID = reinterpret_cast<uintptr_t>(acc->GetEmbeddedChildAt(aIdx));
  return true;
}

bool
DocAccessibleChild::RecvFocusedChild(const uint64_t& aID,
                                       uint64_t* aChild,
                                       bool* aOk)
{
  *aChild = 0;
  *aOk = false;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    Accessible* child = acc->FocusedChild();
    if (child) {
      *aChild = reinterpret_cast<uint64_t>(child->UniqueID());
      *aOk = true;
    }
  }
  return true;
}

bool
DocAccessibleChild::RecvLanguage(const uint64_t& aID,
                                 nsString* aLocale)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->Language(*aLocale);
  }

  return true;
}

bool
DocAccessibleChild::RecvDocType(const uint64_t& aID,
                                nsString* aType)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    acc->AsDoc()->DocType(*aType);
  }

  return true;
}

bool
DocAccessibleChild::RecvTitle(const uint64_t& aID,
                            nsString* aTitle)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    mozilla::ErrorResult rv;
    acc->GetContent()->GetTextContent(*aTitle, rv);
  }

  return true;
}

bool
DocAccessibleChild::RecvURL(const uint64_t& aID,
                            nsString* aURL)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    acc->AsDoc()->URL(*aURL);
  }

  return true;
}

bool
DocAccessibleChild::RecvMimeType(const uint64_t& aID,
                                 nsString* aMime)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    acc->AsDoc()->MimeType(*aMime);
  }

  return true;
}

bool
DocAccessibleChild::RecvURLDocTypeMimeType(const uint64_t& aID,
                                           nsString* aURL,
                                           nsString* aDocType,
                                           nsString* aMimeType)
{
  Accessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    DocAccessible* doc = acc->AsDoc();
    doc->URL(*aURL);
    doc->DocType(*aDocType);
    doc->MimeType(*aMimeType);
  }

  return true;
}

bool
DocAccessibleChild::RecvAccessibleAtPoint(const uint64_t& aID,
                                          const int32_t& aX,
                                          const int32_t& aY,
                                          const bool& aNeedsScreenCoords,
                                          const uint32_t& aWhich,
                                          uint64_t* aResult,
                                          bool* aOk)
{
  *aResult = 0;
  *aOk = false;
  Accessible* acc = IdToAccessible(aID);
  if (acc && !acc->IsDefunct() && !nsAccUtils::MustPrune(acc)) {
    int32_t x = aX;
    int32_t y = aY;
    if (aNeedsScreenCoords) {
      nsIntPoint winCoords =
        nsCoreUtils::GetScreenCoordsForWindow(acc->GetNode());
      x += winCoords.x;
      y += winCoords.y;
    }

    Accessible* result =
      acc->ChildAtPoint(x, y,
                        static_cast<Accessible::EWhichChildAtPoint>(aWhich));
    if (result) {
      *aResult = reinterpret_cast<uint64_t>(result->UniqueID());
      *aOk = true;
    }
  }

  return true;
}

bool
DocAccessibleChild::RecvExtents(const uint64_t& aID,
                                const bool& aNeedsScreenCoords,
                                int32_t* aX,
                                int32_t* aY,
                                int32_t* aWidth,
                                int32_t* aHeight)
{
  *aX = 0;
  *aY = 0;
  *aWidth = 0;
  *aHeight = 0;
  Accessible* acc = IdToAccessible(aID);
  if (acc && !acc->IsDefunct()) {
    nsIntRect screenRect = acc->Bounds();
    if (!screenRect.IsEmpty()) {
      if (aNeedsScreenCoords) {
        nsIntPoint winCoords =
          nsCoreUtils::GetScreenCoordsForWindow(acc->GetNode());
        screenRect.x -= winCoords.x;
        screenRect.y -= winCoords.y;
      }

      *aX = screenRect.x;
      *aY = screenRect.y;
      *aWidth = screenRect.width;
      *aHeight = screenRect.height;
    }
  }
  return true;
}

}
}
