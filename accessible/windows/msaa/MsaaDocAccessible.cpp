/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MsaaDocAccessible.h"

#include "MsaaDocAccessible.h"
#include "DocAccessibleChild.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsWinUtils.h"
#include "Statistics.h"
#include "sdnDocAccessible.h"
#include "mozilla/a11y/Role.h"
#include "ISimpleDOM.h"

using namespace mozilla;
using namespace mozilla::a11y;

DocAccessible* MsaaDocAccessible::DocAcc() {
  return static_cast<DocAccessible*>(LocalAcc());
}

/* static */
MsaaDocAccessible* MsaaDocAccessible::GetFrom(DocAccessible* aDoc) {
  return static_cast<MsaaDocAccessible*>(MsaaAccessible::GetFrom(aDoc));
}

/* static */
MsaaDocAccessible* MsaaDocAccessible::GetFrom(DocAccessibleParent* aDoc) {
  return static_cast<MsaaDocAccessible*>(
      reinterpret_cast<MsaaAccessible*>(aDoc->GetWrapper()));
}

/* static */
MsaaDocAccessible* MsaaDocAccessible::GetFromOwned(Accessible* aAcc) {
  if (RemoteAccessible* remoteAcc = aAcc->AsRemote()) {
    DocAccessibleParent* doc = remoteAcc->Document();
    if (!doc) {
      return nullptr;
    }
    return MsaaDocAccessible::GetFrom(doc);
  }
  DocAccessible* doc = aAcc->AsLocal()->Document();
  if (!doc) {
    return nullptr;
  }
  return MsaaDocAccessible::GetFrom(doc);
}

// IUnknown
IMPL_IUNKNOWN_QUERY_HEAD(MsaaDocAccessible)
if (aIID == IID_ISimpleDOMDocument && LocalAcc()) {
  statistics::ISimpleDOMUsed();
  *aInstancePtr = static_cast<ISimpleDOMDocument*>(new sdnDocAccessible(this));
  static_cast<IUnknown*>(*aInstancePtr)->AddRef();
  return S_OK;
}
IMPL_IUNKNOWN_QUERY_TAIL_INHERITED(ia2AccessibleHypertext)

STDMETHODIMP
MsaaDocAccessible::get_accParent(
    /* [retval][out] */ IDispatch __RPC_FAR* __RPC_FAR* ppdispParent) {
  if (!mAcc) {
    return CO_E_OBJNOTCONNECTED;
  }

  if (mAcc->IsRemote()) {
    DocAccessibleParent* remoteDoc = mAcc->AsRemote()->AsDoc();
    if (nsWinUtils::IsWindowEmulationStarted() && remoteDoc->IsTopLevel()) {
      // Window emulation is enabled and this is a top level document. Return
      // window system accessible object.
      HWND hwnd = remoteDoc->GetEmulatedWindowHandle();
      MOZ_ASSERT(hwnd);
      if (hwnd &&
          SUCCEEDED(::CreateStdAccessibleObject(
              hwnd, OBJID_WINDOW, IID_IAccessible, (void**)ppdispParent))) {
        return S_OK;
      }
    }
    return MsaaAccessible::get_accParent(ppdispParent);
  }

  DocAccessible* docAcc = DocAcc();
  MOZ_ASSERT(docAcc);

  // Return window system accessible object for root document accessibles, as
  // well as tab document accessibles if window emulation is enabled.
  MOZ_ASSERT(XRE_IsParentProcess());
  if ((!docAcc->ParentDocument() ||
       (nsWinUtils::IsWindowEmulationStarted() &&
        nsCoreUtils::IsTopLevelContentDocInProcess(docAcc->DocumentNode())))) {
    HWND hwnd = static_cast<HWND>(docAcc->GetNativeWindow());
    if (hwnd && !docAcc->ParentDocument()) {
      nsIFrame* frame = docAcc->GetFrame();
      if (frame) {
        nsIWidget* widget = frame->GetNearestWidget();
        if (widget->GetWindowType() == widget::WindowType::Child &&
            !widget->GetParent()) {
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
        SUCCEEDED(::CreateStdAccessibleObject(
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

  // MsaaAccessible::get_accValue should have failed (and thus we should have
  // returned early) if the Accessible is dead.
  MOZ_ASSERT(mAcc);
  // If document is being used to create a widget, don't use the URL hack
  roles::Role role = mAcc->Role();
  if (role != roles::DOCUMENT && role != roles::APPLICATION &&
      role != roles::DIALOG && role != roles::ALERT &&
      role != roles::NON_NATIVE_DOCUMENT)
    return hr;

  nsAutoString url;
  nsAccUtils::DocumentURL(mAcc, url);
  if (url.IsEmpty()) return S_FALSE;

  *aValue = ::SysAllocStringLen(url.get(), url.Length());
  return *aValue ? S_OK : E_OUTOFMEMORY;
}
