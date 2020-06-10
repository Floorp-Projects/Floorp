/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleWrap.h"

#include "Compatibility.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/BrowserChild.h"
#include "DocAccessibleChild.h"
#include "nsWinUtils.h"
#include "Role.h"
#include "RootAccessible.h"
#include "sdnDocAccessible.h"
#include "Statistics.h"

#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// DocAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

DocAccessibleWrap::DocAccessibleWrap(dom::Document* aDocument,
                                     PresShell* aPresShell)
    : DocAccessible(aDocument, aPresShell), mHWND(nullptr) {}

DocAccessibleWrap::~DocAccessibleWrap() {}

IMPL_IUNKNOWN_QUERY_HEAD(DocAccessibleWrap)
if (aIID == IID_ISimpleDOMDocument) {
  statistics::ISimpleDOMUsed();
  *aInstancePtr = static_cast<ISimpleDOMDocument*>(new sdnDocAccessible(this));
  static_cast<IUnknown*>(*aInstancePtr)->AddRef();
  return S_OK;
}
IMPL_IUNKNOWN_QUERY_TAIL_INHERITED(HyperTextAccessibleWrap)

STDMETHODIMP
DocAccessibleWrap::get_accParent(
    /* [retval][out] */ IDispatch __RPC_FAR* __RPC_FAR* ppdispParent) {
  if (IsDefunct()) {
    return CO_E_OBJNOTCONNECTED;
  }

  // We might be a top-level document in a content process.
  DocAccessibleChild* ipcDoc = IPCDoc();
  if (ipcDoc && static_cast<dom::BrowserChild*>(ipcDoc->Manager())
                        ->GetTopLevelDocAccessibleChild() == ipcDoc) {
    // Emulated window proxy is only set for the top level content document when
    // emulation is enabled.
    RefPtr<IDispatch> dispParent = ipcDoc->GetEmulatedWindowIAccessible();
    if (!dispParent) {
      dispParent = ipcDoc->GetParentIAccessible();
    }

    if (!dispParent) {
      return S_FALSE;
    }

    dispParent.forget(ppdispParent);
    return S_OK;
  }

  // In the parent process, return window system accessible object for root
  // document accessibles, as well as tab document accessibles if window
  // emulation is enabled.
  if (XRE_IsParentProcess() &&
      (!ParentDocument() ||
       (nsWinUtils::IsWindowEmulationStarted() &&
        nsCoreUtils::IsTopLevelContentDocInProcess(DocumentNode())))) {
    HWND hwnd = static_cast<HWND>(GetNativeWindow());
    if (hwnd && !ParentDocument()) {
      nsIFrame* frame = GetFrame();
      if (frame) {
        nsIWidget* widget = frame->GetNearestWidget();
        if (widget->WindowType() == eWindowType_child && !widget->GetParent()) {
          // Bug 1427304: Windows opened with popup=yes (such as the WebRTC
          // sharing indicator) get two HWNDs. The root widget is associated
          // with the inner HWND, but the outer HWND still answers to
          // WM_GETOBJECT queries. This means that getting the parent of the
          // oleacc window accessible for the inner HWND returns this
          // root accessible. Thus, to avoid a loop, we must never return the
          // oleacc window accessible for the inner HWND. Instead, we use the
          // outer HWND here.
          HWND parentHwnd = ::GetParent(hwnd);
          if (parentHwnd) {
            MOZ_ASSERT(::GetWindowLongW(parentHwnd, GWL_STYLE) & WS_POPUP,
                       "Parent HWND should be a popup!");
            hwnd = parentHwnd;
          }
        }
      }
    }
    if (hwnd &&
        SUCCEEDED(::AccessibleObjectFromWindow(
            hwnd, OBJID_WINDOW, IID_IAccessible, (void**)ppdispParent))) {
      return S_OK;
    }
  }

  return DocAccessible::get_accParent(ppdispParent);
}

STDMETHODIMP
DocAccessibleWrap::get_accValue(VARIANT aVarChild, BSTR __RPC_FAR* aValue) {
  if (!aValue) return E_INVALIDARG;
  *aValue = nullptr;

  // For backwards-compat, we still support old MSAA hack to provide URL in
  // accValue Check for real value first
  HRESULT hr = AccessibleWrap::get_accValue(aVarChild, aValue);
  if (FAILED(hr) || *aValue || aVarChild.lVal != CHILDID_SELF) return hr;

  // If document is being used to create a widget, don't use the URL hack
  roles::Role role = Role();
  if (role != roles::DOCUMENT && role != roles::APPLICATION &&
      role != roles::DIALOG && role != roles::ALERT &&
      role != roles::NON_NATIVE_DOCUMENT)
    return hr;

  nsAutoString url;
  URL(url);
  if (url.IsEmpty()) return S_FALSE;

  *aValue = ::SysAllocStringLen(url.get(), url.Length());
  return *aValue ? S_OK : E_OUTOFMEMORY;
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

void DocAccessibleWrap::Shutdown() {
  // Do window emulation specific shutdown if emulation was started.
  if (nsWinUtils::IsWindowEmulationStarted()) {
    // Destroy window created for root document.
    if (mDocFlags & eTopLevelContentDocInProcess) {
      MOZ_ASSERT(XRE_IsParentProcess());
      HWND hWnd = static_cast<HWND>(mHWND);
      ::RemovePropW(hWnd, kPropNameDocAcc);
      ::DestroyWindow(hWnd);
    }

    mHWND = nullptr;
  }

  DocAccessible::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// DocAccessible public

void* DocAccessibleWrap::GetNativeWindow() const {
  if (XRE_IsContentProcess()) {
    DocAccessibleChild* ipcDoc = IPCDoc();
    if (!ipcDoc) {
      return nullptr;
    }

    return ipcDoc->GetNativeWindowHandle();
  } else if (mHWND) {
    return mHWND;
  }
  return DocAccessible::GetNativeWindow();
}

////////////////////////////////////////////////////////////////////////////////
// DocAccessible protected

void DocAccessibleWrap::DoInitialUpdate() {
  DocAccessible::DoInitialUpdate();

  if (nsWinUtils::IsWindowEmulationStarted()) {
    // Create window for tab document.
    if (mDocFlags & eTopLevelContentDocInProcess) {
      MOZ_ASSERT(XRE_IsParentProcess());
      a11y::RootAccessible* rootDocument = RootAccessible();
      bool isActive = true;
      nsIntRect rect(CW_USEDEFAULT, CW_USEDEFAULT, 0, 0);
      if (Compatibility::IsDolphin()) {
        rect = Bounds();
        nsIntRect rootRect = rootDocument->Bounds();
        rect.MoveToX(rootRect.X() - rect.X());
        rect.MoveByY(-rootRect.Y());

        nsCOMPtr<nsISupports> container = mDocumentNode->GetContainer();
        nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);
        docShell->GetIsActive(&isActive);
      }

      RefPtr<DocAccessibleWrap> self(this);
      nsWinUtils::NativeWindowCreateProc onCreate([self](HWND aHwnd) -> void {
        ::SetPropW(aHwnd, kPropNameDocAcc,
                   reinterpret_cast<HANDLE>(self.get()));
      });

      HWND parentWnd = reinterpret_cast<HWND>(rootDocument->GetNativeWindow());
      mHWND = nsWinUtils::CreateNativeWindow(
          kClassNameTabContent, parentWnd, rect.X(), rect.Y(), rect.Width(),
          rect.Height(), isActive, &onCreate);
    } else {
      DocAccessible* parentDocument = ParentDocument();
      if (parentDocument) mHWND = parentDocument->GetNativeWindow();
    }
  }
}
