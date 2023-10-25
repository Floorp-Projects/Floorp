/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ARIAMap.h"
#include "CachedTableAccessible.h"
#include "DocAccessibleParent.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/Components.h"  // for mozilla::components
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "nsAccessibilityService.h"
#include "xpcAccessibleDocument.h"
#include "xpcAccEvents.h"
#include "nsAccUtils.h"
#include "nsIIOService.h"
#include "TextRange.h"
#include "Relation.h"
#include "RootAccessible.h"

#if defined(XP_WIN)
#  include "Compatibility.h"
#  include "nsWinUtils.h"
#endif

#if defined(ANDROID)
#  define ACQUIRE_ANDROID_LOCK \
    MonitorAutoLock mal(nsAccessibilityService::GetAndroidMonitor());
#else
#  define ACQUIRE_ANDROID_LOCK \
    do {                       \
    } while (0);
#endif

namespace mozilla {

namespace a11y {
uint64_t DocAccessibleParent::sMaxDocID = 0;

DocAccessibleParent::DocAccessibleParent()
    : RemoteAccessible(this),
      mParentDoc(kNoParentDoc),
#if defined(XP_WIN)
      mEmulatedWindowHandle(nullptr),
#endif  // defined(XP_WIN)
      mTopLevel(false),
      mTopLevelInContentProcess(false),
      mShutdown(false),
      mFocus(0),
      mCaretId(0),
      mCaretOffset(-1),
      mIsCaretAtEndOfLine(false) {
  sMaxDocID++;
  mActorID = sMaxDocID;
  MOZ_ASSERT(!LiveDocs().Get(mActorID));
  LiveDocs().InsertOrUpdate(mActorID, this);
}

DocAccessibleParent::~DocAccessibleParent() {
  UnregisterWeakMemoryReporter(this);
  LiveDocs().Remove(mActorID);
  MOZ_ASSERT(mChildDocs.Length() == 0);
  MOZ_ASSERT(!ParentDoc());
}

already_AddRefed<DocAccessibleParent> DocAccessibleParent::New() {
  RefPtr<DocAccessibleParent> dap(new DocAccessibleParent());
  // We need to do this with a non-zero reference count.  The easiest way is to
  // do it in this static method and hide the constructor.
  RegisterWeakMemoryReporter(dap);
  return dap.forget();
}

void DocAccessibleParent::SetBrowsingContext(
    dom::CanonicalBrowsingContext* aBrowsingContext) {
  mBrowsingContext = aBrowsingContext;
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvShowEvent(
    nsTArray<AccessibleData>&& aNewTree, const bool& aEventSuppressed,
    const bool& aComplete, const bool& aFromUser) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) return IPC_OK();

  MOZ_ASSERT(CheckDocTree());

  if (aNewTree.IsEmpty()) {
    return IPC_FAIL(this, "No children being added");
  }

  RemoteAccessible* root = nullptr;
  RemoteAccessible* rootParent = nullptr;
  RemoteAccessible* lastParent = this;
  uint64_t lastParentID = 0;
  for (const auto& accData : aNewTree) {
    RemoteAccessible* parent = accData.ParentID() == lastParentID
                                   ? lastParent
                                   : GetAccessible(accData.ParentID());
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

    uint32_t childIdx = accData.IndexInParent();
    if (childIdx > parent->ChildCount()) {
      NS_ERROR("invalid index to add child at");
#ifdef DEBUG
      return IPC_FAIL(this, "invalid index");
#else
      return IPC_OK();
#endif
    }

    RemoteAccessible* child = CreateAcc(accData);
    if (!child) {
      // This shouldn't happen.
      return IPC_FAIL(this, "failed to add children");
    }
    if (!root && !mPendingShowChild) {
      // This is the first Accessible, which is the root of the shown subtree.
      root = child;
      rootParent = parent;
    }
    // If this show event has been split across multiple messages and this is
    // not the last message, don't attach the shown root to the tree yet.
    // Otherwise, clients might crawl the incomplete subtree and they won't get
    // mutation events for the remaining pieces.
    if (aComplete || root != child) {
      AttachChild(parent, childIdx, child);
    }
  }

