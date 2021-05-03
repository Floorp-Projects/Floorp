/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MsaaDocAccessible.h"

#include "DocAccessibleChild.h"
#include "nsWinUtils.h"
#include "Role.h"

using namespace mozilla;
using namespace mozilla::a11y;

DocAccessible* MsaaDocAccessible::DocAcc() {
  return static_cast<DocAccessible*>(LocalAcc());
}

/* static */
MsaaDocAccessible* MsaaDocAccessible::GetFrom(DocAccessible* aDoc) {
  return static_cast<MsaaDocAccessible*>(MsaaAccessible::GetFrom(aDoc));
}

// IUnknown
IMPL_IUNKNOWN_QUERY_HEAD(MsaaDocAccessible)
if (aIID == IID_ISimpleDOMDocument) {
  statistics::ISimpleDOMUsed();
  *aInstancePtr = static_cast<ISimpleDOMDocument*>(new sdnDocAccessible(this));
  static_cast<IUnknown*>(*aInstancePtr)->AddRef();
  return S_OK;
}
IMPL_IUNKNOWN_QUERY_TAIL_INHERITED(ia2AccessibleHypertext)

STDMETHODIMP
MsaaDocAccessible::get_accParent(
    /* [retval][out] */ IDispatch __RPC_FAR* __RPC_FAR* ppdispParent) {
  DocAccessible* docAcc = DocAcc();
  if (!docAcc) {
    return CO_E_OBJNOTCONNECTED;
  }

  // We might be a top-level document in a content process.
  DocAccessibleChild* ipcDoc = docAcc->IPCDoc();
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
      (!docAcc->ParentDocument() ||
       (nsWinUtils::IsWindowEmulationStarted() &&
        nsCoreUtils::IsTopLevelContentDocInProcess(docAcc->DocumentNode())))) {
    HWND hwnd = static_cast<HWND>(docAcc->GetNativeWindow());
    if (hwnd && !docAcc->ParentDocument()) {
      nsIFrame* frame = docAcc->GetFrame();
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

  return MsaaAccessible::get_accParent(ppdispParent);
}

STDMETHODIMP
MsaaDocAccessible::get_accValue(VARIANT aVarChild, BSTR __RPC_FAR* aValue) {
  if (!aValue) return E_INVALIDARG;
  *aValue = nullptr;

  // For backwards-compat, we still support old MSAA hack to provide URL in
  // accValue Check for real value first
  HRESULT hr = MsaaAccessible::get_accValue(aVarChild, aValue);
  if (FAILED(hr) || *aValue || aVarChild.lVal != CHILDID_SELF) return hr;

  DocAccessible* docAcc = DocAcc();
  // MsaaAccessible::get_accValue should have failed (and thus we should have
  // returned early) if the Accessible is dead.
  MOZ_ASSERT(docAcc);
  // If document is being used to create a widget, don't use the URL hack
  roles::Role role = docAcc->Role();
  if (role != roles::DOCUMENT && role != roles::APPLICATION &&
      role != roles::DIALOG && role != roles::ALERT &&
      role != roles::NON_NATIVE_DOCUMENT)
    return hr;

  nsAutoString url;
  docAcc->URL(url);
  if (url.IsEmpty()) return S_FALSE;

  *aValue = ::SysAllocStringLen(url.get(), url.Length());
  return *aValue ? S_OK : E_OUTOFMEMORY;
}
