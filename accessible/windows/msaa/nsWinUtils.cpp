/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWinUtils.h"

#include "Compatibility.h"
#include "DocAccessible.h"
#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"

#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "nsArrayUtils.h"
#include "nsICSSDeclaration.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using namespace mozilla::a11y;
using mozilla::dom::Element;

// Window property used by ipc related code in identifying accessible
// tab windows.
const wchar_t* kPropNameTabContent = L"AccessibleTabWindow";

/**
 * WindowProc to process WM_GETOBJECT messages, used in windows emulation mode.
 */
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                   LPARAM lParam);

bool nsWinUtils::sWindowEmulationStarted = false;

already_AddRefed<nsICSSDeclaration> nsWinUtils::GetComputedStyleDeclaration(
    nsIContent* aContent) {
  nsIContent* elm = nsCoreUtils::GetDOMElementFor(aContent);
  if (!elm) return nullptr;

  // Returns number of items in style declaration
  nsCOMPtr<nsPIDOMWindowInner> window = elm->OwnerDoc()->GetInnerWindow();
  if (!window) return nullptr;

  ErrorResult dummy;
  nsCOMPtr<Element> domElement(do_QueryInterface(elm));
  nsCOMPtr<nsICSSDeclaration> cssDecl =
      window->GetComputedStyle(*domElement, u""_ns, dummy);
  dummy.SuppressException();
  return cssDecl.forget();
}

bool nsWinUtils::MaybeStartWindowEmulation() {
  // Register window class that'll be used for document accessibles associated
  // with tabs.
  if (IPCAccessibilityActive()) return false;

  if (Compatibility::IsJAWS() || Compatibility::IsWE() ||
      Compatibility::IsDolphin() || Compatibility::IsVisperoShared()) {
    RegisterNativeWindow(kClassNameTabContent);
    sWindowEmulationStarted = true;
    return true;
  }

  return false;
}

void nsWinUtils::ShutdownWindowEmulation() {
  // Unregister window call that's used for document accessibles associated
  // with tabs.
  if (IsWindowEmulationStarted()) {
    ::UnregisterClassW(kClassNameTabContent, GetModuleHandle(nullptr));
    sWindowEmulationStarted = false;
  }
}

void nsWinUtils::RegisterNativeWindow(LPCWSTR aWindowClass) {
  WNDCLASSW wc;
  wc.style = CS_GLOBALCLASS;
  wc.lpfnWndProc = WindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.hIcon = nullptr;
  wc.hCursor = nullptr;
  wc.hbrBackground = nullptr;
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = aWindowClass;
  ::RegisterClassW(&wc);
}

HWND nsWinUtils::CreateNativeWindow(LPCWSTR aWindowClass, HWND aParentWnd,
                                    int aX, int aY, int aWidth, int aHeight,
                                    bool aIsActive,
                                    NativeWindowCreateProc* aOnCreateProc) {
  return ::CreateWindowExW(
      WS_EX_TRANSPARENT, aWindowClass, L"NetscapeDispatchWnd",
      WS_CHILD | (aIsActive ? WS_VISIBLE : 0), aX, aY, aWidth, aHeight,
      aParentWnd, nullptr, GetModuleHandle(nullptr), aOnCreateProc);
}

void nsWinUtils::ShowNativeWindow(HWND aWnd) { ::ShowWindow(aWnd, SW_SHOW); }

void nsWinUtils::HideNativeWindow(HWND aWnd) {
  ::SetWindowPos(
      aWnd, nullptr, 0, 0, 0, 0,
      SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  // Note, this window's message handling should not invoke any call that
  // may result in a cross-process ipc call. Doing so may violate RPC
  // message semantics.

  switch (msg) {
    case WM_CREATE: {
      // Mark this window so that ipc related code can identify it.
      ::SetPropW(hWnd, kPropNameTabContent, reinterpret_cast<HANDLE>(1));

      auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
      auto createProc = reinterpret_cast<nsWinUtils::NativeWindowCreateProc*>(
          createStruct->lpCreateParams);

      if (createProc && *createProc) {
        (*createProc)(hWnd);
      }

      return 0;
    }
    case WM_GETOBJECT: {
      // Do explicit casting to make it working on 64bit systems (see bug 649236
      // for details).
      int32_t objId = static_cast<DWORD>(lParam);
      if (objId == OBJID_CLIENT) {
        RefPtr<IAccessible> msaaAccessible;
        DocAccessible* document =
            reinterpret_cast<DocAccessible*>(::GetPropW(hWnd, kPropNameDocAcc));
        if (document) {
          document->GetNativeInterface(getter_AddRefs(msaaAccessible));
        } else {
          DocAccessibleParent* docParent = static_cast<DocAccessibleParent*>(
              ::GetPropW(hWnd, kPropNameDocAccParent));
          if (docParent) {
            if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
              msaaAccessible = MsaaAccessible::GetFrom(docParent);
            } else {
              docParent->GetCOMInterface(getter_AddRefs(msaaAccessible));
            }
          }
        }
        if (msaaAccessible) {
          LRESULT result =
              ::LresultFromObject(IID_IAccessible, wParam,
                                  msaaAccessible);  // does an addref
          return result;
        }
      }
      return 0;
    }
    case WM_NCHITTEST: {
      LRESULT lRet = ::DefWindowProc(hWnd, msg, wParam, lParam);
      if (HTCLIENT == lRet) lRet = HTTRANSPARENT;
      return lRet;
    }
  }

  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