  MOZ_ASSERT(CheckDocTree());

  if (!aComplete && !mPendingShowChild) {
    // This is the first message for a show event split across multiple
    // messages. Save the show target for subsequent messages and return.
    const auto& accData = aNewTree[0];
    mPendingShowChild = accData.ID();
    mPendingShowParent = accData.ParentID();
    mPendingShowIndex = accData.IndexInParent();
    return IPC_OK();
  }
  if (!aComplete) {
    // This show event has been split into multiple messages, but this is
    // neither the first nor the last message. There's nothing more to do here.
    return IPC_OK();
  }
  MOZ_ASSERT(aComplete);
  if (mPendingShowChild) {
    // This is the last message for a show event split across multiple
    // messages. Retrieve the saved show target, attach it to the tree and fire
    // an event if appropriate.
    rootParent = GetAccessible(mPendingShowParent);
    MOZ_ASSERT(rootParent);
    root = GetAccessible(mPendingShowChild);
    MOZ_ASSERT(root);
    AttachChild(rootParent, mPendingShowIndex, root);
    mPendingShowChild = 0;
    mPendingShowParent = 0;
    mPendingShowIndex = 0;
  }

  // Just update, no events.
  if (aEventSuppressed) {
    return IPC_OK();
  }

  PlatformShowHideEvent(root, rootParent, true, aFromUser);

