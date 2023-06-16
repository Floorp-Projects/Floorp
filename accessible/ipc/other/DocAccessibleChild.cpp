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
