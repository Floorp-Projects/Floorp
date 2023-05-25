/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleChild.h"

#include "LocalAccessible-inl.h"
#include "RemoteAccessible.h"
#include "HyperTextAccessible-inl.h"
#include "mozilla/a11y/DocAccessiblePlatformExtChild.h"

namespace mozilla {
namespace a11y {

mozilla::ipc::IPCResult DocAccessibleChild::RecvScrollToPoint(
    const uint64_t& aID, const uint32_t& aScrollType, const int32_t& aX,
    const int32_t& aY) {
  LocalAccessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->ScrollToPoint(aScrollType, aX, aY);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvAnnounce(
    const uint64_t& aID, const nsAString& aAnnouncement,
    const uint16_t& aPriority) {
  LocalAccessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->Announce(aAnnouncement, aPriority);
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

mozilla::ipc::IPCResult DocAccessibleChild::RecvReplaceText(
    const uint64_t& aID, const nsAString& aText) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole()) {
    acc->ReplaceText(aText);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvInsertText(
    const uint64_t& aID, const nsAString& aText, const int32_t& aPosition,
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

mozilla::ipc::IPCResult DocAccessibleChild::RecvDocType(const uint64_t& aID,
                                                        nsString* aType) {
  LocalAccessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    acc->AsDoc()->DocType(*aType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvMimeType(const uint64_t& aID,
                                                         nsString* aMime) {
  LocalAccessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    acc->AsDoc()->MimeType(*aMime);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvURLDocTypeMimeType(
    const uint64_t& aID, nsString* aURL, nsString* aDocType,
    nsString* aMimeType) {
  LocalAccessible* acc = IdToAccessible(aID);
  if (acc && acc->IsDoc()) {
    DocAccessible* doc = acc->AsDoc();
    doc->URL(*aURL);
    doc->DocType(*aDocType);
    doc->MimeType(*aMimeType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChild::RecvRestoreFocus() {
  if (FocusManager* focusMgr = FocusMgr()) {
    focusMgr->ForceFocusEvent();
  }
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