  if (nsCOMPtr<nsIObserverService> obsService =
          services::GetObserverService()) {
    obsService->NotifyObservers(nullptr, NS_ACCESSIBLE_CACHE_TOPIC, nullptr);
  }

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  uint32_t type = nsIAccessibleEvent::EVENT_SHOW;
  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(root);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsINode* node = nullptr;
  RefPtr<xpcAccEvent> event =
      new xpcAccEvent(type, xpcAcc, doc, node, aFromUser);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

RemoteAccessible* DocAccessibleParent::CreateAcc(
    const AccessibleData& aAccData) {
  RemoteAccessible* newProxy;
  if ((newProxy = GetAccessible(aAccData.ID()))) {
    // This is a move. Reuse the Accessible; don't destroy it.
    MOZ_ASSERT(!newProxy->RemoteParent());
    return newProxy;
  }

  if (!aria::IsRoleMapIndexValid(aAccData.RoleMapEntryIndex())) {
    MOZ_ASSERT_UNREACHABLE("Invalid role map entry index");
    return nullptr;
  }

  newProxy = new RemoteAccessible(aAccData.ID(), this, aAccData.Role(),
                                  aAccData.Type(), aAccData.GenericTypes(),
                                  aAccData.RoleMapEntryIndex());
  mAccessibles.PutEntry(aAccData.ID())->mProxy = newProxy;

  if (RefPtr<AccAttributes> fields = aAccData.CacheFields()) {
    newProxy->ApplyCache(CacheUpdateType::Initial, fields);
  }

  return newProxy;
}

void DocAccessibleParent::AttachChild(RemoteAccessible* aParent,
                                      uint32_t aIndex,
                                      RemoteAccessible* aChild) {
  aParent->AddChildAt(aIndex, aChild);
  aChild->SetParent(aParent);
  // ProxyCreated might have already been called if aChild is being moved.
  if (!aChild->GetWrapper()) {
    ProxyCreated(aChild);
  }
  if (aChild->IsTableCell()) {
    CachedTableAccessible::Invalidate(aChild);
  }
  if (aChild->IsOuterDoc()) {
    // We can only do this after ProxyCreated is called because it will fire an
    // event on aChild.
    mPendingOOPChildDocs.RemoveIf([&](dom::BrowserBridgeParent* bridge) {
      MOZ_ASSERT(bridge->GetBrowserParent(),
                 "Pending BrowserBridgeParent should be alive");
      if (bridge->GetEmbedderAccessibleId() != aChild->ID()) {
        return false;
      }
      MOZ_ASSERT(bridge->GetEmbedderAccessibleDoc() == this);
      if (DocAccessibleParent* childDoc = bridge->GetDocAccessibleParent()) {
        AddChildDoc(childDoc, aChild->ID(), false);
      }
      return true;
    });
  }
}

void DocAccessibleParent::ShutdownOrPrepareForMove(RemoteAccessible* aAcc) {
  // Children might be removed or moved. Handle them the same way. We do this
  // before checking the moving IDs set in order to ensure that we handle moved
  // descendants properly. Avoid descending into the children of outer documents
  // for moves since they are added and removed differently to normal children.
  if (!aAcc->IsOuterDoc()) {
    // Even if some children are kept, those will be re-attached when we handle
    // the show event. For now, clear all of them by moving them to a temporary.
    auto children{std::move(aAcc->mChildren)};
    for (RemoteAccessible* child : children) {
      ShutdownOrPrepareForMove(child);
    }
  }

  const uint64_t id = aAcc->ID();
  if (!mMovingIDs.Contains(id)) {
    // This Accessible is being removed.
    aAcc->Shutdown();
    return;
  }
  // This is a move. Moves are sent as a hide and then a show, but for a move,
  // we want to keep the Accessible alive for reuse later.
  if (aAcc->IsTable() || aAcc->IsTableCell()) {
    // For table cells, it's important that we do this before the parent is
    // cleared because CachedTableAccessible::Invalidate needs the ancestry.
    CachedTableAccessible::Invalidate(aAcc);
  }
  if (aAcc->IsHyperText()) {
    aAcc->InvalidateCachedHyperTextOffsets();
  }
  aAcc->SetParent(nullptr);
  mMovingIDs.EnsureRemoved(id);
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvHideEvent(
    const uint64_t& aRootID, const bool& aFromUser) {
  ACQUIRE_ANDROID_LOCK
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
  PlatformShowHideEvent(root, parent, false, aFromUser);

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
  ShutdownOrPrepareForMove(root);

  MOZ_ASSERT(CheckDocTree());

  if (event) {
    nsCoreUtils::DispatchAccEvent(std::move(event));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvEvent(
    const uint64_t& aID, const uint32_t& aEventType) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }
  if (aEventType == 0 || aEventType >= nsIAccessibleEvent::EVENT_LAST_ENTRY) {
    MOZ_ASSERT_UNREACHABLE("Invalid event");
    return IPC_FAIL(this, "Invalid event");
  }

  RemoteAccessible* remote = GetAccessible(aID);
  if (!remote) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

  FireEvent(remote, aEventType);
  return IPC_OK();
}

void DocAccessibleParent::FireEvent(RemoteAccessible* aAcc,
                                    const uint32_t& aEventType) {
  if (aEventType == nsIAccessibleEvent::EVENT_REORDER ||
      aEventType == nsIAccessibleEvent::EVENT_INNER_REORDER) {
    uint32_t count = aAcc->ChildCount();
    for (uint32_t c = 0; c < count; ++c) {
      aAcc->RemoteChildAt(c)->InvalidateGroupInfo();
    }
  } else if (aEventType == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE &&
             aAcc == this) {
    // A DocAccessible gets the STALE state while it is still loading, but we
    // don't fire a state change for that. That state might have been
    // included in the initial cache push, so clear it here.
    // We also clear the BUSY state here. Although we do fire a state change
    // for that, we fire it after doc load complete. It doesn't make sense
    // for the document to report BUSY after doc load complete and doing so
    // confuses JAWS.
    UpdateStateCache(states::STALE | states::BUSY, false);
  }

  PlatformEvent(aAcc, aEventType);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return;
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(aAcc);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsINode* node = nullptr;
  bool fromUser = true;  // XXX fix me
  RefPtr<xpcAccEvent> event =
      new xpcAccEvent(aEventType, xpcAcc, doc, node, fromUser);
  nsCoreUtils::DispatchAccEvent(std::move(event));
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvStateChangeEvent(
    const uint64_t& aID, const uint64_t& aState, const bool& aEnabled) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("we don't know about the target of a state change event!");
    return IPC_OK();
  }

  target->UpdateStateCache(aState, aEnabled);
  if (nsCOMPtr<nsIObserverService> obsService =
          services::GetObserverService()) {
    obsService->NotifyObservers(nullptr, NS_ACCESSIBLE_CACHE_TOPIC, nullptr);
  }
  PlatformStateChangeEvent(target, aState, aEnabled);

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
    const uint64_t& aID, const LayoutDeviceIntRect& aCaretRect,
    const int32_t& aOffset, const bool& aIsSelectionCollapsed,
    const bool& aIsAtEndOfLine, const int32_t& aGranularity) {
  ACQUIRE_ANDROID_LOCK
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
  if (aIsSelectionCollapsed) {
    // We don't fire selection events for collapsed selections, but we need to
    // ensure we don't have a stale cached selection; e.g. when selecting
    // forward and then unselecting backward.
    mTextSelections.ClearAndRetainStorage();
    mTextSelections.AppendElement(TextRangeData(aID, aID, aOffset, aOffset));
  }

  PlatformCaretMoveEvent(proxy, aOffset, aIsSelectionCollapsed, aGranularity,
                         aCaretRect);

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(proxy);
  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  nsINode* node = nullptr;
  bool fromUser = true;  // XXX fix me
  uint32_t type = nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED;
  RefPtr<xpcAccCaretMoveEvent> event = new xpcAccCaretMoveEvent(
      type, xpcAcc, doc, node, fromUser, aOffset, aIsSelectionCollapsed,
      aIsAtEndOfLine, aGranularity);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvTextChangeEvent(
    const uint64_t& aID, const nsAString& aStr, const int32_t& aStart,
    const uint32_t& aLen, const bool& aIsInsert, const bool& aFromUser) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("text change event target is unknown!");
    return IPC_OK();
  }

