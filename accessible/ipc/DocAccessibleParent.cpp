/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleParent.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/dom/TabParent.h"
#include "xpcAccessibleDocument.h"
#include "xpcAccEvents.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"

namespace mozilla {
namespace a11y {

bool
#if defined(XP_WIN)
DocAccessibleParent::RecvShowEventInfo(const ShowEventData& aData,
                                       nsTArray<MsaaMapping>* aNewMsaaIds)
#else
DocAccessibleParent::RecvShowEvent(const ShowEventData& aData,
                                   const bool& aFromUser)
#endif // defined(XP_WIN)
{
  if (mShutdown)
    return true;

  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());

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

#if defined(XP_WIN)
  aNewMsaaIds->SetCapacity(aData.NewTree().Length());
  uint32_t consumed = AddSubtree(parent, aData.NewTree(), 0, newChildIdx,
                                 aNewMsaaIds);
#else
  uint32_t consumed = AddSubtree(parent, aData.NewTree(), 0, newChildIdx);
#endif
  MOZ_ASSERT(consumed == aData.NewTree().Length());

  // XXX This shouldn't happen, but if we failed to add children then the below
  // is pointless and can crash.
  if (!consumed) {
    return true;
  }

#ifdef DEBUG
  for (uint32_t i = 0; i < consumed; i++) {
    uint64_t id = aData.NewTree()[i].ID();
    MOZ_ASSERT(mAccessibles.GetEntry(id));
  }
#endif

  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());

  // NB: On Windows we dispatch the native event via a subsequent call to
  // RecvEvent().
#if !defined(XP_WIN)
  ProxyAccessible* target = parent->ChildAt(newChildIdx);
  ProxyShowHideEvent(target, parent, true, aFromUser);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return true;
  }

  uint32_t type = nsIAccessibleEvent::EVENT_SHOW;
  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(target);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsIDOMNode* node = nullptr;
  RefPtr<xpcAccEvent> event = new xpcAccEvent(type, xpcAcc, doc, node,
                                              aFromUser);
  nsCoreUtils::DispatchAccEvent(Move(event));
#endif

  return true;
}

uint32_t
DocAccessibleParent::AddSubtree(ProxyAccessible* aParent,
                                const nsTArray<a11y::AccessibleData>& aNewTree,
                                uint32_t aIdx, uint32_t aIdxInParent
#if defined(XP_WIN)
                                , nsTArray<MsaaMapping>* aNewMsaaIds
#endif
                                )
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

#if defined(XP_WIN)
  const IAccessibleHolder& proxyStream = newChild.COMProxy();
  RefPtr<IAccessible> comPtr(proxyStream.Get());
  if (!comPtr) {
    NS_ERROR("Could not obtain remote IAccessible interface");
    return 0;
  }

  ProxyAccessible* newProxy =
    new ProxyAccessible(newChild.ID(), aParent, this, role,
                        newChild.Interfaces(), comPtr);
#else
  ProxyAccessible* newProxy =
    new ProxyAccessible(newChild.ID(), aParent, this, role,
                        newChild.Interfaces());
#endif

  aParent->AddChildAt(aIdxInParent, newProxy);
  mAccessibles.PutEntry(newChild.ID())->mProxy = newProxy;
  ProxyCreated(newProxy, newChild.Interfaces());

#if defined(XP_WIN)
  Accessible* idForAcc = WrapperFor(newProxy);
  MOZ_ASSERT(idForAcc);
  uint32_t newMsaaId = AccessibleWrap::GetChildIDFor(idForAcc);
  MOZ_ASSERT(newMsaaId);
  aNewMsaaIds->AppendElement(MsaaMapping(newChild.ID(), newMsaaId));
#endif // defined(XP_WIN)

  uint32_t accessibles = 1;
  uint32_t kids = newChild.ChildrenCount();
  for (uint32_t i = 0; i < kids; i++) {
    uint32_t consumed = AddSubtree(newProxy, aNewTree, aIdx + accessibles, i
#if defined(XP_WIN)
                                   , aNewMsaaIds
#endif
                                  );
    if (!consumed)
      return 0;

    accessibles += consumed;
  }

  MOZ_ASSERT(newProxy->ChildrenCount() == kids);

  return accessibles;
}

