/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/DocAccessibleChildBase.h"
#include "mozilla/a11y/ProxyAccessible.h"

#include "Accessible-inl.h"

namespace mozilla {
namespace a11y {

/* static */ uint32_t
DocAccessibleChildBase::InterfacesFor(Accessible* aAcc)
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

  if (aAcc->IsTable()) {
    interfaces |= Interfaces::TABLE;
  }

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

/* static */ void
DocAccessibleChildBase::SerializeTree(Accessible* aRoot,
                                      nsTArray<AccessibleData>& aTree)
{
  uint64_t id = reinterpret_cast<uint64_t>(aRoot->UniqueID());
  uint32_t role = aRoot->Role();
  uint32_t childCount = aRoot->ChildCount();
  uint32_t interfaces = InterfacesFor(aRoot);

#if defined(XP_WIN)
  IAccessibleHolder holder(CreateHolderFromAccessible(aRoot));
#endif

  // OuterDocAccessibles are special because we don't want to serialize the
  // child doc here, we'll call PDocAccessibleConstructor in
  // NotificationController.
  MOZ_ASSERT(!aRoot->IsDoc(), "documents shouldn't be serialized");
  if (aRoot->IsOuterDoc()) {
    childCount = 0;
  }

#if defined(XP_WIN)
  aTree.AppendElement(AccessibleData(id, role, childCount, interfaces,
                                     holder));
#else
  aTree.AppendElement(AccessibleData(id, role, childCount, interfaces));
#endif

  for (uint32_t i = 0; i < childCount; i++) {
    SerializeTree(aRoot->GetChildAt(i), aTree);
  }
}

#if defined(XP_WIN)
/* static */ void
DocAccessibleChildBase::SetMsaaIds(Accessible* aRoot,
                                   uint32_t& aMsaaIdIndex,
                                   const nsTArray<MsaaMapping>& aNewMsaaIds)
{
  const MsaaMapping& mapping = aNewMsaaIds[aMsaaIdIndex];
#if defined(DEBUG)
  uint64_t id = reinterpret_cast<uint64_t>(aRoot->UniqueID());
  MOZ_ASSERT(mapping.ID() == id);
#endif // defined(DEBUG)
  static_cast<AccessibleWrap*>(aRoot)->SetID(mapping.MsaaID());
  ++aMsaaIdIndex;
  if (aRoot->IsOuterDoc()) {
    // This needs to match the tree traversal in SerializeTree
    return;
  }
  for (uint32_t i = 0, n = aRoot->ChildCount(); i < n; ++i) {
    SetMsaaIds(aRoot->GetChildAt(i), aMsaaIdIndex, aNewMsaaIds);
  }
}
#endif // defined(XP_WIN)

void
DocAccessibleChildBase::ShowEvent(AccShowEvent* aShowEvent)
{
  Accessible* parent = aShowEvent->Parent();
  uint64_t parentID = parent->IsDoc() ? 0 : reinterpret_cast<uint64_t>(parent->UniqueID());
  uint32_t idxInParent = aShowEvent->InsertionIndex();
  nsTArray<AccessibleData> shownTree;
  ShowEventData data(parentID, idxInParent, shownTree);
  SerializeTree(aShowEvent->GetAccessible(), data.NewTree());
#if defined(XP_WIN)
  nsTArray<MsaaMapping> newMsaaIds;
  SendShowEventInfo(data, &newMsaaIds);
  // newMsaaIds could be empty if something went wrong in SendShowEvent()
  if (!newMsaaIds.IsEmpty()) {
    uint32_t index = 0;
    SetMsaaIds(aShowEvent->GetAccessible(), index, newMsaaIds);
  }
  // NB: On Windows, SendShowEvent attaches the subtree and generates new IDs,
  //     but does *NOT* fire the native event. We need to do that after
  //     we've called SetMsaaIds.
  SendEvent(reinterpret_cast<uint64_t>(aShowEvent->GetAccessible()->UniqueID()),
            nsIAccessibleEvent::EVENT_SHOW);
#else
  SendShowEvent(data, aShowEvent->IsFromUserInput());
#endif // defined(XP_WIN)
}

} // namespace a11y
} // namespace mozilla

