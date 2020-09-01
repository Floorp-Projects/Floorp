/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessiblePlatformExtParent.h"

#include "AccessibleWrap.h"
#include "SessionAccessibility.h"

#include "mozilla/a11y/DocAccessibleParent.h"

namespace mozilla {
namespace a11y {

mozilla::ipc::IPCResult DocAccessiblePlatformExtParent::RecvSetPivotBoundaries(
    PDocAccessibleParent* aFirstDoc, uint64_t aFirst,
    PDocAccessibleParent* aLastDoc, uint64_t aLast) {
  MOZ_ASSERT(aFirstDoc);
  MOZ_ASSERT(aLastDoc);

  SessionAccessibility* sessionAcc = SessionAccessibility::GetInstanceFor(
      static_cast<DocAccessibleParent*>(Manager()));
  if (!sessionAcc) {
    return IPC_OK();
  }

  ProxyAccessible* first =
      static_cast<DocAccessibleParent*>(aFirstDoc)->GetAccessible(aFirst);
  ProxyAccessible* last =
      static_cast<DocAccessibleParent*>(aLastDoc)->GetAccessible(aLast);

  // We may not have proxy accessibles available yet for those accessibles
  // in the parent process.
  if (first && last) {
    sessionAcc->UpdateAccessibleFocusBoundaries(WrapperFor(first),
                                                WrapperFor(last));
  }

  return IPC_OK();
}

}  // namespace a11y
}  // namespace mozilla
