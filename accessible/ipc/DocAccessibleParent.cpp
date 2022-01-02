/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleParent.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "xpcAccessibleDocument.h"
#include "xpcAccEvents.h"
#include "nsAccUtils.h"
#include "TextRange.h"

#if defined(XP_WIN)
#  include "AccessibleWrap.h"
#  include "Compatibility.h"
#  include "mozilla/mscom/PassthruProxy.h"
#  include "mozilla/mscom/Ptr.h"
#  include "nsWinUtils.h"
#  include "RootAccessible.h"
#else
#  include "mozilla/a11y/DocAccessiblePlatformExtParent.h"
#endif

namespace mozilla {

#if defined(XP_WIN)
namespace mscom {
namespace detail {
// Needed by mscom::PassthruProxy::Wrap<IAccessible>.
template <>
struct VTableSizer<IAccessible> {
  // 3 methods in IUnknown + 4 in IDispatch + 21 in IAccessible = 28 total
  enum { Size = 28 };
};
}  // namespace detail
}  // namespace mscom
#endif  // defined (XP_WIN)

namespace a11y {
uint64_t DocAccessibleParent::sMaxDocID = 0;

mozilla::ipc::IPCResult DocAccessibleParent::RecvShowEvent(
    const ShowEventData& aData, const bool& aFromUser) {
  if (mShutdown) return IPC_OK();

  MOZ_ASSERT(CheckDocTree());

  if (aData.NewTree().IsEmpty()) {
    return IPC_FAIL(this, "No children being added");
  }

  RemoteAccessible* parent = GetAccessible(aData.ID());

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
  if (newChildIdx > parent->ChildCount()) {
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

  MOZ_ASSERT(CheckDocTree());

  // Just update, no events.
  if (aData.EventSuppressed()) {
    return IPC_OK();
  }

  RemoteAccessible* target = parent->RemoteChildAt(newChildIdx);
  ProxyShowHideEvent(target, parent, true, aFromUser);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  uint32_t type = nsIAccessibleEvent::EVENT_SHOW;
  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(target);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsINode* node = nullptr;
  RefPtr<xpcAccEvent> event =
      new xpcAccEvent(type, xpcAcc, doc, node, aFromUser);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

uint32_t DocAccessibleParent::AddSubtree(
    RemoteAccessible* aParent, const nsTArray<a11y::AccessibleData>& aNewTree,
    uint32_t aIdx, uint32_t aIdxInParent) {
  if (aNewTree.Length() <= aIdx) {
    NS_ERROR("bad index in serialized tree!");
    return 0;
  }

  const AccessibleData& newChild = aNewTree[aIdx];

  if (mAccessibles.Contains(newChild.ID())) {
    NS_ERROR("ID already in use");
    return 0;
  }

  RemoteAccessible* newProxy = new RemoteAccessible(
      newChild.ID(), aParent, this, newChild.Role(), newChild.Type(),
      newChild.GenericTypes(), newChild.RoleMapEntryIndex());

  aParent->AddChildAt(aIdxInParent, newProxy);
  mAccessibles.PutEntry(newChild.ID())->mProxy = newProxy;
  ProxyCreated(newProxy);

#if defined(XP_WIN)
  if (!StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    MsaaAccessible::GetFrom(newProxy)->SetID(newChild.MsaaID());
  }
#endif

  mPendingOOPChildDocs.RemoveIf([&](dom::BrowserBridgeParent* bridge) {
    MOZ_ASSERT(bridge->GetBrowserParent(),
               "Pending BrowserBridgeParent should be alive");
    if (bridge->GetEmbedderAccessibleId() != newChild.ID()) {
      return false;
    }
    MOZ_ASSERT(bridge->GetEmbedderAccessibleDoc() == this);
    if (DocAccessibleParent* childDoc = bridge->GetDocAccessibleParent()) {
      AddChildDoc(childDoc, newChild.ID(), false);
    }
    return true;
  });
  DebugOnly<bool> isOuterDoc = newProxy->ChildCount() == 1;

  uint32_t accessibles = 1;
  uint32_t kids = newChild.ChildrenCount();
  for (uint32_t i = 0; i < kids; i++) {
    uint32_t consumed = AddSubtree(newProxy, aNewTree, aIdx + accessibles, i);
    if (!consumed) return 0;

    accessibles += consumed;
  }

  MOZ_ASSERT((isOuterDoc && kids == 0) || newProxy->ChildCount() == kids);

  return accessibles;
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvHideEvent(
    const uint64_t& aRootID, const bool& aFromUser) {
  if (mShutdown) return IPC_OK();

  MOZ_ASSERT(CheckDocTree());

  // We shouldn't actually need this because mAccessibles shouldn't have an
  // entry for the document itself, but it doesn't hurt to be explicit.
  if (!aRootID) {
    return IPC_FAIL(this, "Trying to hide entire document?");
  }

  ProxyEntry* rootEntry = mAccessibles.GetEntry(aRootID);
  if (!rootEntry) {
    NS_ERROR("invalid root being removed!");
    return IPC_OK();
  }

  RemoteAccessible* root = rootEntry->mProxy;
  if (!root) {
    NS_ERROR("invalid root being removed!");
    return IPC_OK();
  }

  RemoteAccessible* parent = root->RemoteParent();
  ProxyShowHideEvent(root, parent, false, aFromUser);

  RefPtr<xpcAccHideEvent> event = nullptr;
  if (nsCoreUtils::AccEventObserversExist()) {
    uint32_t type = nsIAccessibleEvent::EVENT_HIDE;
    xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(root);
    xpcAccessibleGeneric* xpcParent = GetXPCAccessible(parent);
    RemoteAccessible* next = root->RemoteNextSibling();
    xpcAccessibleGeneric* xpcNext = next ? GetXPCAccessible(next) : nullptr;
    RemoteAccessible* prev = root->RemotePrevSibling();
    xpcAccessibleGeneric* xpcPrev = prev ? GetXPCAccessible(prev) : nullptr;
    xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
    nsINode* node = nullptr;
    event = new xpcAccHideEvent(type, xpcAcc, doc, node, aFromUser, xpcParent,
                                xpcNext, xpcPrev);
  }

  parent->RemoveChild(root);
  root->Shutdown();

  MOZ_ASSERT(CheckDocTree());

  if (event) {
    nsCoreUtils::DispatchAccEvent(std::move(event));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvEvent(
    const uint64_t& aID, const uint32_t& aEventType) {
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* proxy = GetAccessible(aID);
  if (!proxy) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

  if (aEventType == nsIAccessibleEvent::EVENT_FOCUS) {
    mFocus = aID;
  }
  ProxyEvent(proxy, aEventType);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(proxy);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsINode* node = nullptr;
  bool fromUser = true;  // XXX fix me
  RefPtr<xpcAccEvent> event =
      new xpcAccEvent(aEventType, xpcAcc, doc, node, fromUser);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvStateChangeEvent(
    const uint64_t& aID, const uint64_t& aState, const bool& aEnabled) {
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("we don't know about the target of a state change event!");
    return IPC_OK();
  }

  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    target->UpdateStateCache(aState, aEnabled);
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
  bool fromUser = true;     // XXX fix this
  nsINode* node = nullptr;  // XXX can we do better?
  RefPtr<xpcAccStateChangeEvent> event = new xpcAccStateChangeEvent(
      type, xpcAcc, doc, node, fromUser, state, extra, aEnabled);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvCaretMoveEvent(
    const uint64_t& aID,
#if defined(XP_WIN)
    const LayoutDeviceIntRect& aCaretRect,
#endif  // defined (XP_WIN)
    const int32_t& aOffset, const bool& aIsSelectionCollapsed,
    const bool& aIsAtEndOfLine) {
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* proxy = GetAccessible(aID);
  if (!proxy) {
    NS_ERROR("unknown caret move event target!");
    return IPC_OK();
  }

  mCaretId = aID;
  mCaretOffset = aOffset;
  mIsCaretAtEndOfLine = aIsAtEndOfLine;

#if defined(XP_WIN)
  ProxyCaretMoveEvent(proxy, aCaretRect);
#else
  ProxyCaretMoveEvent(proxy, aOffset, aIsSelectionCollapsed);
#endif

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(proxy);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsINode* node = nullptr;
  bool fromUser = true;  // XXX fix me
  uint32_t type = nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED;
  RefPtr<xpcAccCaretMoveEvent> event =
      new xpcAccCaretMoveEvent(type, xpcAcc, doc, node, fromUser, aOffset,
                               aIsSelectionCollapsed, aIsAtEndOfLine);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvTextChangeEvent(
    const uint64_t& aID, const nsString& aStr, const int32_t& aStart,
    const uint32_t& aLen, const bool& aIsInsert, const bool& aFromUser) {
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
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
  uint32_t type = aIsInsert ? nsIAccessibleEvent::EVENT_TEXT_INSERTED
                            : nsIAccessibleEvent::EVENT_TEXT_REMOVED;
  nsINode* node = nullptr;
  RefPtr<xpcAccTextChangeEvent> event = new xpcAccTextChangeEvent(
      type, xpcAcc, doc, node, aFromUser, aStart, aLen, aIsInsert, aStr);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

#if defined(XP_WIN)

mozilla::ipc::IPCResult DocAccessibleParent::RecvSyncTextChangeEvent(
    const uint64_t& aID, const nsString& aStr, const int32_t& aStart,
    const uint32_t& aLen, const bool& aIsInsert, const bool& aFromUser) {
  return RecvTextChangeEvent(aID, aStr, aStart, aLen, aIsInsert, aFromUser);
}

#endif  // defined(XP_WIN)

mozilla::ipc::IPCResult DocAccessibleParent::RecvSelectionEvent(
    const uint64_t& aID, const uint64_t& aWidgetID, const uint32_t& aType) {
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  RemoteAccessible* widget = GetAccessible(aWidgetID);
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
  RefPtr<xpcAccEvent> event =
      new xpcAccEvent(aType, xpcTarget, xpcDoc, nullptr, false);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvVirtualCursorChangeEvent(
    const uint64_t& aID, const uint64_t& aOldPositionID,
    const int32_t& aOldStartOffset, const int32_t& aOldEndOffset,
    const uint64_t& aNewPositionID, const int32_t& aNewStartOffset,
    const int32_t& aNewEndOffset, const int16_t& aReason,
    const int16_t& aBoundaryType, const bool& aFromUser) {
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  RemoteAccessible* oldPosition = GetAccessible(aOldPositionID);
  RemoteAccessible* newPosition = GetAccessible(aNewPositionID);

  if (!target) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

#if defined(ANDROID)
  ProxyVirtualCursorChangeEvent(
      target, oldPosition, aOldStartOffset, aOldEndOffset, newPosition,
      aNewStartOffset, aNewEndOffset, aReason, aBoundaryType, aFromUser);
#endif

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  RefPtr<xpcAccVirtualCursorChangeEvent> event =
      new xpcAccVirtualCursorChangeEvent(
          nsIAccessibleEvent::EVENT_VIRTUALCURSOR_CHANGED,
          GetXPCAccessible(target), doc, nullptr, aFromUser,
          GetXPCAccessible(oldPosition), aOldStartOffset, aOldEndOffset,
          GetXPCAccessible(newPosition), aNewStartOffset, aNewEndOffset,
          aBoundaryType, aReason);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvScrollingEvent(
    const uint64_t& aID, const uint64_t& aType, const uint32_t& aScrollX,
    const uint32_t& aScrollY, const uint32_t& aMaxScrollX,
    const uint32_t& aMaxScrollY) {
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

#if defined(ANDROID)
  ProxyScrollingEvent(target, aType, aScrollX, aScrollY, aMaxScrollX,
                      aMaxScrollY);
#else
  ProxyEvent(target, aType);
#endif

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(target);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsINode* node = nullptr;
  bool fromUser = true;  // XXX: Determine if this was from user input.
  RefPtr<xpcAccScrollingEvent> event =
      new xpcAccScrollingEvent(aType, xpcAcc, doc, node, fromUser, aScrollX,
                               aScrollY, aMaxScrollX, aMaxScrollY);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvCache(
    const mozilla::a11y::CacheUpdateType& aUpdateType,
    nsTArray<CacheData>&& aData, const bool& aFinal) {
  if (mShutdown) {
    return IPC_OK();
  }

  for (auto& entry : aData) {
    RemoteAccessible* remote = GetAccessible(entry.ID());
    if (!remote) {
      MOZ_ASSERT_UNREACHABLE("No remote found!");
      continue;
    }

    remote->ApplyCache(aUpdateType, entry.Fields());
  }

  if (nsCOMPtr<nsIObserverService> obsService =
          services::GetObserverService()) {
    obsService->NotifyObservers(nullptr, NS_ACCESSIBLE_CACHE_TOPIC, nullptr);
  }

  return IPC_OK();
}

#if !defined(XP_WIN)
mozilla::ipc::IPCResult DocAccessibleParent::RecvAnnouncementEvent(
    const uint64_t& aID, const nsString& aAnnouncement,
    const uint16_t& aPriority) {
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

#  if defined(ANDROID)
  ProxyAnnouncementEvent(target, aAnnouncement, aPriority);
#  endif

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(target);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  RefPtr<xpcAccAnnouncementEvent> event = new xpcAccAnnouncementEvent(
      nsIAccessibleEvent::EVENT_ANNOUNCEMENT, xpcAcc, doc, nullptr, false,
      aAnnouncement, aPriority);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvTextSelectionChangeEvent(
    const uint64_t& aID, nsTArray<TextRangeData>&& aSelection) {
#  ifdef MOZ_WIDGET_COCOA
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

  ProxyTextSelectionChangeEvent(target, aSelection);

  return IPC_OK();
#  else
  return RecvEvent(aID, nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED);
#  endif
}

#endif

mozilla::ipc::IPCResult DocAccessibleParent::RecvRoleChangedEvent(
    const a11y::role& aRole) {
  if (mShutdown) {
    return IPC_OK();
  }

  mRole = aRole;

#ifdef MOZ_WIDGET_COCOA
  ProxyRoleChangedEvent(this, aRole);
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvBindChildDoc(
    PDocAccessibleParent* aChildDoc, const uint64_t& aID) {
  // One document should never directly be the child of another.
  // We should always have at least an outer doc accessible in between.
  MOZ_ASSERT(aID);
  if (!aID) return IPC_FAIL(this, "ID is 0!");

  if (mShutdown) {
    return IPC_OK();
  }

  MOZ_ASSERT(CheckDocTree());

  auto childDoc = static_cast<DocAccessibleParent*>(aChildDoc);
  childDoc->Unbind();
  ipc::IPCResult result = AddChildDoc(childDoc, aID, false);
  MOZ_ASSERT(result);
  MOZ_ASSERT(CheckDocTree());
#ifdef DEBUG
  if (!result) {
    return result;
  }
#else
  result = IPC_OK();
#endif

  return result;
}

ipc::IPCResult DocAccessibleParent::AddChildDoc(DocAccessibleParent* aChildDoc,
                                                uint64_t aParentID,
                                                bool aCreating) {
  // We do not use GetAccessible here because we want to be sure to not get the
  // document it self.
  ProxyEntry* e = mAccessibles.GetEntry(aParentID);
  if (!e) {
    MOZ_DIAGNOSTIC_ASSERT(false, "Binding to nonexistent proxy!");
    return IPC_FAIL(this, "binding to nonexistant proxy!");
  }

  RemoteAccessible* outerDoc = e->mProxy;
  MOZ_ASSERT(outerDoc);

  // OuterDocAccessibles are expected to only have a document as a child.
  // However for compatibility we tolerate replacing one document with another
  // here.
  if (!outerDoc->IsOuterDoc() || outerDoc->ChildCount() > 1 ||
      (outerDoc->ChildCount() == 1 && !outerDoc->RemoteChildAt(0)->IsDoc())) {
    MOZ_DIAGNOSTIC_ASSERT(false,
                          "Binding to parent that isn't a valid OuterDoc!");
    return IPC_FAIL(this, "Binding to parent that isn't a valid OuterDoc!");
  }

  if (outerDoc->ChildCount() == 1) {
    MOZ_ASSERT(outerDoc->RemoteChildAt(0)->AsDoc());
    outerDoc->RemoteChildAt(0)->AsDoc()->Unbind();
  }

  aChildDoc->SetParent(outerDoc);
  outerDoc->SetChildDoc(aChildDoc);
  mChildDocs.AppendElement(aChildDoc->mActorID);
  aChildDoc->mParentDoc = mActorID;

  if (aCreating) {
    ProxyCreated(aChildDoc);
  }

  if (aChildDoc->IsTopLevelInContentProcess()) {
    // aChildDoc is an embedded document in a different content process to
    // this document.
    auto embeddedBrowser =
        static_cast<dom::BrowserParent*>(aChildDoc->Manager());
    dom::BrowserBridgeParent* bridge =
        embeddedBrowser->GetBrowserBridgeParent();
    if (bridge) {
#if defined(XP_WIN)
      if (!StaticPrefs::accessibility_cache_enabled_AtStartup()) {
        // Send a COM proxy for the embedded document to the embedder process
        // hosting the iframe. This will be returned as the child of the
        // embedder OuterDocAccessible.
        RefPtr<IDispatch> docAcc;
        aChildDoc->GetCOMInterface((void**)getter_AddRefs(docAcc));
        MOZ_ASSERT(docAcc);
        if (docAcc) {
          RefPtr<IDispatch> docWrapped(
              mscom::PassthruProxy::Wrap<IDispatch>(WrapNotNull(docAcc)));
          IDispatchHolder::COMPtrType docPtr(
              mscom::ToProxyUniquePtr(std::move(docWrapped)));
          IDispatchHolder docHolder(std::move(docPtr));
          if (bridge->SendSetEmbeddedDocAccessibleCOMProxy(docHolder)) {
#  if defined(MOZ_SANDBOX)
            aChildDoc->mDocProxyStream = docHolder.GetPreservedStream();
#  endif  // defined(MOZ_SANDBOX)
          }
        }
        // Send a COM proxy for the embedder OuterDocAccessible to the embedded
        // document process. This will be returned as the parent of the
        // embedded document.
        aChildDoc->SendParentCOMProxy(outerDoc);
        if (nsWinUtils::IsWindowEmulationStarted()) {
          // The embedded document should use the same emulated window handle as
          // its embedder. It will return the embedder document (not a window
          // accessible) as the parent accessible, so we pass a null accessible
          // when sending the window to the embedded document.
          Unused << aChildDoc->SendEmulatedWindow(
              reinterpret_cast<uintptr_t>(mEmulatedWindowHandle), nullptr);
        }
        // Send a COM proxy for the top level document to the embedded document
        // process. This will be returned when the client calls QueryService
        // with SID_IAccessibleContentDocument on an accessible in the embedded
        // document.
        DocAccessibleParent* topDoc = this;
        while (DocAccessibleParent* parentDoc = topDoc->ParentDoc()) {
          topDoc = parentDoc;
        }
        MOZ_ASSERT(topDoc && topDoc->IsTopLevel());
        RefPtr<IAccessible> topDocAcc;
        topDoc->GetCOMInterface((void**)getter_AddRefs(topDocAcc));
        MOZ_ASSERT(topDocAcc);
        if (topDocAcc) {
          RefPtr<IAccessible> topDocWrapped(
              mscom::PassthruProxy::Wrap<IAccessible>(WrapNotNull(topDocAcc)));
          IAccessibleHolder::COMPtrType topDocPtr(
              mscom::ToProxyUniquePtr(std::move(topDocWrapped)));
          IAccessibleHolder topDocHolder(std::move(topDocPtr));
          if (aChildDoc->SendTopLevelDocCOMProxy(topDocHolder)) {
#  if defined(MOZ_SANDBOX)
            aChildDoc->mTopLevelDocProxyStream =
                topDocHolder.GetPreservedStream();
#  endif  // defined(MOZ_SANDBOX)
          }
        }
      }
      if (nsWinUtils::IsWindowEmulationStarted()) {
        aChildDoc->SetEmulatedWindowHandle(mEmulatedWindowHandle);
      }
#endif  // defined(XP_WIN)
      // We need to fire a reorder event on the outer doc accessible.
      // For same-process documents, this is fired by the content process, but
      // this isn't possible when the document is in a different process to its
      // embedder.
      // RecvEvent fires both OS and XPCOM events.
      Unused << RecvEvent(aParentID, nsIAccessibleEvent::EVENT_REORDER);
    }
  }

  return IPC_OK();
}

ipc::IPCResult DocAccessibleParent::AddChildDoc(
    dom::BrowserBridgeParent* aBridge) {
  MOZ_ASSERT(aBridge->GetEmbedderAccessibleDoc() == this);
  uint64_t parentId = aBridge->GetEmbedderAccessibleId();
  MOZ_ASSERT(parentId);
  if (!mAccessibles.GetEntry(parentId)) {
    // Sometimes, this gets called before the embedder sends us the
    // OuterDocAccessible. We must add the child when the OuterDocAccessible
    // gets created later.
    mPendingOOPChildDocs.Insert(aBridge);
    return IPC_OK();
  }
  return AddChildDoc(aBridge->GetDocAccessibleParent(), parentId,
                     /* aCreating */ false);
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvShutdown() {
  Destroy();

  auto mgr = static_cast<dom::BrowserParent*>(Manager());
  if (!mgr->IsDestroyed()) {
    if (!PDocAccessibleParent::Send__delete__(this)) {
      return IPC_FAIL_NO_REASON(mgr);
    }
  }

  return IPC_OK();
}

void DocAccessibleParent::Destroy() {
  // If we are already shutdown that is because our containing tab parent is
  // shutting down in which case we don't need to do anything.
  if (mShutdown) {
    return;
  }

  mShutdown = true;

  MOZ_DIAGNOSTIC_ASSERT(LiveDocs().Contains(mActorID));
  uint32_t childDocCount = mChildDocs.Length();
  for (uint32_t i = 0; i < childDocCount; i++) {
    for (uint32_t j = i + 1; j < childDocCount; j++) {
      MOZ_DIAGNOSTIC_ASSERT(mChildDocs[i] != mChildDocs[j]);
    }
  }

  // XXX This indirection through the hash map of live documents shouldn't be
  // needed, but be paranoid for now.
  int32_t actorID = mActorID;
  for (uint32_t i = childDocCount - 1; i < childDocCount; i--) {
    DocAccessibleParent* thisDoc = LiveDocs().Get(actorID);
    MOZ_ASSERT(thisDoc);
    if (!thisDoc) {
      return;
    }

    thisDoc->ChildDocAt(i)->Destroy();
  }

  for (auto iter = mAccessibles.Iter(); !iter.Done(); iter.Next()) {
    MOZ_ASSERT(iter.Get()->mProxy != this);
    ProxyDestroyed(iter.Get()->mProxy);
    iter.Remove();
  }

  DocAccessibleParent* thisDoc = LiveDocs().Get(actorID);
  MOZ_ASSERT(thisDoc);
  if (!thisDoc) {
    return;
  }

  // The code above should have already completely cleared these, but to be
  // extra safe make sure they are cleared here.
  thisDoc->mAccessibles.Clear();
  thisDoc->mChildDocs.Clear();

  DocManager::NotifyOfRemoteDocShutdown(thisDoc);
  thisDoc = LiveDocs().Get(actorID);
  MOZ_ASSERT(thisDoc);
  if (!thisDoc) {
    return;
  }

  ProxyDestroyed(thisDoc);
  thisDoc = LiveDocs().Get(actorID);
  MOZ_ASSERT(thisDoc);
  if (!thisDoc) {
    return;
  }

  if (DocAccessibleParent* parentDoc = thisDoc->ParentDoc()) {
    parentDoc->RemoveChildDoc(thisDoc);
  } else if (IsTopLevel()) {
    GetAccService()->RemoteDocShutdown(this);
  }
}

DocAccessibleParent* DocAccessibleParent::ParentDoc() const {
  if (mParentDoc == kNoParentDoc) {
    return nullptr;
  }

  return LiveDocs().Get(mParentDoc);
}

bool DocAccessibleParent::CheckDocTree() const {
  size_t childDocs = mChildDocs.Length();
  for (size_t i = 0; i < childDocs; i++) {
    const DocAccessibleParent* childDoc = ChildDocAt(i);
    if (!childDoc || childDoc->ParentDoc() != this) return false;

    if (!childDoc->CheckDocTree()) {
      return false;
    }
  }

  return true;
}

xpcAccessibleGeneric* DocAccessibleParent::GetXPCAccessible(
    RemoteAccessible* aProxy) {
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  MOZ_ASSERT(doc);

  return doc->GetAccessible(aProxy);
}

#if defined(XP_WIN)
void DocAccessibleParent::MaybeInitWindowEmulation() {
  if (!nsWinUtils::IsWindowEmulationStarted()) {
    return;
  }

  // XXX get the bounds from the browserParent instead of poking at accessibles
  // which might not exist yet.
  LocalAccessible* outerDoc = OuterDocOfRemoteBrowser();
  if (!outerDoc) {
    return;
  }

  RootAccessible* rootDocument = outerDoc->RootAccessible();
  MOZ_ASSERT(rootDocument);

  bool isActive = true;
  nsIntRect rect(CW_USEDEFAULT, CW_USEDEFAULT, 0, 0);
  if (Compatibility::IsDolphin()) {
    rect = Bounds();
    nsIntRect rootRect = rootDocument->Bounds();
    rect.MoveToX(rootRect.X() - rect.X());
    rect.MoveToY(rect.Y() - rootRect.Y());

    auto browserParent = static_cast<dom::BrowserParent*>(Manager());
    isActive = browserParent->GetDocShellIsActive();
  }

  // onCreate is guaranteed to be called synchronously by
  // nsWinUtils::CreateNativeWindow, so this reference isn't really necessary.
  // However, static analysis complains without it.
  RefPtr<DocAccessibleParent> thisRef = this;
  nsWinUtils::NativeWindowCreateProc onCreate([thisRef](HWND aHwnd) -> void {
    IDispatchHolder hWndAccHolder;

    ::SetPropW(aHwnd, kPropNameDocAccParent,
               reinterpret_cast<HANDLE>(thisRef.get()));

    thisRef->SetEmulatedWindowHandle(aHwnd);

    RefPtr<IAccessible> hwndAcc;
    if (SUCCEEDED(::AccessibleObjectFromWindow(
            aHwnd, OBJID_WINDOW, IID_IAccessible, getter_AddRefs(hwndAcc)))) {
      RefPtr<IDispatch> wrapped(
          mscom::PassthruProxy::Wrap<IDispatch>(WrapNotNull(hwndAcc)));
      hWndAccHolder.Set(IDispatchHolder::COMPtrType(
          mscom::ToProxyUniquePtr(std::move(wrapped))));
    }

    Unused << thisRef->SendEmulatedWindow(
        reinterpret_cast<uintptr_t>(thisRef->mEmulatedWindowHandle),
        hWndAccHolder);
  });

  HWND parentWnd = reinterpret_cast<HWND>(rootDocument->GetNativeWindow());
  DebugOnly<HWND> hWnd = nsWinUtils::CreateNativeWindow(
      kClassNameTabContent, parentWnd, rect.X(), rect.Y(), rect.Width(),
      rect.Height(), isActive, &onCreate);
  MOZ_ASSERT(hWnd);
}

void DocAccessibleParent::SendParentCOMProxy(Accessible* aOuterDoc) {
  // Make sure that we're not racing with a tab shutdown
  auto tab = static_cast<dom::BrowserParent*>(Manager());
  MOZ_ASSERT(tab);
  if (tab->IsDestroyed()) {
    return;
  }

  RefPtr<IDispatch> nativeAcc =
      already_AddRefed<IDispatch>(MsaaAccessible::NativeAccessible(aOuterDoc));
  if (NS_WARN_IF(!nativeAcc)) {
    // Couldn't get a COM proxy for the outer doc. That probably means it died,
    // but the parent process hasn't received a message to remove it from the
    // RemoteAccessible tree yet.
    return;
  }

  RefPtr<IDispatch> wrapped(
      mscom::PassthruProxy::Wrap<IDispatch>(WrapNotNull(nativeAcc)));

  IDispatchHolder::COMPtrType ptr(mscom::ToProxyUniquePtr(std::move(wrapped)));
  IDispatchHolder holder(std::move(ptr));
  if (!PDocAccessibleParent::SendParentCOMProxy(holder)) {
    return;
  }

#  if defined(MOZ_SANDBOX)
  mParentProxyStream = holder.GetPreservedStream();
#  endif  // defined(MOZ_SANDBOX)
}

void DocAccessibleParent::SetEmulatedWindowHandle(HWND aWindowHandle) {
  if (!aWindowHandle && mEmulatedWindowHandle && IsTopLevel()) {
    ::DestroyWindow(mEmulatedWindowHandle);
  }
  mEmulatedWindowHandle = aWindowHandle;
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvFocusEvent(
    const uint64_t& aID, const LayoutDeviceIntRect& aCaretRect) {
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* proxy = GetAccessible(aID);
  if (!proxy) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

  mFocus = aID;
  ProxyFocusEvent(proxy, aCaretRect);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(proxy);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsINode* node = nullptr;
  bool fromUser = true;  // XXX fix me
  RefPtr<xpcAccEvent> event = new xpcAccEvent(nsIAccessibleEvent::EVENT_FOCUS,
                                              xpcAcc, doc, node, fromUser);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

#endif  // defined(XP_WIN)

#if !defined(XP_WIN)
mozilla::ipc::IPCResult DocAccessibleParent::RecvBatch(
    const uint64_t& aBatchType, nsTArray<BatchData>&& aData) {
  // Only do something in Android. We can't ifdef the entire protocol out in
  // the ipdl because it doesn't allow preprocessing.
#  if defined(ANDROID)
  if (mShutdown) {
    return IPC_OK();
  }
  nsTArray<RemoteAccessible*> proxies(aData.Length());
  for (size_t i = 0; i < aData.Length(); i++) {
    DocAccessibleParent* doc = static_cast<DocAccessibleParent*>(
        aData.ElementAt(i).Document().get_PDocAccessibleParent());
    MOZ_ASSERT(doc);

    if (doc->IsShutdown()) {
      continue;
    }

    RemoteAccessible* proxy = doc->GetAccessible(aData.ElementAt(i).ID());
    if (!proxy) {
      MOZ_ASSERT_UNREACHABLE("No proxy found!");
      continue;
    }

    proxies.AppendElement(proxy);
  }
  ProxyBatch(this, aBatchType, proxies, aData);
#  endif  // defined(XP_WIN)
  return IPC_OK();
}

bool DocAccessibleParent::DeallocPDocAccessiblePlatformExtParent(
    PDocAccessiblePlatformExtParent* aActor) {
  delete aActor;
  return true;
}

PDocAccessiblePlatformExtParent*
DocAccessibleParent::AllocPDocAccessiblePlatformExtParent() {
  return new DocAccessiblePlatformExtParent();
}

DocAccessiblePlatformExtParent* DocAccessibleParent::GetPlatformExtension() {
  return static_cast<DocAccessiblePlatformExtParent*>(
      SingleManagedOrNull(ManagedPDocAccessiblePlatformExtParent()));
}

#endif  // !defined(XP_WIN)

}  // namespace a11y
}  // namespace mozilla
