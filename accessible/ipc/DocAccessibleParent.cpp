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

#if defined(XP_WIN)
#include "AccessibleWrap.h"
#include "Compatibility.h"
#include "nsWinUtils.h"
#include "RootAccessible.h"
#endif

namespace mozilla {
namespace a11y {

mozilla::ipc::IPCResult
DocAccessibleParent::RecvShowEvent(const ShowEventData& aData,
                                   const bool& aFromUser)
{
  if (mShutdown)
    return IPC_OK();

  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());

  if (aData.NewTree().IsEmpty()) {
    NS_ERROR("no children being added");
    return IPC_FAIL_NO_REASON(this);
  }

  ProxyAccessible* parent = GetAccessible(aData.ID());

  // XXX This should really never happen, but sometimes we fail to fire the
  // required show events.
  if (!parent) {
    NS_ERROR("adding child to unknown accessible");
#ifdef DEBUG
    return IPC_FAIL(this, "unknown parent accessible");
#else
    return IPC_OK();
#endif
  }

  uint32_t newChildIdx = aData.Idx();
  if (newChildIdx > parent->ChildrenCount()) {
    NS_ERROR("invalid index to add child at");
#ifdef DEBUG
    return IPC_FAIL(this, "invalid index");
#else
    return IPC_OK();
#endif
  }

  uint32_t consumed = AddSubtree(parent, aData.NewTree(), 0, newChildIdx);
  MOZ_ASSERT(consumed == aData.NewTree().Length());

  // XXX This shouldn't happen, but if we failed to add children then the below
  // is pointless and can crash.
  if (!consumed) {
    return IPC_FAIL(this, "failed to add children");
  }

#ifdef DEBUG
  for (uint32_t i = 0; i < consumed; i++) {
    uint64_t id = aData.NewTree()[i].ID();
    MOZ_ASSERT(mAccessibles.GetEntry(id));
  }
#endif

  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());

  ProxyAccessible* target = parent->ChildAt(newChildIdx);
  ProxyShowHideEvent(target, parent, true, aFromUser);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  uint32_t type = nsIAccessibleEvent::EVENT_SHOW;
  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(target);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsIDOMNode* node = nullptr;
  RefPtr<xpcAccEvent> event = new xpcAccEvent(type, xpcAcc, doc, node,
                                              aFromUser);
  nsCoreUtils::DispatchAccEvent(Move(event));

  return IPC_OK();
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
    new ProxyAccessible(newChild.ID(), aParent, this, role,
                        newChild.Interfaces());

  aParent->AddChildAt(aIdxInParent, newProxy);
  mAccessibles.PutEntry(newChild.ID())->mProxy = newProxy;
  ProxyCreated(newProxy, newChild.Interfaces());

#if defined(XP_WIN)
  WrapperFor(newProxy)->SetID(newChild.MsaaID());
#endif

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

mozilla::ipc::IPCResult
DocAccessibleParent::RecvHideEvent(const uint64_t& aRootID,
                                   const bool& aFromUser)
{
  if (mShutdown)
    return IPC_OK();

  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());

  // We shouldn't actually need this because mAccessibles shouldn't have an
  // entry for the document itself, but it doesn't hurt to be explicit.
  if (!aRootID) {
    NS_ERROR("trying to hide entire document?");
    return IPC_FAIL_NO_REASON(this);
  }

  ProxyEntry* rootEntry = mAccessibles.GetEntry(aRootID);
  if (!rootEntry) {
    NS_ERROR("invalid root being removed!");
    return IPC_OK();
  }

