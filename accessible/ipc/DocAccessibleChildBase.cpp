/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/DocAccessibleChildBase.h"
#include "mozilla/a11y/RemoteAccessible.h"

#include "LocalAccessible-inl.h"

namespace mozilla {
namespace a11y {

/* static */
void DocAccessibleChildBase::SerializeTree(LocalAccessible* aRoot,
                                           nsTArray<AccessibleData>& aTree) {
  uint64_t id = reinterpret_cast<uint64_t>(aRoot->UniqueID());
#if defined(XP_WIN)
  int32_t msaaId = MsaaAccessible::GetChildIDFor(aRoot);
#endif
  a11y::role role = aRoot->Role();
  uint32_t childCount = aRoot->ChildCount();

  // OuterDocAccessibles are special because we don't want to serialize the
  // child doc here, we'll call PDocAccessibleConstructor in
  // NotificationController.
  MOZ_ASSERT(!aRoot->IsDoc(), "documents shouldn't be serialized");
  if (aRoot->IsOuterDoc()) {
    childCount = 0;
  }

  uint32_t genericTypes = aRoot->mGenericTypes;
  if (aRoot->ARIAHasNumericValue()) {
    // XXX: We need to do this because this requires a state check.
    genericTypes |= eNumericValue;
  }
  if (aRoot->ActionCount()) {
    genericTypes |= eActionable;
  }

#if defined(XP_WIN)
  aTree.AppendElement(AccessibleData(
      id, msaaId, role, childCount, static_cast<AccType>(aRoot->mType),
      static_cast<AccGenericType>(genericTypes), aRoot->mRoleMapEntryIndex));
#else
  aTree.AppendElement(AccessibleData(
      id, role, childCount, static_cast<AccType>(aRoot->mType),
      static_cast<AccGenericType>(genericTypes), aRoot->mRoleMapEntryIndex));
#endif

  for (uint32_t i = 0; i < childCount; i++) {
    SerializeTree(aRoot->LocalChildAt(i), aTree);
  }
}

void DocAccessibleChildBase::InsertIntoIpcTree(LocalAccessible* aParent,
                                               LocalAccessible* aChild,
                                               uint32_t aIdxInParent) {
  uint64_t parentID =
      aParent->IsDoc() ? 0 : reinterpret_cast<uint64_t>(aParent->UniqueID());
  nsTArray<AccessibleData> shownTree;
  ShowEventData data(parentID, aIdxInParent, shownTree, true);
  SerializeTree(aChild, data.NewTree());
  MaybeSendShowEvent(data, false);
}

void DocAccessibleChildBase::ShowEvent(AccShowEvent* aShowEvent) {
  LocalAccessible* parent = aShowEvent->LocalParent();
  uint64_t parentID =
      parent->IsDoc() ? 0 : reinterpret_cast<uint64_t>(parent->UniqueID());
  uint32_t idxInParent = aShowEvent->GetAccessible()->IndexInParent();
  nsTArray<AccessibleData> shownTree;
  ShowEventData data(parentID, idxInParent, shownTree, false);
  SerializeTree(aShowEvent->GetAccessible(), data.NewTree());
  MaybeSendShowEvent(data, aShowEvent->IsFromUserInput());
}

}  // namespace a11y
}  // namespace mozilla
