/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWinUtils.h"

#include "Compatibility.h"
#include "DocAccessible.h"
#include "nsCoreUtils.h"

#include "mozilla/Preferences.h"
#include "nsArrayUtils.h"
#include "nsIArray.h"
#include "nsIDocument.h"
#include "nsIDocShellTreeItem.h"

using namespace mozilla;
using namespace mozilla::a11y;

// Window property used by ipc related code in identifying accessible
// tab windows.
const PRUnichar* kPropNameTabContent = L"AccessibleTabWindow";

already_AddRefed<nsIDOMCSSStyleDeclaration>
nsWinUtils::GetComputedStyleDeclaration(nsIContent* aContent)
{
  nsIContent* elm = nsCoreUtils::GetDOMElementFor(aContent);
  if (!elm)
    return nsnull;

  // Returns number of items in style declaration
  nsCOMPtr<nsIDOMWindow> window =
    do_QueryInterface(elm->OwnerDoc()->GetWindow());
  if (!window)
    return nsnull;

  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(elm));
  window->GetComputedStyle(domElement, EmptyString(), getter_AddRefs(cssDecl));
  return cssDecl.forget();
}

HRESULT
nsWinUtils::ConvertToIA2Array(nsIArray *aGeckoArray, IUnknown ***aIA2Array,
                              long *aIA2ArrayLen)
{
  *aIA2Array = NULL;
  *aIA2ArrayLen = 0;

  if (!aGeckoArray)
    return S_FALSE;

  PRUint32 length = 0;
  nsresult rv = aGeckoArray->GetLength(&length);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (length == 0)
    return S_FALSE;

  *aIA2Array =
    static_cast<IUnknown**>(::CoTaskMemAlloc((length) * sizeof(IUnknown*)));
  if (!*aIA2Array)
    return E_OUTOFMEMORY;

  PRUint32 idx = 0;
  for (; idx < length; ++idx) {
    nsCOMPtr<nsIWinAccessNode> winAccessNode =
      do_QueryElementAt(aGeckoArray, idx, &rv);
    if (NS_FAILED(rv))
      break;

    void *instancePtr = NULL;
    nsresult rv = winAccessNode->QueryNativeInterface(IID_IUnknown,
                                                      &instancePtr);
    if (NS_FAILED(rv))
      break;

    (*aIA2Array)[idx] = static_cast<IUnknown*>(instancePtr);
  }

  if (NS_FAILED(rv)) {
    for (PRUint32 idx2 = 0; idx2 < idx; idx2++) {
      (*aIA2Array)[idx2]->Release();
      (*aIA2Array)[idx2] = NULL;
    }

    ::CoTaskMemFree(*aIA2Array);
    return GetHRESULT(rv);
  }

  *aIA2ArrayLen = length;
  return S_OK;
}

bool
nsWinUtils::MaybeStartWindowEmulation()
{
  // Register window class that'll be used for document accessibles associated
  // with tabs.
  if (Compatibility::IsJAWS() || Compatibility::IsWE() ||
      Compatibility::IsDolphin() ||
      Preferences::GetBool("browser.tabs.remote")) {
    RegisterNativeWindow(kClassNameTabContent);
    nsAccessNodeWrap::sHWNDCache.Init(4);
    return true;
  }

  return false;
}

void
nsWinUtils::ShutdownWindowEmulation()
{
  // Unregister window call that's used for document accessibles associated
  // with tabs.
  if (IsWindowEmulationStarted())
    ::UnregisterClassW(kClassNameTabContent, GetModuleHandle(NULL));
}

bool
nsWinUtils::IsWindowEmulationStarted()
{
  return nsAccessNodeWrap::sHWNDCache.IsInitialized();
}

void
nsWinUtils::RegisterNativeWindow(LPCWSTR aWindowClass)
{
  WNDCLASSW wc;
  wc.style = CS_GLOBALCLASS;
  wc.lpfnWndProc = nsAccessNodeWrap::WindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hIcon = NULL;
  wc.hCursor = NULL;
  wc.hbrBackground = NULL;
  wc.lpszMenuName = NULL;
  wc.lpszClassName = aWindowClass;
  ::RegisterClassW(&wc);
}

HWND
nsWinUtils::CreateNativeWindow(LPCWSTR aWindowClass, HWND aParentWnd,
                               int aX, int aY, int aWidth, int aHeight,
                               bool aIsActive)
{
  HWND hwnd = ::CreateWindowExW(WS_EX_TRANSPARENT, aWindowClass,
                                L"NetscapeDispatchWnd",
                                WS_CHILD | (aIsActive ? WS_VISIBLE : 0),
                                aX, aY, aWidth, aHeight,
                                aParentWnd,
                                NULL,
                                GetModuleHandle(NULL),
                                NULL);
  if (hwnd) {
    // Mark this window so that ipc related code can identify it.
    ::SetPropW(hwnd, kPropNameTabContent, (HANDLE)1);
  }
  return hwnd;
}

void
nsWinUtils::ShowNativeWindow(HWND aWnd)
{
  ::ShowWindow(aWnd, SW_SHOW);
}

void
nsWinUtils::HideNativeWindow(HWND aWnd)
{
  ::SetWindowPos(aWnd, NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE |
                 SWP_NOZORDER | SWP_NOACTIVATE);
}
