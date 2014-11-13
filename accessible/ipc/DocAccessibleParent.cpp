/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleParent.h"
#include "nsAutoPtr.h"
#include "mozilla/a11y/Platform.h"

namespace mozilla {
namespace a11y {

bool
DocAccessibleParent::RecvShowEvent(const ShowEventData& aData)
{
  if (aData.NewTree().IsEmpty()) {
    NS_ERROR("no children being added");
    return false;
  }

  ProxyAccessible* parent = nullptr;
  if (aData.ID()) {
    ProxyEntry* e = mAccessibles.GetEntry(aData.ID());
    if (e)
      parent = e->mProxy;
  } else {
    parent = this;
  }

  // XXX This should really never happen, but sometimes we fail to fire the
  // required show events.
  if (!parent) {
    NS_ERROR("adding child to unknown accessible");
    return false;
  }

  uint32_t newChildIdx = aData.Idx();
  if (newChildIdx > parent->ChildrenCount()) {
    NS_ERROR("invalid index to add child at");
    return false;
  }

  uint32_t consumed = AddSubtree(parent, aData.NewTree(), 0, newChildIdx);
  MOZ_ASSERT(consumed == aData.NewTree().Length());
#ifdef DEBUG
  for (uint32_t i = 0; i < consumed; i++) {
    uint64_t id = aData.NewTree()[i].ID();
    MOZ_ASSERT(mAccessibles.GetEntry(id));
  }
#endif

  return consumed;
}

uint32_t
DocAccessibleParent::AddSubtree(ProxyAccessible* aParent,
                                const nsTArray<a11y::AccessibleData>& aNewTree,
                                uint32_t aIdx, uint32_t aIdxInParent)
{
  if (aNewTree.Length() <= aIdx) {
    NS_ERROR("bad index in serialized tree!");
    return 0;
  }

  const AccessibleData& newChild = aNewTree[aIdx];
  if (newChild.Role() > roles::LAST_ROLE) {
    NS_ERROR("invalid role");
    return 0;
  }

  auto role = static_cast<a11y::role>(newChild.Role());
  ProxyAccessible* newProxy =
    new ProxyAccessible(newChild.ID(), aParent, this, role);
  aParent->AddChildAt(aIdxInParent, newProxy);
  mAccessibles.PutEntry(newChild.ID())->mProxy = newProxy;
  ProxyCreated(newProxy);

  uint32_t accessibles = 1;
  uint32_t kids = newChild.ChildrenCount();
  for (uint32_t i = 0; i < kids; i++) {
    uint32_t consumed = AddSubtree(newProxy, aNewTree, aIdx + accessibles, i);
    if (!consumed)
      return 0;

    accessibles += consumed;
  }

  MOZ_ASSERT(newProxy->ChildrenCount() == kids);

  return accessibles;
}

bool
DocAccessibleParent::RecvHideEvent(const uint64_t& aRootID)
{
  ProxyEntry* rootEntry = mAccessibles.GetEntry(aRootID);
  if (!rootEntry) {
    NS_ERROR("invalid root being removed!");
    return true;
  }

  ProxyAccessible* root = rootEntry->mProxy;
  if (!root) {
    NS_ERROR("invalid root being removed!");
    return true;
  }

  ProxyAccessible* parent = root->Parent();
  parent->RemoveChild(root);
  root->Shutdown();

  return true;
}

bool
DocAccessibleParent::RecvEvent(const uint64_t& aID, const uint32_t& aEventType)
{
  if (!aID) {
    ProxyEvent(this, aEventType);
    return true;
  }

  ProxyEntry* e = mAccessibles.GetEntry(aID);
  if (!e) {
    NS_ERROR("no proxy for event!");
    return true;
  }

  ProxyEvent(e->mProxy, aEventType);
  return true;
}
}
}