  PlatformTextChangeEvent(target, aStr, aStart, aLen, aIsInsert, aFromUser);

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

mozilla::ipc::IPCResult DocAccessibleParent::RecvSelectionEvent(
    const uint64_t& aID, const uint64_t& aWidgetID, const uint32_t& aType) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }
  if (aType == 0 || aType >= nsIAccessibleEvent::EVENT_LAST_ENTRY) {
    MOZ_ASSERT_UNREACHABLE("Invalid event");
    return IPC_FAIL(this, "Invalid event");
  }

  RemoteAccessible* target = GetAccessible(aID);
  RemoteAccessible* widget = GetAccessible(aWidgetID);
  if (!target || !widget) {
    NS_ERROR("invalid id in selection event");
    return IPC_OK();
  }

  PlatformSelectionEvent(target, widget, aType);
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
    const uint64_t& aNewPositionID, const int16_t& aReason,
    const bool& aFromUser) {
  ACQUIRE_ANDROID_LOCK
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
  PlatformVirtualCursorChangeEvent(target, oldPosition, newPosition, aReason,
                                   aFromUser);
#endif

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }

  xpcAccessibleDocument* doc = GetAccService()->GetXPCDocument(this);
  RefPtr<xpcAccVirtualCursorChangeEvent> event =
      new xpcAccVirtualCursorChangeEvent(
          nsIAccessibleEvent::EVENT_VIRTUALCURSOR_CHANGED,
          GetXPCAccessible(target), doc, nullptr, aFromUser,
          GetXPCAccessible(oldPosition), GetXPCAccessible(newPosition),
          aReason);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvScrollingEvent(
    const uint64_t& aID, const uint64_t& aType, const uint32_t& aScrollX,
    const uint32_t& aScrollY, const uint32_t& aMaxScrollX,
    const uint32_t& aMaxScrollY) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }
  if (aType == 0 || aType >= nsIAccessibleEvent::EVENT_LAST_ENTRY) {
    MOZ_ASSERT_UNREACHABLE("Invalid event");
    return IPC_FAIL(this, "Invalid event");
  }

  RemoteAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

