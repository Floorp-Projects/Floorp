/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleChild.h"

#include "nsAccessibilityService.h"
#include "Accessible-inl.h"
#include "ProxyAccessible.h"
#include "Relation.h"
#include "HyperTextAccessible-inl.h"
#include "TextLeafAccessible.h"
#include "ImageAccessible.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "nsIPersistentProperties2.h"
#include "nsAccUtils.h"
#ifdef MOZ_ACCESSIBILITY_ATK
#  include "AccessibleWrap.h"
#endif
#include "mozilla/PresShell.h"
#include "mozilla/a11y/DocAccessiblePlatformExtChild.h"

namespace mozilla {
namespace a11y {

Accessible* DocAccessibleChild::IdToAccessible(const uint64_t& aID) const {
  if (!aID) return mDoc;

  if (!mDoc) return nullptr;

  return mDoc->GetAccessibleByUniqueID(reinterpret_cast<void*>(aID));
}

Accessible* DocAccessibleChild::IdToAccessibleLink(const uint64_t& aID) const {
  Accessible* acc = IdToAccessible(aID);
  return acc && acc->IsLink() ? acc : nullptr;
}

Accessible* DocAccessibleChild::IdToAccessibleSelect(
    const uint64_t& aID) const {
  Accessible* acc = IdToAccessible(aID);
  return acc && acc->IsSelect() ? acc : nullptr;
}

HyperTextAccessible* DocAccessibleChild::IdToHyperTextAccessible(
    const uint64_t& aID) const {
  Accessible* acc = IdToAccessible(aID);
  return acc && acc->IsHyperText() ? acc->AsHyperText() : nullptr;
}

TextLeafAccessible* DocAccessibleChild::IdToTextLeafAccessible(
    const uint64_t& aID) const {
  Accessible* acc = IdToAccessible(aID);
  return acc && acc->IsTextLeaf() ? acc->AsTextLeaf() : nullptr;
}

ImageAccessible* DocAccessibleChild::IdToImageAccessible(
    const uint64_t& aID) const {
  Accessible* acc = IdToAccessible(aID);
  return (acc && acc->IsImage()) ? acc->AsImage() : nullptr;
}

TableCellAccessible* DocAccessibleChild::IdToTableCellAccessible(
    const uint64_t& aID) const {
  Accessible* acc = IdToAccessible(aID);
  return (acc && acc->IsTableCell()) ? acc->AsTableCell() : nullptr;
}

TableAccessible* DocAccessibleChild::IdToTableAccessible(
    const uint64_t& aID) const {
  Accessible* acc = IdToAccessible(aID);
  return (acc && acc->IsTable()) ? acc->AsTable() : nullptr;
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvState(const uint64_t& aID,
                                                      uint64_t* aState) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    *aState = states::DEFUNCT;
    return IPC_OK();
  }

  *aState = acc->State();

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvNativeState(const uint64_t& aID,
                                                            uint64_t* aState) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    *aState = states::DEFUNCT;
    return IPC_OK();
  }

  *aState = acc->NativeState();

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvName(const uint64_t& aID,
                                                     nsString* aName,
                                                     uint32_t* aFlag) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) return IPC_OK();

  *aFlag = acc->Name(*aName);
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvValue(const uint64_t& aID,
                                                      nsString* aValue) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return IPC_OK();
  }

  acc->Value(*aValue);
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvHelp(const uint64_t& aID,
                                                     nsString* aHelp) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return IPC_OK();
  }

  acc->Help(*aHelp);
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvDescription(const uint64_t& aID,
                                                            nsString* aDesc) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) return IPC_OK();

  acc->Description(*aDesc);
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAttributes(
    const uint64_t& aID, nsTArray<Attribute>* aAttributes) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) return IPC_OK();

  nsCOMPtr<nsIPersistentProperties> props = acc->Attributes();
  if (!nsAccUtils::PersistentPropertiesToArray(props, aAttributes)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvRelationByType(
    const uint64_t& aID, const uint32_t& aType, nsTArray<uint64_t>* aTargets) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) return IPC_OK();

  auto type = static_cast<RelationType>(aType);
  Relation rel = acc->RelationByType(type);
  while (Accessible* target = rel.Next())
    aTargets->AppendElement(reinterpret_cast<uint64_t>(target->UniqueID()));

  return IPC_OK();
}

