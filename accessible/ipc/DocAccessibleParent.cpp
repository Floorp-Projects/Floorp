/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleParent.h"
#include "nsAutoPtr.h"
#include "mozilla/a11y/Platform.h"
#include "ProxyAccessible.h"
#include "mozilla/dom/TabParent.h"

namespace mozilla {
namespace a11y {

bool
DocAccessibleParent::RecvShowEvent(const ShowEventData& aData)
{
  if (mShutdown)
    return true;

  CheckDocTree();

  if (aData.NewTree().IsEmpty()) {
    NS_ERROR("no children being added");
    return false;
  }

  ProxyAccessible* parent = GetAccessible(aData.ID());

  // XXX This should really never happen, but sometimes we fail to fire the
  // required show events.
  if (!parent) {
    NS_ERROR("adding child to unknown accessible");
    return true;
  }

  uint32_t newChildIdx = aData.Idx();
  if (newChildIdx > parent->ChildrenCount()) {
    NS_ERROR("invalid index to add child at");
    return true;
  }

  DebugOnly<uint32_t> consumed = AddSubtree(parent, aData.NewTree(), 0, newChildIdx);
  MOZ_ASSERT(consumed == aData.NewTree().Length());
#ifdef DEBUG
  for (uint32_t i = 0; i < consumed; i++) {
    uint64_t id = aData.NewTree()[i].ID();
    MOZ_ASSERT(mAccessibles.GetEntry(id));
  }
#endif

  CheckDocTree();

  return true;
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

  if (mAccessibles.Contains(newChild.ID())) {
    NS_ERROR("ID already in use");
    return 0;
  }

  auto role = static_cast<a11y::role>(newChild.Role());
  ProxyAccessible* newProxy =
    new ProxyAccessible(newChild.ID(), aParent, this, role);
  aParent->AddChildAt(aIdxInParent, newProxy);
  mAccessibles.PutEntry(newChild.ID())->mProxy = newProxy;
  ProxyCreated(newProxy, newChild.Interfaces());

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
  if (mShutdown)
    return true;

  CheckDocTree();

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

  CheckDocTree();

  return true;
}

bool
DocAccessibleParent::RecvEvent(const uint64_t& aID, const uint32_t& aEventType)
{
  ProxyAccessible* proxy = GetAccessible(aID);
  if (!proxy) {
    NS_ERROR("no proxy for event!");
    return true;
  }

  ProxyEvent(proxy, aEventType);
  return true;
}

bool
DocAccessibleParent::RecvStateChangeEvent(const uint64_t& aID,
                                          const uint64_t& aState,
                                          const bool& aEnabled)
{
  ProxyAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("we don't know about the target of a state change event!");
    return true;
  }

  ProxyStateChangeEvent(target, aState, aEnabled);
  return true;
}

bool
DocAccessibleParent::RecvCaretMoveEvent(const uint64_t& aID, const int32_t& aOffset)
{
  ProxyAccessible* proxy = GetAccessible(aID);
  if (!proxy) {
    NS_ERROR("unknown caret move event target!");
    return true;
  }

  ProxyCaretMoveEvent(proxy, aOffset);
  return true;
}

bool
DocAccessibleParent::RecvTextChangeEvent(const uint64_t& aID,
                                         const nsString& aStr,
                                         const int32_t& aStart,
                                         const uint32_t& aLen,
                                         const bool& aIsInsert,
                                         const bool& aFromUser)
{
  ProxyAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("text change event target is unknown!");
    return true;
  }

  ProxyTextChangeEvent(target, aStr, aStart, aLen, aIsInsert, aFromUser);

  return true;
}

bool
DocAccessibleParent::RecvBindChildDoc(PDocAccessibleParent* aChildDoc, const uint64_t& aID)
{
  // One document should never directly be the child of another.
  // We should always have at least an outer doc accessible in between.
  MOZ_ASSERT(aID);
  if (!aID)
    return false;

  CheckDocTree();

  auto childDoc = static_cast<DocAccessibleParent*>(aChildDoc);
  bool result = AddChildDoc(childDoc, aID, false);
  MOZ_ASSERT(result);
  CheckDocTree();
  return result;
}

bool
DocAccessibleParent::AddChildDoc(DocAccessibleParent* aChildDoc,
                                 uint64_t aParentID, bool aCreating)
{
  // We do not use GetAccessible here because we want to be sure to not get the
  // document it self.
  ProxyEntry* e = mAccessibles.GetEntry(aParentID);
  if (!e)
    return false;

  ProxyAccessible* outerDoc = e->mProxy;
  MOZ_ASSERT(outerDoc);

  aChildDoc->mParent = outerDoc;
  outerDoc->SetChildDoc(aChildDoc);
  mChildDocs.AppendElement(aChildDoc);
  aChildDoc->mParentDoc = this;

  if (aCreating) {
    ProxyCreated(aChildDoc, 0);
  }

  return true;
}

bool
DocAccessibleParent::RecvShutdown()
{
  Destroy();

  if (!static_cast<dom::TabParent*>(Manager())->IsDestroyed()) {
  return PDocAccessibleParent::Send__delete__(this);
  }

  return true;
}

void
DocAccessibleParent::Destroy()
{
  NS_ASSERTION(mChildDocs.IsEmpty(),
               "why weren't the child docs destroyed already?");
  MOZ_ASSERT(!mShutdown);
  mShutdown = true;

  uint32_t childDocCount = mChildDocs.Length();
  for (uint32_t i = childDocCount - 1; i < childDocCount; i--)
    mChildDocs[i]->Destroy();

  for (auto iter = mAccessibles.Iter(); !iter.Done(); iter.Next()) {
    ProxyDestroyed(iter.Get()->mProxy);
    iter.Remove();
  }
  ProxyDestroyed(this);
  if (mParentDoc)
    mParentDoc->RemoveChildDoc(this);
  else if (IsTopLevel())
    GetAccService()->RemoteDocShutdown(this);
}

void
DocAccessibleParent::CheckDocTree() const
{
  size_t childDocs = mChildDocs.Length();
  for (size_t i = 0; i < childDocs; i++) {
    if (!mChildDocs[i] || mChildDocs[i]->mParentDoc != this)
      MOZ_CRASH("document tree is broken!");

    mChildDocs[i]->CheckDocTree();
  }
}

} // a11y
} // mozilla
