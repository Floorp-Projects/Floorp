/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/DocAccessibleChildBase.h"
#include "mozilla/a11y/CacheConstants.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/StaticPrefs_accessibility.h"

#include "LocalAccessible-inl.h"
#ifdef A11Y_LOG
#  include "Logging.h"
#endif

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
    if (acc->IsTextLeaf() || acc->IsImage()) {
      // Ideally, we'd set eActionable for any Accessible with an ancedstor
      // action. However, that requires an ancestor walk which is too expensive
      // here. eActionable is only used by ATK. For now, we only expose ancestor
      // actions on text leaf and image Accessibles. This means that we don't
      // support "click ancestor" for ATK.
      if (acc->ActionCount()) {
        genericTypes |= eActionable;
      }
    } else if (acc->HasPrimaryAction()) {
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
                     aSuppressShowEvent ||
                         StaticPrefs::accessibility_cache_enabled_AtStartup());
  SerializeTree(shownTree, data.NewTree());
  MaybeSendShowEvent(data, false);
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    nsTArray<CacheData> cache(shownTree.Length());
    for (LocalAccessible* acc : shownTree) {
      if (mDoc->IsAccessibleBeingMoved(acc)) {
        // Even though we send moves as a hide and a show, we don't want to
        // push the cache again for moves.
        continue;
      }
      RefPtr<AccAttributes> fields =
          acc->BundleFieldsForCache(CacheDomain::All, CacheUpdateType::Initial);
      if (fields->Count()) {
        uint64_t id = reinterpret_cast<uint64_t>(acc->UniqueID());
        cache.AppendElement(CacheData(id, fields));
      }
    }
    Unused << SendCache(CacheUpdateType::Initial, cache, true);
  }
}

void DocAccessibleChildBase::ShowEvent(AccShowEvent* aShowEvent) {
  LocalAccessible* child = aShowEvent->GetAccessible();
  InsertIntoIpcTree(aShowEvent->LocalParent(), child, child->IndexInParent(),
                    false);
}

mozilla::ipc::IPCResult DocAccessibleChildBase::RecvTakeFocus(
    const uint64_t& aID) {
  LocalAccessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->TakeFocus();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChildBase::RecvScrollTo(
    const uint64_t& aID, const uint32_t& aScrollType) {
  LocalAccessible* acc = IdToAccessible(aID);
  if (acc) {
    RefPtr<PresShell> presShell = acc->Document()->PresShellPtr();
    nsCOMPtr<nsIContent> content = acc->GetContent();
    nsCoreUtils::ScrollTo(presShell, content, aScrollType);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChildBase::RecvTakeSelection(
    const uint64_t& aID) {
  LocalAccessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->TakeSelection();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChildBase::RecvSetSelected(
    const uint64_t& aID, const bool& aSelect) {
  LocalAccessible* acc = IdToAccessible(aID);
  if (acc) {
    acc->SetSelected(aSelect);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChildBase::RecvVerifyCache(
    const uint64_t& aID, const uint64_t& aCacheDomain, AccAttributes* aFields) {
#ifdef A11Y_LOG
  LocalAccessible* acc = IdToAccessible(aID);
  if (!acc) {
    return IPC_OK();
  }

  RefPtr<AccAttributes> localFields =
      acc->BundleFieldsForCache(aCacheDomain, CacheUpdateType::Update);
  bool mismatches = false;

  for (auto prop : *localFields) {
    if (prop.Value<DeleteEntry>()) {
      if (aFields->HasAttribute(prop.Name())) {
        if (!mismatches) {
          logging::MsgBegin("Mismatch!", "Local and remote values differ");
          logging::AccessibleInfo("", acc);
          mismatches = true;
        }
        nsAutoCString propName;
        prop.Name()->ToUTF8String(propName);
        nsAutoString val;
        aFields->GetAttribute(prop.Name(), val);
        logging::MsgEntry(
            "Remote value for %s should be empty, but instead it is '%s'",
            propName.get(), NS_ConvertUTF16toUTF8(val).get());
      }
      continue;
    }

    nsAutoString localVal;
    prop.ValueAsString(localVal);
    nsAutoString remoteVal;
    aFields->GetAttribute(prop.Name(), remoteVal);
    if (!localVal.Equals(remoteVal)) {
      if (!mismatches) {
        logging::MsgBegin("Mismatch!", "Local and remote values differ");
        logging::AccessibleInfo("", acc);
        mismatches = true;
      }
      nsAutoCString propName;
      prop.Name()->ToUTF8String(propName);
      logging::MsgEntry("Fields differ: %s '%s' != '%s'", propName.get(),
                        NS_ConvertUTF16toUTF8(remoteVal).get(),
                        NS_ConvertUTF16toUTF8(localVal).get());
    }
  }
  if (mismatches) {
    logging::MsgEnd();
  }
#endif  // A11Y_LOG

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChildBase::RecvDoActionAsync(
    const uint64_t& aID, const uint8_t& aIndex) {
  if (LocalAccessible* acc = IdToAccessible(aID)) {
    Unused << acc->DoAction(aIndex);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleChildBase::RecvSetCaretOffset(
    const uint64_t& aID, const int32_t& aOffset) {
  HyperTextAccessible* acc = IdToHyperTextAccessible(aID);
  if (acc && acc->IsTextRole() && acc->IsValidOffset(aOffset)) {
    acc->SetCaretOffset(aOffset);
  }
  return IPC_OK();
}

LocalAccessible* DocAccessibleChildBase::IdToAccessible(
    const uint64_t& aID) const {
  if (!aID) return mDoc;

  if (!mDoc) return nullptr;

  return mDoc->GetAccessibleByUniqueID(reinterpret_cast<void*>(aID));
}

HyperTextAccessible* DocAccessibleChildBase::IdToHyperTextAccessible(
    const uint64_t& aID) const {
  LocalAccessible* acc = IdToAccessible(aID);
  return acc && acc->IsHyperText() ? acc->AsHyperText() : nullptr;
}

}  // namespace a11y
}  // namespace mozilla
