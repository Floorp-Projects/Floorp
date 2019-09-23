/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessiblePlatformExtChild.h"

#include "DocAccessibleChild.h"
#include "AccessibleWrap.h"

namespace mozilla {
namespace a11y {

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvPivot(
    uint64_t aID, int32_t aGranularity, bool aForward, bool aInclusive) {
  if (auto acc = IdToAccessibleWrap(aID)) {
    acc->Pivot(aGranularity, aForward, aInclusive);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvNavigateText(
    uint64_t aID, int32_t aGranularity, int32_t aStartOffset, int32_t aEndOffset,
    bool aForward, bool aSelect) {
  if (auto acc = IdToAccessibleWrap(aID)) {
    acc->NavigateText(aGranularity, aStartOffset, aEndOffset, aForward,
                      aSelect);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvSetSelection(
    uint64_t aID, int32_t aStart, int32_t aEnd) {
  if (auto acc = IdToAccessibleWrap(aID)) {
    // XXX: Forward to appropriate wrapper method.
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvCut(uint64_t aID) {
  if (auto acc = IdToAccessibleWrap(aID)) {
    // XXX: Forward to appropriate wrapper method.
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvCopy(uint64_t aID) {
  if (auto acc = IdToAccessibleWrap(aID)) {
    // XXX: Forward to appropriate wrapper method.
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvPaste(uint64_t aID) {
  if (auto acc = IdToAccessibleWrap(aID)) {
    // XXX: Forward to appropriate wrapper method.
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvExploreByTouch(
    uint64_t aID, float aX, float aY) {
  if (auto acc = IdToAccessibleWrap(aID)) {
    acc->ExploreByTouch(aX, aY);
  }

  return IPC_OK();
}

AccessibleWrap* DocAccessiblePlatformExtChild::IdToAccessibleWrap(
    const uint64_t& aID) const {
  return static_cast<AccessibleWrap*>(
      static_cast<DocAccessibleChild*>(Manager())->IdToAccessible(aID));
}

}  // namespace a11y
}  // namespace mozilla