static void AddRelation(Accessible* aAcc, RelationType aType,
                        nsTArray<RelationTargets>* aTargets) {
  Relation rel = aAcc->RelationByType(aType);
  nsTArray<uint64_t> targets;
  while (Accessible* target = rel.Next())
    targets.AppendElement(reinterpret_cast<uint64_t>(target->UniqueID()));

  if (!targets.IsEmpty()) {
    RelationTargets* newRelation = aTargets->AppendElement(
        RelationTargets(static_cast<uint32_t>(aType), nsTArray<uint64_t>()));
    newRelation->Targets().SwapElements(targets);
  }
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvRelations(
    const uint64_t& aID, nsTArray<RelationTargets>* aRelations) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) return IPC_OK();

#define RELATIONTYPE(gecko, s, a, m, i) \
  AddRelation(acc, RelationType::gecko, aRelations);

#include "RelationTypeMap.h"
#undef RELATIONTYPE

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvIsSearchbox(const uint64_t& aID,
                                                            bool* aRetVal) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) return IPC_OK();

  *aRetVal = acc->IsSearchbox();
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvLandmarkRole(
    const uint64_t& aID, nsString* aLandmark) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return IPC_OK();
  }

  if (nsAtom* roleAtom = acc->LandmarkRole()) {
    roleAtom->ToString(*aLandmark);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvARIARoleAtom(
    const uint64_t& aID, nsString* aRole) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return IPC_OK();
  }

  if (const nsRoleMapEntry* roleMap = acc->ARIARoleMap()) {
    if (nsStaticAtom* roleAtom = roleMap->roleAtom) {
      roleAtom->ToString(*aRole);
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvGroupPosition(
    const uint64_t& aID, int32_t* aLevel, int32_t* aSimilarItemsInGroup,
    int32_t* aPositionInGroup) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    GroupPos groupPos = acc->GroupPosition();
    *aLevel = groupPos.level;
    *aSimilarItemsInGroup = groupPos.setSize;
    *aPositionInGroup = groupPos.posInSet;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvScrollTo(
    const uint64_t& aID, const uint32_t& aScrollType) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    RefPtr<PresShell> presShell = acc->Document()->PresShellPtr();
    nsCOMPtr<nsIContent> content = acc->GetContent();
    nsCoreUtils::ScrollTo(presShell, content, aScrollType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvScrollToPoint(
    const uint64_t& aID, const uint32_t& aScrollType, const int32_t& aX,
    const int32_t& aY) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->ScrollToPoint(aScrollType, aX, aY);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAnnounce(
    const uint64_t& aID, const nsString& aAnnouncement,
    const uint16_t& aPriority) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->Announce(aAnnouncement, aPriority);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvCaretLineNumber(
    const uint64_t& aID, int32_t* aLineNumber) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aLineNumber = acc && acc->IsTextRole() ? acc->CaretLineNumber() : 0;
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvCaretOffset(const uint64_t& aID,
                                                            int32_t* aOffset) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aOffset = acc && acc->IsTextRole() ? acc->CaretOffset() : 0;
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvSetCaretOffset(
    const uint64_t& aID, const int32_t& aOffset) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole() && acc->IsValidOffset(aOffset)) {
    acc->SetCaretOffset(aOffset);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvCharacterCount(
    const uint64_t& aID, int32_t* aCount) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aCount = acc ? acc->CharacterCount() : 0;
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvSelectionCount(
    const uint64_t& aID, int32_t* aCount) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aCount = acc ? acc->SelectionCount() : 0;
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTextSubstring(
    const uint64_t& aID, const int32_t& aStartOffset, const int32_t& aEndOffset,
    nsString* aText, bool* aValid) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return IPC_OK();
  }

  TextLeafAccessible* leaf = acc->AsTextLeaf();
  if (leaf) {
    if (aStartOffset != 0 || aEndOffset != -1) {
      // We don't support fetching partial text from a leaf.
      *aValid = false;
      return IPC_OK();
    }
    *aValid = true;
    *aText = leaf->Text();
    return IPC_OK();
  }

  HyperTextAccessible* hyper = acc->AsHyperText();
  if (!hyper) {
    return IPC_OK();
  }

  *aValid = hyper->IsValidRange(aStartOffset, aEndOffset);
  hyper->TextSubstring(aStartOffset, aEndOffset, *aText);
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvGetTextAfterOffset(
    const uint64_t& aID, const int32_t& aOffset, const int32_t& aBoundaryType,
    nsString* aText, int32_t* aStartOffset, int32_t* aEndOffset) {
  *aStartOffset = 0;
  *aEndOffset = 0;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->TextAfterOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset,
                         *aText);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvGetTextAtOffset(
    const uint64_t& aID, const int32_t& aOffset, const int32_t& aBoundaryType,
    nsString* aText, int32_t* aStartOffset, int32_t* aEndOffset) {
  *aStartOffset = 0;
  *aEndOffset = 0;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->TextAtOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset, *aText);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvGetTextBeforeOffset(
    const uint64_t& aID, const int32_t& aOffset, const int32_t& aBoundaryType,
    nsString* aText, int32_t* aStartOffset, int32_t* aEndOffset) {
  *aStartOffset = 0;
  *aEndOffset = 0;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->TextBeforeOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset,
                          *aText);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvCharAt(const uint64_t& aID,
                                                       const int32_t& aOffset,
                                                       uint16_t* aChar) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aChar = acc && acc->IsTextRole()
               ? static_cast<uint16_t>(acc->CharAt(aOffset))
               : 0;
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTextAttributes(
    const uint64_t& aID, const bool& aIncludeDefAttrs, const int32_t& aOffset,
    nsTArray<Attribute>* aAttributes, int32_t* aStartOffset,
    int32_t* aEndOffset) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (!acc || !acc->IsTextRole()) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPersistentProperties> props =
      acc->TextAttributes(aIncludeDefAttrs, aOffset, aStartOffset, aEndOffset);
  if (!nsAccUtils::PersistentPropertiesToArray(props, aAttributes)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvDefaultTextAttributes(
    const uint64_t& aID, nsTArray<Attribute>* aAttributes) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (!acc || !acc->IsTextRole()) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPersistentProperties> props = acc->DefaultTextAttributes();
  if (!nsAccUtils::PersistentPropertiesToArray(props, aAttributes)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTextBounds(
    const uint64_t& aID, const int32_t& aStartOffset, const int32_t& aEndOffset,
    const uint32_t& aCoordType, nsIntRect* aRetVal) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aRetVal = acc->TextBounds(aStartOffset, aEndOffset, aCoordType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvCharBounds(
    const uint64_t& aID, const int32_t& aOffset, const uint32_t& aCoordType,
    nsIntRect* aRetVal) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aRetVal = acc->CharBounds(aOffset, aCoordType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvOffsetAtPoint(
    const uint64_t& aID, const int32_t& aX, const int32_t& aY,
    const uint32_t& aCoordType, int32_t* aRetVal) {
  *aRetVal = -1;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aRetVal = acc->OffsetAtPoint(aX, aY, aCoordType);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvSelectionBoundsAt(
    const uint64_t& aID, const int32_t& aSelectionNum, bool* aSucceeded,
    nsString* aData, int32_t* aStartOffset, int32_t* aEndOffset) {
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

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvSetSelectionBoundsAt(
    const uint64_t& aID, const int32_t& aSelectionNum,
    const int32_t& aStartOffset, const int32_t& aEndOffset, bool* aSucceeded) {
  *aSucceeded = false;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aSucceeded =
        acc->SetSelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAddToSelection(
    const uint64_t& aID, const int32_t& aStartOffset, const int32_t& aEndOffset,
    bool* aSucceeded) {
  *aSucceeded = false;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aSucceeded = acc->AddToSelection(aStartOffset, aEndOffset);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvRemoveFromSelection(
    const uint64_t& aID, const int32_t& aSelectionNum, bool* aSucceeded) {
  *aSucceeded = false;
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aSucceeded = acc->RemoveFromSelection(aSelectionNum);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvScrollSubstringTo(
    const uint64_t& aID, const int32_t& aStartOffset, const int32_t& aEndOffset,
    const uint32_t& aScrollType) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->ScrollSubstringTo(aStartOffset, aEndOffset, aScrollType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvScrollSubstringToPoint(
    const uint64_t& aID, const int32_t& aStartOffset, const int32_t& aEndOffset,
    const uint32_t& aCoordinateType, const int32_t& aX, const int32_t& aY) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc) {
    acc->ScrollSubstringToPoint(aStartOffset, aEndOffset, aCoordinateType, aX,
                                aY);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvText(const uint64_t& aID,
                                                     nsString* aText) {
  TextLeafAccessible* acc = IdToTextLeafAccessible(aID);
  if (acc) {
    *aText = acc->Text();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvReplaceText(
    const uint64_t& aID, const nsString& aText) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    acc->ReplaceText(aText);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvInsertText(
    const uint64_t& aID, const nsString& aText, const int32_t& aPosition,
    bool* aValid) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aValid = acc->IsValidOffset(aPosition);
    acc->InsertText(aText, aPosition);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvCopyText(
    const uint64_t& aID, const int32_t& aStartPos, const int32_t& aEndPos,
    bool* aValid) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    acc->CopyText(aStartPos, aEndPos);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvCutText(
    const uint64_t& aID, const int32_t& aStartPos, const int32_t& aEndPos,
    bool* aValid) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aValid = acc->IsValidRange(aStartPos, aEndPos);
    acc->CutText(aStartPos, aEndPos);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvDeleteText(
    const uint64_t& aID, const int32_t& aStartPos, const int32_t& aEndPos,
    bool* aValid) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aValid = acc->IsValidRange(aStartPos, aEndPos);
    acc->DeleteText(aStartPos, aEndPos);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvPasteText(
    const uint64_t& aID, const int32_t& aPosition, bool* aValid) {
  RefPtr<HyperTextAccessible> acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    *aValid = acc->IsValidOffset(aPosition);
    acc->PasteText(aPosition);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvImagePosition(
    const uint64_t& aID, const uint32_t& aCoordType, nsIntPoint* aRetVal) {
  ImageAccessible* acc = IdToImageAccessible(aID);
  if (acc) {
    *aRetVal = acc->Position(aCoordType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvImageSize(const uint64_t& aID,
                                                          nsIntSize* aRetVal) {
  ImageAccessible* acc = IdToImageAccessible(aID);
  if (acc) {
    *aRetVal = acc->Size();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvStartOffset(const uint64_t& aID,
                                                            uint32_t* aRetVal,
                                                            bool* aOk) {
  Accessible* acc = IdToAccessibleLink(aID);
  if (acc) {
    *aRetVal = acc->StartOffset();
    *aOk = true;
  } else {
    *aRetVal = 0;
    *aOk = false;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvEndOffset(const uint64_t& aID,
                                                          uint32_t* aRetVal,
                                                          bool* aOk) {
  Accessible* acc = IdToAccessibleLink(aID);
  if (acc) {
    *aRetVal = acc->EndOffset();
    *aOk = true;
  } else {
    *aRetVal = 0;
    *aOk = false;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvIsLinkValid(const uint64_t& aID,
                                                            bool* aRetVal) {
  Accessible* acc = IdToAccessibleLink(aID);
  if (acc) {
    *aRetVal = acc->IsLinkValid();
  } else {
    *aRetVal = false;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAnchorCount(const uint64_t& aID,
                                                            uint32_t* aRetVal,
                                                            bool* aOk) {
  Accessible* acc = IdToAccessibleLink(aID);
  if (acc) {
    *aRetVal = acc->AnchorCount();
    *aOk = true;
  } else {
    *aRetVal = 0;
    *aOk = false;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAnchorURIAt(
    const uint64_t& aID, const uint32_t& aIndex, nsCString* aURI, bool* aOk) {
  Accessible* acc = IdToAccessibleLink(aID);
  *aOk = false;
  if (acc) {
    nsCOMPtr<nsIURI> uri = acc->AnchorURIAt(aIndex);
    if (uri) {
      uri->GetSpec(*aURI);
      *aOk = true;
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAnchorAt(const uint64_t& aID,
                                                         const uint32_t& aIndex,
                                                         uint64_t* aIDOfAnchor,
                                                         bool* aOk) {
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

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvLinkCount(const uint64_t& aID,
                                                          uint32_t* aCount) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aCount = acc ? acc->LinkCount() : 0;
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvLinkAt(const uint64_t& aID,
                                                       const uint32_t& aIndex,
                                                       uint64_t* aIDOfLink,
                                                       bool* aOk) {
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

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvLinkIndexOf(
    const uint64_t& aID, const uint64_t& aLinkID, int32_t* aIndex) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  Accessible* link = IdToAccessible(aLinkID);
  *aIndex = -1;
  if (acc && link) {
    *aIndex = acc->LinkIndexOf(link);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvLinkIndexAtOffset(
    const uint64_t& aID, const uint32_t& aOffset, int32_t* aIndex) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  *aIndex = acc ? acc->LinkIndexAtOffset(aOffset) : -1;
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableOfACell(
    const uint64_t& aID, uint64_t* aTableID, bool* aOk) {
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

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvColIdx(const uint64_t& aID,
                                                       uint32_t* aIndex) {
  *aIndex = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aIndex = acc->ColIdx();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvRowIdx(const uint64_t& aID,
                                                       uint32_t* aIndex) {
  *aIndex = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aIndex = acc->RowIdx();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvGetPosition(const uint64_t& aID,
                                                            uint32_t* aColIdx,
                                                            uint32_t* aRowIdx) {
  *aColIdx = 0;
  *aRowIdx = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aColIdx = acc->ColIdx();
    *aRowIdx = acc->RowIdx();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvGetColRowExtents(
    const uint64_t& aID, uint32_t* aColIdx, uint32_t* aRowIdx,
    uint32_t* aColExtent, uint32_t* aRowExtent) {
  *aColIdx = 0;
  *aRowIdx = 0;
  *aColExtent = 0;
  *aRowExtent = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aColIdx = acc->ColIdx();
    *aRowIdx = acc->RowIdx();
    *aColExtent = acc->ColExtent();
    *aRowExtent = acc->RowExtent();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvColExtent(const uint64_t& aID,
                                                          uint32_t* aExtent) {
  *aExtent = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aExtent = acc->ColExtent();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvRowExtent(const uint64_t& aID,
                                                          uint32_t* aExtent) {
  *aExtent = 0;
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    *aExtent = acc->RowExtent();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvColHeaderCells(
    const uint64_t& aID, nsTArray<uint64_t>* aCells) {
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    AutoTArray<Accessible*, 10> headerCells;
    acc->ColHeaderCells(&headerCells);
    aCells->SetCapacity(headerCells.Length());
    for (uint32_t i = 0; i < headerCells.Length(); ++i) {
      aCells->AppendElement(
          reinterpret_cast<uint64_t>(headerCells[i]->UniqueID()));
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvRowHeaderCells(
    const uint64_t& aID, nsTArray<uint64_t>* aCells) {
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  if (acc) {
    AutoTArray<Accessible*, 10> headerCells;
    acc->RowHeaderCells(&headerCells);
    aCells->SetCapacity(headerCells.Length());
    for (uint32_t i = 0; i < headerCells.Length(); ++i) {
      aCells->AppendElement(
          reinterpret_cast<uint64_t>(headerCells[i]->UniqueID()));
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvIsCellSelected(
    const uint64_t& aID, bool* aSelected) {
  TableCellAccessible* acc = IdToTableCellAccessible(aID);
  *aSelected = acc && acc->Selected();
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableCaption(
    const uint64_t& aID, uint64_t* aCaptionID, bool* aOk) {
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

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSummary(
    const uint64_t& aID, nsString* aSummary) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->Summary(*aSummary);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableColumnCount(
    const uint64_t& aID, uint32_t* aColCount) {
  *aColCount = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aColCount = acc->ColCount();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableRowCount(
    const uint64_t& aID, uint32_t* aRowCount) {
  *aRowCount = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aRowCount = acc->RowCount();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableCellAt(
    const uint64_t& aID, const uint32_t& aRow, const uint32_t& aCol,
    uint64_t* aCellID, bool* aOk) {
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

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableCellIndexAt(
    const uint64_t& aID, const uint32_t& aRow, const uint32_t& aCol,
    int32_t* aIndex) {
  *aIndex = -1;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aIndex = acc->CellIndexAt(aRow, aCol);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableColumnIndexAt(
    const uint64_t& aID, const uint32_t& aCellIndex, int32_t* aCol) {
  *aCol = -1;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aCol = acc->ColIndexAt(aCellIndex);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableRowIndexAt(
    const uint64_t& aID, const uint32_t& aCellIndex, int32_t* aRow) {
  *aRow = -1;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aRow = acc->RowIndexAt(aCellIndex);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableRowAndColumnIndicesAt(
    const uint64_t& aID, const uint32_t& aCellIndex, int32_t* aRow,
    int32_t* aCol) {
  *aRow = -1;
  *aCol = -1;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->RowAndColIndicesAt(aCellIndex, aRow, aCol);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableColumnExtentAt(
    const uint64_t& aID, const uint32_t& aRow, const uint32_t& aCol,
    uint32_t* aExtent) {
  *aExtent = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aExtent = acc->ColExtentAt(aRow, aCol);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableRowExtentAt(
    const uint64_t& aID, const uint32_t& aRow, const uint32_t& aCol,
    uint32_t* aExtent) {
  *aExtent = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aExtent = acc->RowExtentAt(aRow, aCol);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableColumnDescription(
    const uint64_t& aID, const uint32_t& aCol, nsString* aDescription) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->ColDescription(aCol, *aDescription);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableRowDescription(
    const uint64_t& aID, const uint32_t& aRow, nsString* aDescription) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->RowDescription(aRow, *aDescription);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableColumnSelected(
    const uint64_t& aID, const uint32_t& aCol, bool* aSelected) {
  *aSelected = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelected = acc->IsColSelected(aCol);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableRowSelected(
    const uint64_t& aID, const uint32_t& aRow, bool* aSelected) {
  *aSelected = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelected = acc->IsRowSelected(aRow);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableCellSelected(
    const uint64_t& aID, const uint32_t& aRow, const uint32_t& aCol,
    bool* aSelected) {
  *aSelected = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelected = acc->IsCellSelected(aRow, aCol);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSelectedCellCount(
    const uint64_t& aID, uint32_t* aSelectedCells) {
  *aSelectedCells = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelectedCells = acc->SelectedCellCount();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSelectedColumnCount(
    const uint64_t& aID, uint32_t* aSelectedColumns) {
  *aSelectedColumns = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelectedColumns = acc->SelectedColCount();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSelectedRowCount(
    const uint64_t& aID, uint32_t* aSelectedRows) {
  *aSelectedRows = 0;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aSelectedRows = acc->SelectedRowCount();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSelectedCells(
    const uint64_t& aID, nsTArray<uint64_t>* aCellIDs) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    AutoTArray<Accessible*, 30> cells;
    acc->SelectedCells(&cells);
    aCellIDs->SetCapacity(cells.Length());
    for (uint32_t i = 0; i < cells.Length(); ++i) {
      aCellIDs->AppendElement(reinterpret_cast<uint64_t>(cells[i]->UniqueID()));
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSelectedCellIndices(
    const uint64_t& aID, nsTArray<uint32_t>* aCellIndices) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectedCellIndices(aCellIndices);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSelectedColumnIndices(
    const uint64_t& aID, nsTArray<uint32_t>* aColumnIndices) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectedColIndices(aColumnIndices);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSelectedRowIndices(
    const uint64_t& aID, nsTArray<uint32_t>* aRowIndices) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectedRowIndices(aRowIndices);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSelectColumn(
    const uint64_t& aID, const uint32_t& aCol) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectCol(aCol);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableSelectRow(
    const uint64_t& aID, const uint32_t& aRow) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->SelectRow(aRow);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableUnselectColumn(
    const uint64_t& aID, const uint32_t& aCol) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->UnselectCol(aCol);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableUnselectRow(
    const uint64_t& aID, const uint32_t& aRow) {
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    acc->UnselectRow(aRow);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTableIsProbablyForLayout(
    const uint64_t& aID, bool* aForLayout) {
  *aForLayout = false;
  TableAccessible* acc = IdToTableAccessible(aID);
  if (acc) {
    *aForLayout = acc->IsProbablyLayoutTable();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAtkTableColumnHeader(
    const uint64_t& aID, const int32_t& aCol, uint64_t* aHeader, bool* aOk) {
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

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAtkTableRowHeader(
    const uint64_t& aID, const int32_t& aRow, uint64_t* aHeader, bool* aOk) {
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

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvSelectedItems(
    const uint64_t& aID, nsTArray<uint64_t>* aSelectedItemIDs) {
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    AutoTArray<Accessible*, 10> selectedItems;
    acc->SelectedItems(&selectedItems);
    aSelectedItemIDs->SetCapacity(selectedItems.Length());
    for (size_t i = 0; i < selectedItems.Length(); ++i) {
      aSelectedItemIDs->AppendElement(
          reinterpret_cast<uint64_t>(selectedItems[i]->UniqueID()));
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvSelectedItemCount(
    const uint64_t& aID, uint32_t* aCount) {
  *aCount = 0;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aCount = acc->SelectedItemCount();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvGetSelectedItem(
    const uint64_t& aID, const uint32_t& aIndex, uint64_t* aSelected,
    bool* aOk) {
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

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvIsItemSelected(
    const uint64_t& aID, const uint32_t& aIndex, bool* aSelected) {
  *aSelected = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSelected = acc->IsItemSelected(aIndex);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAddItemToSelection(
    const uint64_t& aID, const uint32_t& aIndex, bool* aSuccess) {
  *aSuccess = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSuccess = acc->AddItemToSelection(aIndex);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvRemoveItemFromSelection(
    const uint64_t& aID, const uint32_t& aIndex, bool* aSuccess) {
  *aSuccess = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSuccess = acc->RemoveItemFromSelection(aIndex);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvSelectAll(const uint64_t& aID,
                                                          bool* aSuccess) {
  *aSuccess = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSuccess = acc->SelectAll();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvUnselectAll(const uint64_t& aID,
                                                            bool* aSuccess) {
  *aSuccess = false;
  Accessible* acc = IdToAccessibleSelect(aID);
  if (acc) {
    *aSuccess = acc->UnselectAll();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTakeSelection(
    const uint64_t& aID) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->TakeSelection();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvSetSelected(
    const uint64_t& aID, const bool& aSelect) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->SetSelected(aSelect);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvDoAction(const uint64_t& aID,
                                                         const uint8_t& aIndex,
                                                         bool* aSuccess) {
  *aSuccess = false;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aSuccess = acc->DoAction(aIndex);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvActionCount(const uint64_t& aID,
                                                            uint8_t* aCount) {
  *aCount = 0;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aCount = acc->ActionCount();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvActionDescriptionAt(
    const uint64_t& aID, const uint8_t& aIndex, nsString* aDescription) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->ActionDescriptionAt(aIndex, *aDescription);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvActionNameAt(
    const uint64_t& aID, const uint8_t& aIndex, nsString* aName) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->ActionNameAt(aIndex, *aName);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAccessKey(
    const uint64_t& aID, uint32_t* aKey, uint32_t* aModifierMask) {
  *aKey = 0;
  *aModifierMask = 0;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    KeyBinding kb = acc->AccessKey();
    *aKey = kb.Key();
    *aModifierMask = kb.ModifierMask();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvKeyboardShortcut(
    const uint64_t& aID, uint32_t* aKey, uint32_t* aModifierMask) {
  *aKey = 0;
  *aModifierMask = 0;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    KeyBinding kb = acc->KeyboardShortcut();
    *aKey = kb.Key();
    *aModifierMask = kb.ModifierMask();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAtkKeyBinding(
    const uint64_t& aID, nsString* aResult) {
#ifdef MOZ_ACCESSIBILITY_ATK
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    AccessibleWrap::GetKeyBinding(acc, *aResult);
  }
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvCurValue(const uint64_t& aID,
                                                         double* aValue) {
  *aValue = UnspecifiedNaN<double>();
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aValue = acc->CurValue();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvSetCurValue(
    const uint64_t& aID, const double& aValue, bool* aRetVal) {
  *aRetVal = false;
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aRetVal = acc->SetCurValue(aValue);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvMinValue(const uint64_t& aID,
                                                         double* aValue) {
  *aValue = UnspecifiedNaN<double>();
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aValue = acc->MinValue();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvMaxValue(const uint64_t& aID,
                                                         double* aValue) {
  *aValue = UnspecifiedNaN<double>();
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aValue = acc->MaxValue();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvStep(const uint64_t& aID,
                                                     double* aStep) {
  *aStep = UnspecifiedNaN<double>();
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    *aStep = acc->Step();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTakeFocus(const uint64_t& aID) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->TakeFocus();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvFocusedChild(
    const uint64_t& aID, uint64_t* aChild, bool* aOk) {
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
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvLanguage(const uint64_t& aID,
                                                         nsString* aLocale) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->Language(*aLocale);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvDocType(const uint64_t& aID,
                                                        nsString* aType) {
  Accessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    acc->AsDoc()->DocType(*aType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvTitle(const uint64_t& aID,
                                                      nsString* aTitle) {
  Accessible* acc = IdToAccessible(aID);
  if (acc) {
    mozilla::ErrorResult rv;
    acc->GetContent()->GetTextContent(*aTitle, rv);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvURL(const uint64_t& aID,
                                                    nsString* aURL) {
  Accessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    acc->AsDoc()->URL(*aURL);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvMimeType(const uint64_t& aID,
                                                         nsString* aMime) {
  Accessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    acc->AsDoc()->MimeType(*aMime);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvURLDocTypeMimeType(
    const uint64_t& aID, nsString* aURL, nsString* aDocType,
    nsString* aMimeType) {
  Accessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    DocAccessible* doc = acc->AsDoc();
    doc->URL(*aURL);
    doc->DocType(*aDocType);
    doc->MimeType(*aMimeType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvChildAtPoint(
    const uint64_t& aID, const int32_t& aX, const int32_t& aY,
    const uint32_t& aWhich, PDocAccessibleChild** aResultDoc,
    uint64_t* aResultID) {
  *aResultDoc = nullptr;
  *aResultID = 0;
  Accessible* acc = IdToAccessible(aID);
  if (acc && !acc->IsDefunct()) {
    int32_t x = aX;
    int32_t y = aY;
    Accessible* result = acc->ChildAtPoint(
        x, y, static_cast<Accessible::EWhichChildAtPoint>(aWhich));
    if (result) {
      // Accessible::ChildAtPoint can return an Accessible from a descendant
      // document.
      *aResultDoc = result->Document()->IPCDoc();
      *aResultID =
          result->IsDoc() ? 0 : reinterpret_cast<uint64_t>(result->UniqueID());
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvExtents(
    const uint64_t& aID, const bool& aNeedsScreenCoords, int32_t* aX,
    int32_t* aY, int32_t* aWidth, int32_t* aHeight) {
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
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvExtentsInCSSPixels(
    const uint64_t& aID, int32_t* aX, int32_t* aY, int32_t* aWidth,
    int32_t* aHeight) {
  *aX = 0;
  *aY = 0;
  *aWidth = 0;
  *aHeight = 0;
  Accessible* acc = IdToAccessible(aID);
  if (acc && !acc->IsDefunct()) {
    nsIntRect screenRect = acc->BoundsInCSSPixels();
    if (!screenRect.IsEmpty()) {
      *aX = screenRect.x;
      *aY = screenRect.y;
      *aWidth = screenRect.width;
      *aHeight = screenRect.height;
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvDOMNodeID(
    const uint64_t& aID, nsString* aDOMNodeID) {
  Accessible* acc = IdToAccessible(aID);
  if (!acc) {
    return IPC_OK();
  }

  nsIContent* content = acc->GetContent();
  if (!content) {
    return IPC_OK();
  }

  nsAtom* id = content->GetID();
  if (id) {
    id->ToString(*aDOMNodeID);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvRestoreFocus() {
  FocusMgr()->ForceFocusEvent();
  return IPC_OK();
}

bool DocAccessibleChild::DeallocPDocAccessiblePlatformExtChild(
    PDocAccessiblePlatformExtChild* aActor) {
  delete aActor;
  return true;
}

PDocAccessiblePlatformExtChild*
DocAccessibleChild::AllocPDocAccessiblePlatformExtChild() {
  return new DocAccessiblePlatformExtChild();
}

DocAccessiblePlatformExtChild* DocAccessibleChild::GetPlatformExtension() {
  return static_cast<DocAccessiblePlatformExtChild*>(
      SingleManagedOrNull(ManagedPDocAccessiblePlatformExtChild()));
}

}  // namespace a11y
}  // namespace mozilla
