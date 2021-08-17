/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/DocAccessibleChildBase.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/StaticPrefs_accessibility.h"

#include "LocalAccessible-inl.h"

namespace mozilla {
namespace a11y {

/* static */
void DocAccessibleChildBase::FlattenTree(LocalAccessible* aRoot,
                                         nsTArray<LocalAccessible*>& aTree) {
  MOZ_ASSERT(!aRoot->IsDoc(), "documents shouldn't be serialized");

  aTree.AppendElement(aRoot);
  // OuterDocAccessibles are special because we don't want to serialize the
  // child doc here, we'll call PDocAccessibleConstructor in
  // NotificationController.
  uint32_t childCount = aRoot->IsOuterDoc() ? 0 : aRoot->ChildCount();

  for (uint32_t i = 0; i < childCount; i++) {
    FlattenTree(aRoot->LocalChildAt(i), aTree);
  }
}

/* static */
void DocAccessibleChildBase::SerializeTree(nsTArray<LocalAccessible*>& aTree,
                                           nsTArray<AccessibleData>& aData) {
  for (LocalAccessible* acc : aTree) {
    uint64_t id = reinterpret_cast<uint64_t>(acc->UniqueID());
#if defined(XP_WIN)
    int32_t msaaId = StaticPrefs::accessibility_cache_enabled_AtStartup()
                         ? 0
                         : MsaaAccessible::GetChildIDFor(acc);
#endif
    a11y::role role = acc->Role();
    uint32_t childCount = acc->IsOuterDoc() ? 0 : acc->ChildCount();

    uint32_t genericTypes = acc->mGenericTypes;
    if (acc->ARIAHasNumericValue()) {
      // XXX: We need to do this because this requires a state check.
      genericTypes |= eNumericValue;
    }
    if (acc->ActionCount()) {
      genericTypes |= eActionable;
    }

#if defined(XP_WIN)
    aData.AppendElement(AccessibleData(
        id, msaaId, role, childCount, static_cast<AccType>(acc->mType),
        static_cast<AccGenericType>(genericTypes), acc->mRoleMapEntryIndex));
#else
    aData.AppendElement(AccessibleData(
        id, role, childCount, static_cast<AccType>(acc->mType),
        static_cast<AccGenericType>(genericTypes), acc->mRoleMapEntryIndex));
#endif
  }
}

void DocAccessibleChildBase::InsertIntoIpcTree(LocalAccessible* aParent,
                                               LocalAccessible* aChild,
                                               uint32_t aIdxInParent,
                                               bool aSuppressShowEvent) {
  uint64_t parentID =
      aParent->IsDoc() ? 0 : reinterpret_cast<uint64_t>(aParent->UniqueID());
  nsTArray<LocalAccessible*> shownTree;
  FlattenTree(aChild, shownTree);
  ShowEventData data(parentID, aIdxInParent,
                     nsTArray<AccessibleData>(shownTree.Length()),
                     aSuppressShowEvent);
  SerializeTree(shownTree, data.NewTree());
  MaybeSendShowEvent(data, false);
}

void DocAccessibleChildBase::ShowEvent(AccShowEvent* aShowEvent) {
  LocalAccessible* child = aShowEvent->GetAccessible();
  InsertIntoIpcTree(aShowEvent->LocalParent(), child, child->IndexInParent(),
                    false);
}

}  // namespace a11y
}  // namespace mozilla