#if defined(ANDROID)
  PlatformScrollingEvent(target, aType, aScrollX, aScrollY, aMaxScrollX,
                         aMaxScrollY);
#else
  PlatformEvent(target, aType);
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
    nsTArray<CacheData>&& aData) {
  ACQUIRE_ANDROID_LOCK
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

mozilla::ipc::IPCResult DocAccessibleParent::RecvSelectedAccessiblesChanged(
    nsTArray<uint64_t>&& aSelectedIDs, nsTArray<uint64_t>&& aUnselectedIDs) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }

  for (auto& id : aSelectedIDs) {
    RemoteAccessible* remote = GetAccessible(id);
    if (!remote) {
      MOZ_ASSERT_UNREACHABLE("No remote found!");
      continue;
    }

    remote->UpdateStateCache(states::SELECTED, true);
  }

  for (auto& id : aUnselectedIDs) {
    RemoteAccessible* remote = GetAccessible(id);
    if (!remote) {
      MOZ_ASSERT_UNREACHABLE("No remote found!");
      continue;
    }

    remote->UpdateStateCache(states::SELECTED, false);
  }

  if (nsCOMPtr<nsIObserverService> obsService =
          services::GetObserverService()) {
    obsService->NotifyObservers(nullptr, NS_ACCESSIBLE_CACHE_TOPIC, nullptr);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvAccessiblesWillMove(
    nsTArray<uint64_t>&& aIDs) {
  for (uint64_t id : aIDs) {
    mMovingIDs.EnsureInserted(id);
  }
  return IPC_OK();
}

#if !defined(XP_WIN)
mozilla::ipc::IPCResult DocAccessibleParent::RecvAnnouncementEvent(
    const uint64_t& aID, const nsAString& aAnnouncement,
    const uint16_t& aPriority) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

#  if defined(ANDROID)
  PlatformAnnouncementEvent(target, aAnnouncement, aPriority);
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
#endif  // !defined(XP_WIN)

mozilla::ipc::IPCResult DocAccessibleParent::RecvTextSelectionChangeEvent(
    const uint64_t& aID, nsTArray<TextRangeData>&& aSelection) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* target = GetAccessible(aID);
  if (!target) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

  mTextSelections.ClearAndRetainStorage();
  mTextSelections.AppendElements(aSelection);

#ifdef MOZ_WIDGET_COCOA
  AutoTArray<TextRange, 1> ranges;
  SelectionRanges(&ranges);
  PlatformTextSelectionChangeEvent(target, ranges);
#else
  PlatformEvent(target, nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED);
#endif

  if (!nsCoreUtils::AccEventObserversExist()) {
    return IPC_OK();
  }
  xpcAccessibleGeneric* xpcAcc = GetXPCAccessible(target);
  xpcAccessibleDocument* doc = nsAccessibilityService::GetXPCDocument(this);
  nsINode* node = nullptr;
  bool fromUser = true;  // XXX fix me
  RefPtr<xpcAccEvent> event =
      new xpcAccEvent(nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED, xpcAcc,
                      doc, node, fromUser);
  nsCoreUtils::DispatchAccEvent(std::move(event));

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvRoleChangedEvent(
    const a11y::role& aRole, const uint8_t& aRoleMapEntryIndex) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }
  if (!aria::IsRoleMapIndexValid(aRoleMapEntryIndex)) {
    MOZ_ASSERT_UNREACHABLE("Invalid role map entry index");
    return IPC_FAIL(this, "Invalid role map entry index");
  }

  mRole = aRole;
  mRoleMapEntryIndex = aRoleMapEntryIndex;

