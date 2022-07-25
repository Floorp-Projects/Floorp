/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "mozilla/a11y/DocAccessibleParent.h"
#include "AccEvent.h"
#include "GeckoCustom.h"
#include "nsAccUtils.h"
#include "nsIAccessibleEvent.h"
#include "nsIWidget.h"
#include "nsWindowsHelpers.h"
#include "mozilla/a11y/HyperTextAccessible.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "ServiceProvider.h"
#include "sdnAccessible.h"

#include "mozilla/mscom/AsyncInvoker.h"
#include "mozilla/StaticPrefs_accessibility.h"

using namespace mozilla;
using namespace mozilla::a11y;

/* For documentation of the accessibility architecture,
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

StaticAutoPtr<nsTArray<AccessibleWrap::HandlerControllerData>>
    AccessibleWrap::sHandlerControllers;

////////////////////////////////////////////////////////////////////////////////
// AccessibleWrap
////////////////////////////////////////////////////////////////////////////////
AccessibleWrap::AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc)
    : LocalAccessible(aContent, aDoc) {}

NS_IMPL_ISUPPORTS_INHERITED0(AccessibleWrap, LocalAccessible)

void AccessibleWrap::Shutdown() {
  if (mMsaa) {
    mMsaa->MsaaShutdown();
    // Don't release mMsaa here because this will cause its id to be released
    // immediately, which will result in immediate reuse, causing problems
    // for clients. Instead, we release it in the destructor.
  }
  LocalAccessible::Shutdown();
}

//-----------------------------------------------------
// IUnknown interface methods - see iunknown.h for documentation
//-----------------------------------------------------

MsaaAccessible* AccessibleWrap::GetMsaa() {
  if (!mMsaa) {
    mMsaa = MsaaAccessible::Create(this);
  }
  return mMsaa;
}

void AccessibleWrap::GetNativeInterface(void** aOutAccessible) {
  RefPtr<IAccessible> result = GetMsaa();
  return result.forget(aOutAccessible);
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible

nsresult AccessibleWrap::HandleAccEvent(AccEvent* aEvent) {
  nsresult rv = LocalAccessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IPCAccessibilityActive()) {
    return NS_OK;
  }

  uint32_t eventType = aEvent->GetEventType();

  // Means we're not active.
  NS_ENSURE_TRUE(!IsDefunct(), NS_ERROR_FAILURE);

  LocalAccessible* accessible = aEvent->GetAccessible();
  if (!accessible) return NS_OK;

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED ||
      eventType == nsIAccessibleEvent::EVENT_FOCUS) {
    UpdateSystemCaretFor(accessible);
  }

  MsaaAccessible::FireWinEvent(accessible, eventType);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// AccessibleWrap

//------- Helper methods ---------

bool AccessibleWrap::IsRootForHWND() {
  if (IsRoot()) {
    return true;
  }
  HWND thisHwnd = MsaaAccessible::GetHWNDFor(this);
  AccessibleWrap* parent = static_cast<AccessibleWrap*>(LocalParent());
  MOZ_ASSERT(parent);
  HWND parentHwnd = MsaaAccessible::GetHWNDFor(parent);
  return thisHwnd != parentHwnd;
}

void AccessibleWrap::UpdateSystemCaretFor(LocalAccessible* aAccessible) {
  // Move the system caret so that Windows Tablet Edition and tradional ATs with
  // off-screen model can follow the caret
  ::DestroyCaret();

  HyperTextAccessible* text = aAccessible->AsHyperText();
  if (!text) return;

  nsIWidget* widget = nullptr;
  LayoutDeviceIntRect caretRect = text->GetCaretRect(&widget);

  if (!widget) {
    return;
  }

  HWND caretWnd =
      reinterpret_cast<HWND>(widget->GetNativeData(NS_NATIVE_WINDOW));
  UpdateSystemCaretFor(caretWnd, caretRect);
}

/* static */
void AccessibleWrap::UpdateSystemCaretFor(
    RemoteAccessible* aProxy, const LayoutDeviceIntRect& aCaretRect) {
  ::DestroyCaret();

  // The HWND should be the real widget HWND, not an emulated HWND.
  // We get the HWND from the proxy's outer doc to bypass window emulation.
  LocalAccessible* outerDoc = aProxy->OuterDocOfRemoteBrowser();
  UpdateSystemCaretFor(MsaaAccessible::GetHWNDFor(outerDoc), aCaretRect);
}