  ProxyAccessible* root = rootEntry->mProxy;
  if (!root) {
    NS_ERROR("invalid root being removed!");
    return IPC_OK();
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

  return IPC_OK();
}

mozilla::ipc::IPCResult
DocAccessibleParent::RecvEvent(const uint64_t& aID, const uint32_t& aEventType)
{
  if (mShutdown) {
    return IPC_OK();
  }

  ProxyAccessible* proxy = GetAccessible(aID);
  if (!proxy) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

  ProxyEvent(proxy, aEventType);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(proxy);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsIDOMNode* node = nullptr;
  bool fromUser = true; // XXX fix me
  RefPtr<xpcAccEvent> event = new xpcAccEvent(aEventType, xpcAcc, doc, node,
                                              fromUser);
  nsCoreUtils::DispatchAccEvent(Move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult
DocAccessibleParent::RecvStateChangeEvent(const uint64_t& aID,
                                          const uint64_t& aState,
                                          const bool& aEnabled)
{
  if (mShutdown) {
    return IPC_OK();
  }

  ProxyAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("we don't know about the target of a state change event!");
    return IPC_OK();
  }

  ProxyStateChangeEvent(target, aState, aEnabled);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
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

  return IPC_OK();
}

mozilla::ipc::IPCResult
DocAccessibleParent::RecvCaretMoveEvent(const uint64_t& aID, const int32_t& aOffset)
{
  if (mShutdown) {
    return IPC_OK();
  }

  ProxyAccessible* proxy = GetAccessible(aID);
  if (!proxy) {
    NS_ERROR("unknown caret move event target!");
    return IPC_OK();
  }

  ProxyCaretMoveEvent(proxy, aOffset);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(proxy);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsIDOMNode* node = nullptr;
  bool fromUser = true; // XXX fix me
  uint32_t type = nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED;
  RefPtr<xpcAccCaretMoveEvent> event =
    new xpcAccCaretMoveEvent(type, xpcAcc, doc, node, fromUser, aOffset);
  nsCoreUtils::DispatchAccEvent(Move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult
DocAccessibleParent::RecvTextChangeEvent(const uint64_t& aID,
                                         const nsString& aStr,
                                         const int32_t& aStart,
                                         const uint32_t& aLen,
                                         const bool& aIsInsert,
                                         const bool& aFromUser)
{
  if (mShutdown) {
    return IPC_OK();
  }

  ProxyAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("text change event target is unknown!");
    return IPC_OK();
  }

  ProxyTextChangeEvent(target, aStr, aStart, aLen, aIsInsert, aFromUser);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
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

  return IPC_OK();
}

mozilla::ipc::IPCResult
DocAccessibleParent::RecvSelectionEvent(const uint64_t& aID,
                                        const uint64_t& aWidgetID,
                                        const uint32_t& aType)
{
  if (mShutdown) {
    return IPC_OK();
  }

  ProxyAccessible* target = GetAccessible(aID);
  ProxyAccessible* widget = GetAccessible(aWidgetID);
  if (!target || !widget) {
    NS_ERROR("invalid id in selection event");
    return IPC_OK();
  }

  ProxySelectionEvent(target, widget, aType);
  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }
  xpcAccessibleGeneric* xpcTarget = GetXPCAccessible(target);
  xpcAccessibleDocument* xpcDoc = GetAccService()->GetXPCDocument(this);
  RefPtr<xpcAccEvent> event = new xpcAccEvent(aType, xpcTarget, xpcDoc,
                                              nullptr, false);
  nsCoreUtils::DispatchAccEvent(Move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult
DocAccessibleParent::RecvRoleChangedEvent(const uint32_t& aRole)
{
  if (mShutdown) {
    return IPC_OK();
  }

 if (aRole >= roles::LAST_ROLE) {
   NS_ERROR("child sent bad role in RoleChangedEvent");
   return IPC_FAIL_NO_REASON(this);
 }

 mRole = static_cast<a11y::role>(aRole);
 return IPC_OK();
}

mozilla::ipc::IPCResult
DocAccessibleParent::RecvBindChildDoc(PDocAccessibleParent* aChildDoc, const uint64_t& aID)
{
  // One document should never directly be the child of another.
  // We should always have at least an outer doc accessible in between.
  MOZ_ASSERT(aID);
  if (!aID)
    return IPC_FAIL(this, "ID is 0!");

  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());

  auto childDoc = static_cast<DocAccessibleParent*>(aChildDoc);
  childDoc->Unbind();
  ipc::IPCResult result = AddChildDoc(childDoc, aID, false);
  MOZ_ASSERT(result);
  MOZ_DIAGNOSTIC_ASSERT(CheckDocTree());
  if (!result) {
    return result;
  }
  return IPC_OK();
}

ipc::IPCResult
DocAccessibleParent::AddChildDoc(DocAccessibleParent* aChildDoc,
                                 uint64_t aParentID, bool aCreating)
{
  // We do not use GetAccessible here because we want to be sure to not get the
  // document it self.
  ProxyEntry* e = mAccessibles.GetEntry(aParentID);
  if (!e)
    return IPC_FAIL(this, "binding to nonexistant proxy!");

  ProxyAccessible* outerDoc = e->mProxy;
  MOZ_ASSERT(outerDoc);

  // OuterDocAccessibles are expected to only have a document as a child.
  // However for compatibility we tolerate replacing one document with another
  // here.
  if (outerDoc->ChildrenCount() > 1 ||
      (outerDoc->ChildrenCount() == 1 && !outerDoc->ChildAt(0)->IsDoc())) {
    return IPC_FAIL(this, "binding to proxy that can't be a outerDoc!");
  }

  aChildDoc->mParent = outerDoc;
  outerDoc->SetChildDoc(aChildDoc);
  mChildDocs.AppendElement(aChildDoc);
  aChildDoc->mParentDoc = this;

  if (aCreating) {
    ProxyCreated(aChildDoc, Interfaces::DOCUMENT | Interfaces::HYPERTEXT);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
DocAccessibleParent::RecvShutdown()
{
  Destroy();

  auto mgr = static_cast<dom::TabParent*>(Manager());
  if (!mgr->IsDestroyed()) {
    if (!PDocAccessibleParent::Send__delete__(this)) {
      return IPC_FAIL_NO_REASON(mgr);
    }
  }

  return IPC_OK();
}

void
DocAccessibleParent::Destroy()
{
  // If we are already shutdown that is because our containing tab parent is
  // shutting down in which case we don't need to do anything.
  if (mShutdown) {
    return;
  }

  mShutdown = true;

  uint32_t childDocCount = mChildDocs.Length();
  for (uint32_t i = 0; i < childDocCount; i++) {
    for (uint32_t j = i + 1; j < childDocCount; j++) {
      MOZ_DIAGNOSTIC_ASSERT(mChildDocs[i] != mChildDocs[j]);
    }
  }

  for (uint32_t i = childDocCount - 1; i < childDocCount; i--)
    mChildDocs[i]->Destroy();

  for (auto iter = mAccessibles.Iter(); !iter.Done(); iter.Next()) {
    MOZ_ASSERT(iter.Get()->mProxy != this);
    ProxyDestroyed(iter.Get()->mProxy);
    iter.Remove();
  }

  // The code above should have already completely cleared these, but to be
  // extra safe make sure they are cleared here.
  mAccessibles.Clear();
  mChildDocs.Clear();

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
 */
void
DocAccessibleParent::SetCOMProxy(const RefPtr<IAccessible>& aCOMProxy)
{
  SetCOMInterface(aCOMProxy);

  // Make sure that we're not racing with a tab shutdown
  auto tab = static_cast<dom::TabParent*>(Manager());
  MOZ_ASSERT(tab);
  if (tab->IsDestroyed()) {
    return;
  }

  Accessible* outerDoc = OuterDocOfRemoteBrowser();
  MOZ_ASSERT(outerDoc);

  IAccessible* rawNative = nullptr;
  if (outerDoc) {
    outerDoc->GetNativeInterface((void**) &rawNative);
    MOZ_ASSERT(rawNative);
  }

  IAccessibleHolder::COMPtrType ptr(rawNative);
  IAccessibleHolder holder(Move(ptr));

  if (nsWinUtils::IsWindowEmulationStarted()) {
    RootAccessible* rootDocument = outerDoc->RootAccessible();
    MOZ_ASSERT(rootDocument);

    bool isActive = true;
    nsIntRect rect(CW_USEDEFAULT, CW_USEDEFAULT, 0, 0);
    if (Compatibility::IsDolphin()) {
      rect = Bounds();
      nsIntRect rootRect = rootDocument->Bounds();
      rect.x = rootRect.x - rect.x;
      rect.y -= rootRect.y;
      tab->GetDocShellIsActive(&isActive);
    }

    HWND parentWnd = reinterpret_cast<HWND>(rootDocument->GetNativeWindow());
    HWND hWnd = nsWinUtils::CreateNativeWindow(kClassNameTabContent,
                                               parentWnd, rect.x, rect.y,
                                               rect.width, rect.height,
                                               isActive);
    if (hWnd) {
      // Attach accessible document to the emulated native window
      ::SetPropW(hWnd, kPropNameDocAccParent, (HANDLE)this);
      SetEmulatedWindowHandle(hWnd);
    }
  }
  Unused << SendParentCOMProxy(holder, reinterpret_cast<uintptr_t>(
                               mEmulatedWindowHandle));
}

void
DocAccessibleParent::SetEmulatedWindowHandle(HWND aWindowHandle)
{
  if (!aWindowHandle && mEmulatedWindowHandle && IsTopLevel()) {
    ::DestroyWindow(mEmulatedWindowHandle);
  }
  mEmulatedWindowHandle = aWindowHandle;
}

mozilla::ipc::IPCResult
DocAccessibleParent::RecvGetWindowedPluginIAccessible(
      const WindowsHandle& aHwnd, IAccessibleHolder* aPluginCOMProxy)
{
#if defined(MOZ_CONTENT_SANDBOX)
  // We don't actually want the accessible object for aHwnd, but rather the
  // one that belongs to its child (see HTMLWin32ObjectAccessible).
  HWND childWnd = ::GetWindow(reinterpret_cast<HWND>(aHwnd), GW_CHILD);
  if (!childWnd) {
    // We're seeing this in the wild - the plugin is windowed but we no longer
    // have a window.
    return IPC_OK();
  }

  IAccessible* rawAccPlugin = nullptr;
  HRESULT hr = ::AccessibleObjectFromWindow(childWnd, OBJID_WINDOW,
                                            IID_IAccessible,
                                            (void**)&rawAccPlugin);
  if (FAILED(hr)) {
    // This might happen if the plugin doesn't handle WM_GETOBJECT properly.
    // We should not consider that a failure.
    return IPC_OK();
  }

  aPluginCOMProxy->Set(IAccessibleHolder::COMPtrType(rawAccPlugin));

  return IPC_OK();
#else
  return IPC_FAIL(this, "Message unsupported in this build configuration");
#endif
}

#endif // defined(XP_WIN)

} // a11y
} // mozilla
