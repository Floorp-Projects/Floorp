/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessiblePlatformExtChild.h"

#include "DocAccessibleChild.h"
#include "HyperTextAccessibleWrap.h"

#define UNIQUE_ID(acc)               \
  !acc || acc->Document() == acc ? 0 \
                                 : reinterpret_cast<uint64_t>(acc->UniqueID())

namespace mozilla {
namespace a11y {

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvLeftWordAt(
    const uint64_t& aID, const int32_t& aOffset, uint64_t* aStartContainer,
    int32_t* aStartOffset, uint64_t* aEndContainer, int32_t* aEndOffset) {
  HyperTextAccessibleWrap* acc = IdToHyperTextAccessibleWrap(aID);
  if (!acc) {
    return IPC_OK();
  }

  HyperTextAccessible* startContainer = nullptr;
  HyperTextAccessible* endContainer = nullptr;

  acc->LeftWordAt(aOffset, &startContainer, aStartOffset, &endContainer,
                  aEndOffset);

  MOZ_ASSERT(!startContainer || startContainer->Document() == acc->Document());
  MOZ_ASSERT(!endContainer || endContainer->Document() == acc->Document());

  *aStartContainer = UNIQUE_ID(startContainer);
  *aEndContainer = UNIQUE_ID(endContainer);

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvRightWordAt(
    const uint64_t& aID, const int32_t& aOffset, uint64_t* aStartContainer,
    int32_t* aStartOffset, uint64_t* aEndContainer, int32_t* aEndOffset) {
  HyperTextAccessibleWrap* acc = IdToHyperTextAccessibleWrap(aID);
  if (!acc) {
    return IPC_OK();
  }

  HyperTextAccessible* startContainer = nullptr;
  HyperTextAccessible* endContainer = nullptr;

  acc->RightWordAt(aOffset, &startContainer, aStartOffset, &endContainer,
                   aEndOffset);

  MOZ_ASSERT(!startContainer || startContainer->Document() == acc->Document());
  MOZ_ASSERT(!endContainer || endContainer->Document() == acc->Document());

  *aStartContainer = UNIQUE_ID(startContainer);
  *aEndContainer = UNIQUE_ID(endContainer);

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvNextClusterAt(
    const uint64_t& aID, const int32_t& aOffset, uint64_t* aNextContainer,
    int32_t* aNextOffset) {
  HyperTextAccessibleWrap* acc = IdToHyperTextAccessibleWrap(aID);
  if (!acc) {
    return IPC_OK();
  }

  HyperTextAccessible* nextContainer = nullptr;

  acc->NextClusterAt(aOffset, &nextContainer, aNextOffset);

  MOZ_ASSERT(!nextContainer || nextContainer->Document() == acc->Document());

  *aNextContainer = UNIQUE_ID(nextContainer);

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvPreviousClusterAt(
    const uint64_t& aID, const int32_t& aOffset, uint64_t* aPrevContainer,
    int32_t* aPrevOffset) {
  HyperTextAccessibleWrap* acc = IdToHyperTextAccessibleWrap(aID);
  if (!acc) {
    return IPC_OK();
  }

  HyperTextAccessible* prevContainer = nullptr;

  acc->PreviousClusterAt(aOffset, &prevContainer, aPrevOffset);

  MOZ_ASSERT(!prevContainer || prevContainer->Document() == acc->Document());

  *aPrevContainer = UNIQUE_ID(prevContainer);

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvTextForRange(
    const uint64_t& aID, const int32_t& aStartOffset,
    const uint64_t& aEndContainer, const int32_t& aEndOffset, nsString* aText) {
  HyperTextAccessibleWrap* acc = IdToHyperTextAccessibleWrap(aID);
  HyperTextAccessibleWrap* endContainer =
      IdToHyperTextAccessibleWrap(aEndContainer);
  if (!acc || !endContainer) {
    return IPC_OK();
  }

  acc->TextForRange(*aText, aStartOffset, endContainer, aEndOffset);

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessiblePlatformExtChild::RecvBoundsForRange(
    const uint64_t& aID, const int32_t& aStartOffset,
    const uint64_t& aEndContainer, const int32_t& aEndOffset,
    nsIntRect* aBounds) {
  return IPC_OK();
}

HyperTextAccessibleWrap*
DocAccessiblePlatformExtChild::IdToHyperTextAccessibleWrap(
    const uint64_t& aID) const {
  return static_cast<HyperTextAccessibleWrap*>(
      static_cast<DocAccessibleChild*>(Manager())->IdToHyperTextAccessible(
          aID));
}

}  // namespace a11y
}  // namespace mozilla