/* static */
void AccessibleWrap::UpdateSystemCaretFor(
    HWND aCaretWnd, const LayoutDeviceIntRect& aCaretRect) {
  if (!aCaretWnd || aCaretRect.IsEmpty()) {
    return;
  }

  // Create invisible bitmap for caret, otherwise its appearance interferes
  // with Gecko caret
  nsAutoBitmap caretBitMap(CreateBitmap(1, aCaretRect.Height(), 1, 1, nullptr));
  if (::CreateCaret(aCaretWnd, caretBitMap, 1,
                    aCaretRect.Height())) {  // Also destroys the last caret
    ::ShowCaret(aCaretWnd);
    RECT windowRect;
    ::GetWindowRect(aCaretWnd, &windowRect);
    ::SetCaretPos(aCaretRect.X() - windowRect.left,
                  aCaretRect.Y() - windowRect.top);
  }
}

/* static */
void AccessibleWrap::SetHandlerControl(DWORD aPid,
                                       RefPtr<IHandlerControl> aCtrl) {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  if (!sHandlerControllers) {
    sHandlerControllers = new nsTArray<HandlerControllerData>();
    ClearOnShutdown(&sHandlerControllers);
  }

  HandlerControllerData ctrlData(aPid, std::move(aCtrl));
  if (sHandlerControllers->Contains(ctrlData)) {
    return;
  }

  sHandlerControllers->AppendElement(std::move(ctrlData));
}

/* static */
void AccessibleWrap::InvalidateHandlers() {
  static const HRESULT kErrorServerDied =
      HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);

  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!sHandlerControllers || sHandlerControllers->IsEmpty()) {
    return;
  }

  // We iterate in reverse so that we may safely remove defunct elements while
  // executing the loop.
  for (auto& controller : Reversed(*sHandlerControllers)) {
    MOZ_ASSERT(controller.mPid);
    MOZ_ASSERT(controller.mCtrl);

    ASYNC_INVOKER_FOR(IHandlerControl)
    invoker(controller.mCtrl, Some(controller.mIsProxy));

    HRESULT hr = ASYNC_INVOKE(invoker, Invalidate);

    if (hr == CO_E_OBJNOTCONNECTED || hr == kErrorServerDied) {
      sHandlerControllers->RemoveElement(controller);
    } else {
      Unused << NS_WARN_IF(FAILED(hr));
    }
  }
}

/* static */
bool AccessibleWrap::DispatchTextChangeToHandler(Accessible* aAcc,
                                                 bool aIsInsert,
                                                 const nsAString& aText,
                                                 int32_t aStart,
                                                 uint32_t aLen) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!StaticPrefs::accessibility_cache_enabled_AtStartup());

  if (!sHandlerControllers || sHandlerControllers->IsEmpty()) {
    return false;
  }

  HWND hwnd = MsaaAccessible::GetHWNDFor(aAcc);
  MOZ_ASSERT(hwnd);
  if (!hwnd) {
    return false;
  }

  long msaaId = MsaaAccessible::GetChildIDFor(aAcc);

  DWORD ourPid = ::GetCurrentProcessId();

  // The handler ends up calling NotifyWinEvent, which should only be done once
  // since it broadcasts the same event to every process who is subscribed.
  // OTOH, if our chrome process contains a handler, we should prefer to
  // broadcast the event from that process, as we want any DLLs injected by ATs
  // to receive the event synchronously. Otherwise we simply choose the first
  // handler in the list, for the lack of a better heuristic.

  nsTArray<HandlerControllerData>::index_type ctrlIndex =
      sHandlerControllers->IndexOf(ourPid);

  if (ctrlIndex == nsTArray<HandlerControllerData>::NoIndex) {
    ctrlIndex = 0;
  }

  HandlerControllerData& controller = sHandlerControllers->ElementAt(ctrlIndex);
  MOZ_ASSERT(controller.mPid);
  MOZ_ASSERT(controller.mCtrl);

  VARIANT_BOOL isInsert = aIsInsert ? VARIANT_TRUE : VARIANT_FALSE;

  IA2TextSegment textSegment{
      ::SysAllocStringLen(
          reinterpret_cast<const wchar_t*>(aText.BeginReading()),
          aText.Length()),
      aStart, aStart + static_cast<long>(aLen)};

  ASYNC_INVOKER_FOR(IHandlerControl)
  invoker(controller.mCtrl, Some(controller.mIsProxy));

  HRESULT hr = ASYNC_INVOKE(invoker, OnTextChange, PtrToLong(hwnd), msaaId,
                            isInsert, &textSegment);

  ::SysFreeString(textSegment.text);

  return SUCCEEDED(hr);
}