#ifdef MOZ_WIDGET_COCOA
  PlatformRoleChangedEvent(this, aRole, aRoleMapEntryIndex);
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult DocAccessibleParent::RecvBindChildDoc(
    NotNull<PDocAccessibleParent*> aChildDoc, const uint64_t& aID) {
  ACQUIRE_ANDROID_LOCK
  // One document should never directly be the child of another.
  // We should always have at least an outer doc accessible in between.
  MOZ_ASSERT(aID);
  if (!aID) return IPC_FAIL(this, "ID is 0!");

  if (mShutdown) {
    return IPC_OK();
  }

  MOZ_ASSERT(CheckDocTree());

  auto childDoc = static_cast<DocAccessibleParent*>(aChildDoc.get());
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
#ifndef FUZZING_SNAPSHOT
    // This diagnostic assert and the one down below expect a well-behaved
    // child process. In IPC fuzzing, we directly fuzz parameters of each
    // method over IPDL and the asserts are not valid under these conditions.
    MOZ_DIAGNOSTIC_ASSERT(false, "Binding to nonexistent proxy!");
#endif
    return IPC_FAIL(this, "binding to nonexistant proxy!");
  }

  RemoteAccessible* outerDoc = e->mProxy;
  MOZ_ASSERT(outerDoc);

  // OuterDocAccessibles are expected to only have a document as a child.
  // However for compatibility we tolerate replacing one document with another
  // here.
  if (!outerDoc->IsOuterDoc() || outerDoc->ChildCount() > 1 ||
      (outerDoc->ChildCount() == 1 && !outerDoc->RemoteChildAt(0)->IsDoc())) {
#ifndef FUZZING_SNAPSHOT
    MOZ_DIAGNOSTIC_ASSERT(false,
                          "Binding to parent that isn't a valid OuterDoc!");
#endif
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
      if (nsWinUtils::IsWindowEmulationStarted()) {
        aChildDoc->SetEmulatedWindowHandle(mEmulatedWindowHandle);
      }
#endif  // defined(XP_WIN)
      // We need to fire a reorder event on the outer doc accessible.
      // For same-process documents, this is fired by the content process, but
      // this isn't possible when the document is in a different process to its
      // embedder.
      // FireEvent fires both OS and XPCOM events.
      FireEvent(outerDoc, nsIAccessibleEvent::EVENT_REORDER);
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
  ACQUIRE_ANDROID_LOCK
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
  mBrowsingContext = nullptr;

#ifdef ANDROID
  if (FocusMgr() && FocusMgr()->IsFocusedRemoteDoc(this)) {
    FocusMgr()->SetFocusedRemoteDoc(nullptr);
  }
#endif

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
    RemoteAccessible* acc = iter.Get()->mProxy;
    MOZ_ASSERT(acc != this);
    if (acc->IsTable()) {
      CachedTableAccessible::Invalidate(acc);
    }
    ProxyDestroyed(acc);
    iter.Remove();
  }

  DocAccessibleParent* thisDoc = LiveDocs().Get(actorID);
  MOZ_ASSERT(thisDoc);
  if (!thisDoc) {
    return;
  }

  mChildren.Clear();
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

void DocAccessibleParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(CheckDocTree());
  if (!mShutdown) {
    ACQUIRE_ANDROID_LOCK
    Destroy();
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
  LayoutDeviceIntRect rect(CW_USEDEFAULT, CW_USEDEFAULT, 0, 0);
  if (Compatibility::IsDolphin()) {
    rect = Bounds();
    LayoutDeviceIntRect rootRect = rootDocument->Bounds();
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
    ::SetPropW(aHwnd, kPropNameDocAccParent,
               reinterpret_cast<HANDLE>(thisRef.get()));
    thisRef->SetEmulatedWindowHandle(aHwnd);
  });

  HWND parentWnd = reinterpret_cast<HWND>(rootDocument->GetNativeWindow());
  DebugOnly<HWND> hWnd = nsWinUtils::CreateNativeWindow(
      kClassNameTabContent, parentWnd, rect.X(), rect.Y(), rect.Width(),
      rect.Height(), isActive, &onCreate);
  MOZ_ASSERT(hWnd);
}

