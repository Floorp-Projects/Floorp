/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleChild.h"

#include "nsAccessibilityService.h"
#include "LocalAccessible-inl.h"
#include "mozilla/a11y/PlatformChild.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "RootAccessible.h"

namespace mozilla {
namespace a11y {

static StaticAutoPtr<PlatformChild> sPlatformChild;

DocAccessibleChild::DocAccessibleChild(DocAccessible* aDoc, IProtocol* aManager)
    : DocAccessibleChildBase(aDoc), mEmulatedWindowHandle(nullptr) {
  MOZ_COUNT_CTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
  if (!sPlatformChild) {
    sPlatformChild = new PlatformChild();
    ClearOnShutdown(&sPlatformChild, ShutdownPhase::XPCOMShutdown);
  }

  SetManager(aManager);
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // If the cache is enabled, we don't need to care whether this is
    // constructed in the parent process. We must still set this flag because we
    // defer sending any events unless it is set.
    SetConstructedInParentProcess();
  }
}

DocAccessibleChild::~DocAccessibleChild() {
  MOZ_COUNT_DTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
}

void DocAccessibleChild::Shutdown() {
  if (IsConstructedInParentProcess()) {
    DocAccessibleChildBase::Shutdown();
    return;
  }

  PushDeferredEvent(MakeUnique<SerializedShutdown>(this));
  DetachDocument();
}

ipc::IPCResult DocAccessibleChild::RecvParentCOMProxy(
    const IDispatchHolder& aParentCOMProxy) {
  MOZ_ASSERT(!aParentCOMProxy.IsNull());
  mParentProxy.reset(const_cast<IDispatchHolder&>(aParentCOMProxy).Release());
  SetConstructedInParentProcess();

  for (uint32_t i = 0, l = mDeferredEvents.Length(); i < l; ++i) {
    mDeferredEvents[i]->Dispatch();
  }

  mDeferredEvents.Clear();

  return IPC_OK();
}

ipc::IPCResult DocAccessibleChild::RecvEmulatedWindow(
    const WindowsHandle& aEmulatedWindowHandle,
    const IDispatchHolder& aEmulatedWindowCOMProxy) {
  mEmulatedWindowHandle = reinterpret_cast<HWND>(aEmulatedWindowHandle);
  if (!aEmulatedWindowCOMProxy.IsNull()) {
    MOZ_ASSERT(!mEmulatedWindowProxy);
    mEmulatedWindowProxy.reset(
        const_cast<IDispatchHolder&>(aEmulatedWindowCOMProxy).Release());
  }

  return IPC_OK();
}

ipc::IPCResult DocAccessibleChild::RecvTopLevelDocCOMProxy(
    const IAccessibleHolder& aCOMProxy) {
  MOZ_ASSERT(!aCOMProxy.IsNull());
  mTopLevelDocProxy.reset(const_cast<IAccessibleHolder&>(aCOMProxy).Release());
  return IPC_OK();
}

HWND DocAccessibleChild::GetNativeWindowHandle() const {
  if (mEmulatedWindowHandle) {
    return mEmulatedWindowHandle;
  }

  auto browser = static_cast<dom::BrowserChild*>(Manager());
  MOZ_ASSERT(browser);
  // Iframe documents use the same window handle as their top level document.
  auto topDoc = static_cast<DocAccessibleChild*>(
      browser->GetTopLevelDocAccessibleChild());
  MOZ_ASSERT(topDoc);
  if (topDoc != this && topDoc->mEmulatedWindowHandle) {
    return topDoc->mEmulatedWindowHandle;
  }

  return reinterpret_cast<HWND>(browser->GetNativeWindowHandle());
}

void DocAccessibleChild::PushDeferredEvent(UniquePtr<DeferredEvent> aEvent) {
  DocAccessibleChild* topLevelIPCDoc = nullptr;

  if (mDoc && mDoc->IsRoot()) {
    topLevelIPCDoc = this;
  } else {
    auto browserChild = static_cast<dom::BrowserChild*>(Manager());
    if (!browserChild) {
      return;
    }

    topLevelIPCDoc = static_cast<DocAccessibleChild*>(
        browserChild->GetTopLevelDocAccessibleChild());
  }

  if (topLevelIPCDoc) {
    topLevelIPCDoc->mDeferredEvents.AppendElement(std::move(aEvent));
  }
}

bool DocAccessibleChild::SendEvent(const uint64_t& aID, const uint32_t& aType) {
  if (IsConstructedInParentProcess()) {
    return PDocAccessibleChild::SendEvent(aID, aType);
  }

  PushDeferredEvent(MakeUnique<SerializedEvent>(this, aID, aType));
  return false;
}

void DocAccessibleChild::MaybeSendShowEvent(ShowEventData& aData,
                                            bool aFromUser) {
  if (IsConstructedInParentProcess()) {
    Unused << SendShowEvent(aData, aFromUser);
    return;
  }

  PushDeferredEvent(MakeUnique<SerializedShow>(this, aData, aFromUser));
}

bool DocAccessibleChild::SendHideEvent(const uint64_t& aRootID,
                                       const bool& aFromUser) {
  if (IsConstructedInParentProcess()) {
    return PDocAccessibleChild::SendHideEvent(aRootID, aFromUser);
  }

  PushDeferredEvent(MakeUnique<SerializedHide>(this, aRootID, aFromUser));
  return true;
}

bool DocAccessibleChild::SendStateChangeEvent(const uint64_t& aID,
                                              const uint64_t& aState,
                                              const bool& aEnabled) {
  if (IsConstructedInParentProcess()) {
    return PDocAccessibleChild::SendStateChangeEvent(aID, aState, aEnabled);
  }

  PushDeferredEvent(
      MakeUnique<SerializedStateChange>(this, aID, aState, aEnabled));
  return true;
}

LayoutDeviceIntRect DocAccessibleChild::GetCaretRectFor(const uint64_t& aID) {
  LocalAccessible* target;

  if (aID) {
    target = reinterpret_cast<LocalAccessible*>(aID);
  } else {
    target = mDoc;
  }

  MOZ_ASSERT(target);

  HyperTextAccessible* text = target->AsHyperText();
  if (!text) {
    return LayoutDeviceIntRect();
  }

  nsIWidget* widget = nullptr;
  return text->GetCaretRect(&widget);
}

bool DocAccessibleChild::SendFocusEvent(const uint64_t& aID) {
  return SendFocusEvent(aID, GetCaretRectFor(aID));
}

bool DocAccessibleChild::SendFocusEvent(const uint64_t& aID,
                                        const LayoutDeviceIntRect& aCaretRect) {
  if (IsConstructedInParentProcess()) {
    return PDocAccessibleChild::SendFocusEvent(aID, aCaretRect);
  }

  PushDeferredEvent(MakeUnique<SerializedFocus>(this, aID, aCaretRect));
  return true;
}

bool DocAccessibleChild::SendCaretMoveEvent(const uint64_t& aID,
                                            const int32_t& aOffset,
                                            const bool& aIsSelectionCollapsed) {
  return SendCaretMoveEvent(aID, GetCaretRectFor(aID), aOffset,
                            aIsSelectionCollapsed);
}

bool DocAccessibleChild::SendCaretMoveEvent(
    const uint64_t& aID, const LayoutDeviceIntRect& aCaretRect,
    const int32_t& aOffset, const bool& aIsSelectionCollapsed) {
  if (IsConstructedInParentProcess()) {
    return PDocAccessibleChild::SendCaretMoveEvent(aID, aCaretRect, aOffset,
                                                   aIsSelectionCollapsed);
  }

  PushDeferredEvent(MakeUnique<SerializedCaretMove>(
      this, aID, aCaretRect, aOffset, aIsSelectionCollapsed));
  return true;
}

bool DocAccessibleChild::SendTextChangeEvent(
    const uint64_t& aID, const nsString& aStr, const int32_t& aStart,
    const uint32_t& aLen, const bool& aIsInsert, const bool& aFromUser,
    const bool aDoSync) {
  if (IsConstructedInParentProcess()) {
    if (aDoSync) {
      // The AT is going to need to reenter content while the event is being
      // dispatched synchronously.
      return PDocAccessibleChild::SendSyncTextChangeEvent(
          aID, aStr, aStart, aLen, aIsInsert, aFromUser);
    }
    return PDocAccessibleChild::SendTextChangeEvent(aID, aStr, aStart, aLen,
                                                    aIsInsert, aFromUser);
  }

  PushDeferredEvent(MakeUnique<SerializedTextChange>(
      this, aID, aStr, aStart, aLen, aIsInsert, aFromUser));
  return true;
}

bool DocAccessibleChild::SendSelectionEvent(const uint64_t& aID,
                                            const uint64_t& aWidgetID,
                                            const uint32_t& aType) {
  if (IsConstructedInParentProcess()) {
    return PDocAccessibleChild::SendSelectionEvent(aID, aWidgetID, aType);
  }

  PushDeferredEvent(
      MakeUnique<SerializedSelection>(this, aID, aWidgetID, aType));
  return true;
}

bool DocAccessibleChild::SendRoleChangedEvent(const a11y::role& aRole) {
  if (IsConstructedInParentProcess()) {
    return PDocAccessibleChild::SendRoleChangedEvent(aRole);
  }

  PushDeferredEvent(MakeUnique<SerializedRoleChanged>(this, aRole));
  return true;
}

bool DocAccessibleChild::SendScrollingEvent(const uint64_t& aID,
                                            const uint64_t& aType,
                                            const uint32_t& aScrollX,
                                            const uint32_t& aScrollY,
                                            const uint32_t& aMaxScrollX,
                                            const uint32_t& aMaxScrollY) {
  if (IsConstructedInParentProcess()) {
    return PDocAccessibleChild::SendScrollingEvent(
        aID, aType, aScrollX, aScrollY, aMaxScrollX, aMaxScrollY);
  }

  PushDeferredEvent(MakeUnique<SerializedScrolling>(
      this, aID, aType, aScrollX, aScrollY, aMaxScrollX, aMaxScrollY));
  return true;
}

bool DocAccessibleChild::ConstructChildDocInParentProcess(
    DocAccessibleChild* aNewChildDoc, uint64_t aUniqueID, uint32_t aMsaaID) {
  if (IsConstructedInParentProcess()) {
    // We may send the constructor immediately
    auto browserChild = static_cast<dom::BrowserChild*>(Manager());
    MOZ_ASSERT(browserChild);
    bool result = browserChild->SendPDocAccessibleConstructor(
        aNewChildDoc, this, aUniqueID, aMsaaID, IAccessibleHolder());
    if (result) {
      aNewChildDoc->SetConstructedInParentProcess();
    }
    return result;
  }

  PushDeferredEvent(MakeUnique<SerializedChildDocConstructor>(
      aNewChildDoc, this, aUniqueID, aMsaaID));
  return true;
}

bool DocAccessibleChild::SendBindChildDoc(DocAccessibleChild* aChildDoc,
                                          const uint64_t& aNewParentID) {
  if (IsConstructedInParentProcess()) {
    return DocAccessibleChildBase::SendBindChildDoc(aChildDoc, aNewParentID);
  }

  PushDeferredEvent(
      MakeUnique<SerializedBindChildDoc>(this, aChildDoc, aNewParentID));
  return true;
}

ipc::IPCResult DocAccessibleChild::RecvRestoreFocus() {
  FocusMgr()->ForceFocusEvent();
  return IPC_OK();
}

void DocAccessibleChild::SetEmbedderOnBridge(dom::BrowserBridgeChild* aBridge,
                                             uint64_t aID) {
  if (IsConstructedInParentProcess()) {
    MOZ_ASSERT(CanSend());
    aBridge->SendSetEmbedderAccessible(this, aID);
    return;
  }
  // Even though this doesn't fire an event, we must ensure this is sent in
  // the correct order with insertions/removals, which are deferred until
  // we are notified about parent process construction. Otherwise, the
  // parent process might bind a child document to the wrong accessible if
  // ids get reused.
  PushDeferredEvent(MakeUnique<SerializedSetEmbedder>(aBridge, this, aID));
}

}  // namespace a11y
}  // namespace mozilla