bool
DocAccessibleParent::RecvHideEvent(const uint64_t& aRootID,
                                   const bool& aFromUser)
{
  if (mShutdown)
    return true;

  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());

  // We shouldn't actually need this because mAccessibles shouldn't have an
  // entry for the document itself, but it doesn't hurt to be explicit.
  if (!aRootID) {
    NS_ERROR("trying to hide entire document?");
    return false;
  }

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
  ProxyShowHideEvent(root, parent, false, aFromUser);

  RefPtr<xpcAccHideEvent> event = nullptr;
  if (nsCoreUtils::AccEventObserversExist()) {
    uint32_t type = nsIAccessibleEvent::EVENT_HIDE;
    xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(root);
    xpcAccessibleGeneric* xpcParent = GetXPCAccessible(parent);
    ProxyAccessible* next = root->NextSibling();
    xpcAccessibleGeneric* xpcNext = next ? GetXPCAccessible(next) : nullptr;
    ProxyAccessible* prev = root->PrevSibling();
    xpcAccessibleGeneric* xpcPrev = prev ? GetXPCAccessible(prev) : nullptr;
    xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
    nsIDOMNode* node = nullptr;
    event = new xpcAccHideEvent(type, xpcAcc, doc, node, aFromUser, xpcParent,
                                xpcNext, xpcPrev);
  }

  parent->RemoveChild(root);
  root->Shutdown();

  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());

  if (event) {
    nsCoreUtils::DispatchAccEvent(Move(event));
  }

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

  if (!nsCoreUtils::AccEventObserversExist()) {
    return true;
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(proxy);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsIDOMNode* node = nullptr;
  bool fromUser = true; // XXX fix me
  RefPtr<xpcAccEvent> event = new xpcAccEvent(aEventType, xpcAcc, doc, node,
                                              fromUser);
  nsCoreUtils::DispatchAccEvent(Move(event));

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

  if (!nsCoreUtils::AccEventObserversExist()) {
    return true;
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(target);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  uint32_t type = nsIAccessibleEvent::EVENT_STATE_CHANGE;
  bool extra;
  uint32_t state = nsAccUtils::To32States(aState, &extra);
  bool fromUser = true; // XXX fix this
  nsIDOMNode* node = nullptr; // XXX can we do better?
  RefPtr<xpcAccStateChangeEvent> event =
    new xpcAccStateChangeEvent(type, xpcAcc, doc, node, fromUser, state, extra,
                               aEnabled);
  nsCoreUtils::DispatchAccEvent(Move(event));

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

  if (!nsCoreUtils::AccEventObserversExist()) {
    return true;
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(proxy);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsIDOMNode* node = nullptr;
  bool fromUser = true; // XXX fix me
  uint32_t type = nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED;
  RefPtr<xpcAccCaretMoveEvent> event =
    new xpcAccCaretMoveEvent(type, xpcAcc, doc, node, fromUser, aOffset);
  nsCoreUtils::DispatchAccEvent(Move(event));

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

  if (!nsCoreUtils::AccEventObserversExist()) {
    return true;
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(target);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  uint32_t type = aIsInsert ? nsIAccessibleEvent::EVENT_TEXT_INSERTED :
                              nsIAccessibleEvent::EVENT_TEXT_REMOVED;
  nsIDOMNode* node = nullptr;
  RefPtr<xpcAccTextChangeEvent> event =
    new xpcAccTextChangeEvent(type, xpcAcc, doc, node, aFromUser, aStart, aLen,
                              aIsInsert, aStr);
  nsCoreUtils::DispatchAccEvent(Move(event));

  return true;
}

bool
DocAccessibleParent::RecvSelectionEvent(const uint64_t& aID,
                                        const uint64_t& aWidgetID,
                                        const uint32_t& aType)
{
  ProxyAccessible* target = GetAccessible(aID);
  ProxyAccessible* widget = GetAccessible(aWidgetID);
  if (!target || !widget) {
    NS_ERROR("invalid id in selection event");
    return true;
  }

  ProxySelectionEvent(target, widget, aType);
  if (!nsCoreUtils::AccEventObserversExist()) {
    return true;
  }
  xpcAccessibleGeneric* xpcTarget = GetXPCAccessible(target);
  xpcAccessibleDocument* xpcDoc = GetAccService()->GetXPCDocument(this);
  RefPtr<xpcAccEvent> event = new xpcAccEvent(aType, xpcTarget, xpcDoc,
                                              nullptr, false);
  nsCoreUtils::DispatchAccEvent(Move(event));

  return true;
}

bool
DocAccessibleParent::RecvRoleChangedEvent(const uint32_t& aRole)
{
 if (aRole >= roles::LAST_ROLE) {
   NS_ERROR("child sent bad role in RoleChangedEvent");
   return false;
 }

 mRole = static_cast<a11y::role>(aRole);
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

  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());

  auto childDoc = static_cast<DocAccessibleParent*>(aChildDoc);
  childDoc->Unbind();
  bool result = AddChildDoc(childDoc, aID, false);
  MOZ_ASSERT(result);
  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());
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

  // OuterDocAccessibles are expected to only have a document as a child.
  // However for compatibility we tolerate replacing one document with another
  // here.
  if (outerDoc->ChildrenCount() > 1 ||
      (outerDoc->ChildrenCount() == 1 && !outerDoc->ChildAt(0)->IsDoc())) {
    return false;
  }

  aChildDoc->mParent = outerDoc;
  outerDoc->SetChildDoc(aChildDoc);
  mChildDocs.AppendElement(aChildDoc);
  aChildDoc->mParentDoc = this;

  if (aCreating) {
    ProxyCreated(aChildDoc, Interfaces::DOCUMENT | Interfaces::HYPERTEXT);
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
    MOZ_ASSERT(iter.Get()->mProxy != this);
    ProxyDestroyed(iter.Get()->mProxy);
    iter.Remove();
  }

  DocManager::NotifyOfRemoteDocShutdown(this);
  ProxyDestroyed(this);
  if (mParentDoc)
    mParentDoc->RemoveChildDoc(this);
  else if (IsTopLevel())
    GetAccService()->RemoteDocShutdown(this);
}

bool
DocAccessibleParent::CheckDocTree() const
{
  size_t childDocs = mChildDocs.Length();
  for (size_t i = 0; i < childDocs; i++) {
    if (!mChildDocs[i] || mChildDocs[i]->mParentDoc != this)
      return false;

    if (!mChildDocs[i]->CheckDocTree()) {
      return false;
    }
  }

  return true;
}

xpcAccessibleGeneric*
DocAccessibleParent::GetXPCAccessible(ProxyAccessible* aProxy)
{
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  MOZ_ASSERT(doc);

  return doc->GetXPCAccessible(aProxy);
}

#if defined(XP_WIN)
/**
 * @param aCOMProxy COM Proxy to the document in the content process.
 * @param aParentCOMProxy COM Proxy to the OuterDocAccessible that is
 *        the parent of the document. The content process will use this
 *        proxy when traversing up across the content/chrome boundary.
 */
bool
DocAccessibleParent::RecvCOMProxy(const IAccessibleHolder& aCOMProxy,
                                  IAccessibleHolder* aParentCOMProxy,
                                  uint32_t* aMsaaID)
{
  RefPtr<IAccessible> ptr(aCOMProxy.Get());
  SetCOMInterface(ptr);

  Accessible* outerDoc = OuterDocOfRemoteBrowser();
  IAccessible* rawNative = nullptr;
  if (outerDoc) {
    outerDoc->GetNativeInterface((void**) &rawNative);
  }

  aParentCOMProxy->Set(IAccessibleHolder::COMPtrType(rawNative));
  Accessible* wrapper = WrapperFor(this);
  *aMsaaID = AccessibleWrap::GetChildIDFor(wrapper);
  return true;
}
#endif // defined(XP_WIN)

} // a11y
} // mozilla