void DocAccessibleParent::SetEmulatedWindowHandle(HWND aWindowHandle) {
  if (!aWindowHandle && mEmulatedWindowHandle && IsTopLevel()) {
    ::DestroyWindow(mEmulatedWindowHandle);
  }
  mEmulatedWindowHandle = aWindowHandle;
}
#endif  // defined(XP_WIN)

mozilla::ipc::IPCResult DocAccessibleParent::RecvFocusEvent(
    const uint64_t& aID, const LayoutDeviceIntRect& aCaretRect) {
  ACQUIRE_ANDROID_LOCK
  if (mShutdown) {
    return IPC_OK();
  }

  RemoteAccessible* proxy = GetAccessible(aID);
  if (!proxy) {
    NS_ERROR("no proxy for event!");
    return IPC_OK();
  }

#ifdef ANDROID
  if (FocusMgr()) {
    FocusMgr()->SetFocusedRemoteDoc(this);
  }
#endif

  mFocus = aID;
  PlatformFocusEvent(proxy, aCaretRect);

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

void DocAccessibleParent::SelectionRanges(nsTArray<TextRange>* aRanges) const {
  aRanges->SetCapacity(mTextSelections.Length());
  for (const auto& data : mTextSelections) {
    // Selection ranges should usually be in sync with the tree. However, tree
    // and selection updates happen using separate IPDL calls, so it's possible
    // for a client selection query to arrive between them. Thus, we validate
    // the Accessibles and offsets here.
    auto* startAcc =
        const_cast<RemoteAccessible*>(GetAccessible(data.StartID()));
    auto* endAcc = const_cast<RemoteAccessible*>(GetAccessible(data.EndID()));
    if (!startAcc || !endAcc) {
      continue;
    }
    uint32_t startCount = startAcc->CharacterCount();
    if (startCount == 0 ||
        data.StartOffset() > static_cast<int32_t>(startCount)) {
      continue;
    }
    uint32_t endCount = endAcc->CharacterCount();
    if (endCount == 0 || data.EndOffset() > static_cast<int32_t>(endCount)) {
      continue;
    }
    aRanges->AppendElement(TextRange(const_cast<DocAccessibleParent*>(this),
                                     startAcc, data.StartOffset(), endAcc,
                                     data.EndOffset()));
  }
}

Accessible* DocAccessibleParent::FocusedChild() {
  LocalAccessible* outerDoc = OuterDocOfRemoteBrowser();
  if (!outerDoc) {
    return nullptr;
  }

  RootAccessible* rootDocument = outerDoc->RootAccessible();
  return rootDocument->FocusedChild();
}

void DocAccessibleParent::URL(nsACString& aURL) const {
  if (!mBrowsingContext) {
    return;
  }
  nsCOMPtr<nsIURI> uri = mBrowsingContext->GetCurrentURI();
  if (!uri) {
    return;
  }
  // Let's avoid treating too long URI in the main process for avoiding
  // memory fragmentation as far as possible.
  if (uri->SchemeIs("data") || uri->SchemeIs("blob")) {
    return;
  }
  nsCOMPtr<nsIIOService> io = mozilla::components::IO::Service();
  if (NS_WARN_IF(!io)) {
    return;
  }
  nsCOMPtr<nsIURI> exposableURI;
  if (NS_FAILED(io->CreateExposableURI(uri, getter_AddRefs(exposableURI))) ||
      MOZ_UNLIKELY(!exposableURI)) {
    return;
  }
  exposableURI->GetSpec(aURL);
}

void DocAccessibleParent::URL(nsAString& aURL) const {
  nsAutoCString url;
  URL(url);
  CopyUTF8toUTF16(url, aURL);
}

void DocAccessibleParent::MimeType(nsAString& aMime) const {
  if (mCachedFields) {
    mCachedFields->GetAttribute(CacheKey::MimeType, aMime);
  }
}

Relation DocAccessibleParent::RelationByType(RelationType aType) const {
  // If the accessible is top-level, provide the NODE_CHILD_OF relation so that
  // MSAA clients can easily get to true parent instead of getting to oleacc's
  // ROLE_WINDOW accessible when window emulation is enabled which will prevent
  // us from going up further (because it is system generated and has no idea
  // about the hierarchy above it).
  if (aType == RelationType::NODE_CHILD_OF && IsTopLevel()) {
    return Relation(Parent());
  }

  return RemoteAccessible::RelationByType(aType);
}

DocAccessibleParent* DocAccessibleParent::GetFrom(
    dom::BrowsingContext* aBrowsingContext) {
  if (!aBrowsingContext) {
    return nullptr;
  }

  dom::BrowserParent* bp = aBrowsingContext->Canonical()->GetBrowserParent();
  if (!bp) {
    return nullptr;
  }

  const ManagedContainer<PDocAccessibleParent>& docs =
      bp->ManagedPDocAccessibleParent();
  for (auto* key : docs) {
    // Iterate over our docs until we find one with a browsing
    // context that matches the one we passed in. Return that
    // document.
    auto* doc = static_cast<a11y::DocAccessibleParent*>(key);
    if (doc->GetBrowsingContext() == aBrowsingContext) {
      return doc;
    }
  }

  return nullptr;
}

size_t DocAccessibleParent::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) {
  size_t size = 0;

  size += RemoteAccessible::SizeOfExcludingThis(aMallocSizeOf);

  size += mReverseRelations.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto i = mReverseRelations.Iter(); !i.Done(); i.Next()) {
    size += i.Data().ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto j = i.Data().Iter(); !j.Done(); j.Next()) {
      size += j.Data().ShallowSizeOfExcludingThis(aMallocSizeOf);
    }
  }

  size += mOnScreenAccessibles.ShallowSizeOfExcludingThis(aMallocSizeOf);

  size += mChildDocs.ShallowSizeOfExcludingThis(aMallocSizeOf);

  size += mAccessibles.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto i = mAccessibles.Iter(); !i.Done(); i.Next()) {
    size += i.Get()->mProxy->SizeOfIncludingThis(aMallocSizeOf);
  }

  size += mPendingOOPChildDocs.ShallowSizeOfExcludingThis(aMallocSizeOf);

  // The mTextSelections array contains structs of integers.  We can count them
  // by counting the size of the array - there's no deep structure here.
  size += mTextSelections.ShallowSizeOfExcludingThis(aMallocSizeOf);

  return size;
}

MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOfAccessibilityCache);

NS_IMETHODIMP
DocAccessibleParent::CollectReports(nsIHandleReportCallback* aHandleReport,
                                    nsISupports* aData, bool aAnon) {
  nsAutoCString path;

  if (aAnon) {
    path = nsPrintfCString("explicit/a11y/cache(%" PRIu64 ")", mActorID);
  } else {
    nsCString url;
    URL(url);
    url.ReplaceChar(
        '/', '\\');  // Tell the memory reporter this is not a path seperator.
    path = nsPrintfCString("explicit/a11y/cache(%s)", url.get());
  }

  aHandleReport->Callback(
      /* process */ ""_ns, path, KIND_HEAP, UNITS_BYTES,
      SizeOfIncludingThis(MallocSizeOfAccessibilityCache),
      nsLiteralCString("Size of the accessability cache for this document."),
      aData);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(DocAccessibleParent, nsIMemoryReporter);

}  // namespace a11y
}  // namespace mozilla
