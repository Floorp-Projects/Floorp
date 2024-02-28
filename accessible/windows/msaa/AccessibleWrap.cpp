/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "mozilla/a11y/DocAccessibleParent.h"
#include "AccEvent.h"
#include "nsAccUtils.h"
#include "nsIAccessibleEvent.h"
#include "nsIWidget.h"
#include "nsWindowsHelpers.h"
#include "mozilla/a11y/HyperTextAccessible.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "ServiceProvider.h"
#include "sdnAccessible.h"
#include "LocalAccessible-inl.h"

using namespace mozilla;
using namespace mozilla::a11y;

/* For documentation of the accessibility architecture,
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

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

/* static */
void AccessibleWrap::UpdateSystemCaretFor(
    Accessible* aAccessible, const LayoutDeviceIntRect& aCaretRect) {
  if (LocalAccessible* localAcc = aAccessible->AsLocal()) {
    // XXX We need the widget for LocalAccessible, so we have to call
    // HyperTextAccessible::GetCaretRect. We should find some way of avoiding
    // the need for the widget.
    UpdateSystemCaretFor(localAcc);
  } else {
    UpdateSystemCaretFor(aAccessible->AsRemote(), aCaretRect);
  }
}

/* static */
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
    POINT clientPoint{aCaretRect.X(), aCaretRect.Y()};
    ::ScreenToClient(aCaretWnd, &clientPoint);
    ::SetCaretPos(clientPoint.x, clientPoint.y);
  }
}
