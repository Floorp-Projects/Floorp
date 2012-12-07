/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessNodeWrap.h"

#include "AccessibleApplication.h"
#include "ApplicationAccessibleWrap.h"
#include "sdnAccessible.h"

#include "Compatibility.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsWinUtils.h"
#include "RootAccessible.h"
#include "Statistics.h"

#include "nsAttrName.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLElement.h"
#include "nsINameSpaceManager.h"
#include "nsPIDOMWindow.h"
#include "nsIServiceManager.h"

using namespace mozilla;
using namespace mozilla::a11y;

AccTextChangeEvent* nsAccessNodeWrap::gTextEvent = nullptr;

////////////////////////////////////////////////////////////////////////////////
// nsAccessNodeWrap
////////////////////////////////////////////////////////////////////////////////

nsAccessNodeWrap::
  nsAccessNodeWrap(nsIContent* aContent, DocAccessible* aDoc) :
  nsAccessNode(aContent, aDoc)
{
}

nsAccessNodeWrap::~nsAccessNodeWrap()
{
}

//-----------------------------------------------------
// nsISupports methods
//-----------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED1(nsAccessNodeWrap, nsAccessNode, nsIWinAccessNode)

//-----------------------------------------------------
// nsIWinAccessNode methods
//-----------------------------------------------------

NS_IMETHODIMP
nsAccessNodeWrap::QueryNativeInterface(REFIID aIID, void** aInstancePtr)
{
  // XXX Wrong for E_NOINTERFACE
  return static_cast<nsresult>(QueryInterface(aIID, aInstancePtr));
}

STDMETHODIMP nsAccessNodeWrap::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = nullptr;

  if (IID_IUnknown == iid) {
    *ppv = static_cast<IUnknown*>(this);
  } else {
    return E_NOINTERFACE; //iid not supported.
  }

  (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
  return S_OK;
}

STDMETHODIMP
nsAccessNodeWrap::QueryService(REFGUID guidService, REFIID iid, void** ppv)
{
  *ppv = nullptr;

  // Provide a special service ID for getting the accessible for the browser tab
  // document that contains this accessible object. If this accessible object
  // is not inside a browser tab then the service fails with E_NOINTERFACE.
  // A use case for this is for screen readers that need to switch context or
  // 'virtual buffer' when focus moves from one browser tab area to another.
  static const GUID SID_IAccessibleContentDocument =
    { 0xa5d8e1f3,0x3571,0x4d8f,{0x95,0x21,0x07,0xed,0x28,0xfb,0x07,0x2e} };
  if (guidService == SID_IAccessibleContentDocument) {
    if (iid != IID_IAccessible)
      return E_NOINTERFACE;

    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = 
      nsCoreUtils::GetDocShellTreeItemFor(mContent);
    if (!docShellTreeItem)
      return E_UNEXPECTED;

    // Walk up the parent chain without crossing the boundary at which item
    // types change, preventing us from walking up out of tab content.
    nsCOMPtr<nsIDocShellTreeItem> root;
    docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
    if (!root)
      return E_UNEXPECTED;


    // If the item type is typeContent, we assume we are in browser tab content.
    // Note this includes content such as about:addons, for consistency.
    int32_t itemType;
    root->GetItemType(&itemType);
    if (itemType != nsIDocShellTreeItem::typeContent)
      return E_NOINTERFACE;

    // Make sure this is a document.
    DocAccessible* docAcc = nsAccUtils::GetDocAccessibleFor(root);
    if (!docAcc)
      return E_UNEXPECTED;

    *ppv = static_cast<IAccessible*>(docAcc);

    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  // Can get to IAccessibleApplication from any node via QS
  if (guidService == IID_IAccessibleApplication ||
      (Compatibility::IsJAWS() && iid == IID_IAccessibleApplication)) {
    ApplicationAccessible* applicationAcc = ApplicationAcc();
    if (!applicationAcc)
      return E_NOINTERFACE;

    nsresult rv = applicationAcc->QueryNativeInterface(iid, ppv);
    return NS_SUCCEEDED(rv) ? S_OK : E_NOINTERFACE;
  }

  /**
   * To get an ISimpleDOMNode, ISimpleDOMDocument, ISimpleDOMText
   * or any IAccessible2 interface on should use IServiceProvider like this:
   * -----------------------------------------------------------------------
   * ISimpleDOMDocument *pAccDoc = NULL;
   * IServiceProvider *pServProv = NULL;
   * pAcc->QueryInterface(IID_IServiceProvider, (void**)&pServProv);
   * if (pServProv) {
   *   const GUID unused;
   *   pServProv->QueryService(unused, IID_ISimpleDOMDocument, (void**)&pAccDoc);
   *   pServProv->Release();
   * }
   */

  static const GUID IID_SimpleDOMDeprecated =
    { 0x0c539790,0x12e4,0x11cf,{0xb6,0x61,0x00,0xaa,0x00,0x4c,0xd6,0xd8} };
  if (guidService == IID_ISimpleDOMNode ||
      guidService == IID_SimpleDOMDeprecated ||
      guidService == IID_IAccessible ||  guidService == IID_IAccessible2)
    return QueryInterface(iid, ppv);

  return E_INVALIDARG;
}
 
int nsAccessNodeWrap::FilterA11yExceptions(unsigned int aCode, EXCEPTION_POINTERS *aExceptionInfo)
{
  if (aCode == EXCEPTION_ACCESS_VIOLATION) {
#ifdef MOZ_CRASHREPORTER
    // MSAA swallows crashes (because it is COM-based)
    // but we still need to learn about those crashes so we can fix them
    // Make sure to pass them to the crash reporter
    nsCOMPtr<nsICrashReporter> crashReporter =
      do_GetService("@mozilla.org/toolkit/crash-reporter;1");
    if (crashReporter) {
      crashReporter->WriteMinidumpForException(aExceptionInfo);
    }
#endif
  }
  else {
    NS_NOTREACHED("We should only be catching crash exceptions");
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

HRESULT
GetHRESULT(nsresult aResult)
{
  switch (aResult) {
    case NS_OK:
      return S_OK;

    case NS_ERROR_INVALID_ARG: case NS_ERROR_INVALID_POINTER:
      return E_INVALIDARG;

    case NS_ERROR_OUT_OF_MEMORY:
      return E_OUTOFMEMORY;

    case NS_ERROR_NOT_IMPLEMENTED:
      return E_NOTIMPL;

    default:
      return E_FAIL;
  }
}

nsRefPtrHashtable<nsPtrHashKey<void>, DocAccessible> nsAccessNodeWrap::sHWNDCache;

LRESULT CALLBACK
nsAccessNodeWrap::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  // Note, this window's message handling should not invoke any call that
  // may result in a cross-process ipc call. Doing so may violate RPC
  // message semantics.

  switch (msg) {
    case WM_GETOBJECT:
    {
      if (lParam == OBJID_CLIENT) {
        DocAccessible* document = sHWNDCache.GetWeak(static_cast<void*>(hWnd));
        if (document) {
          IAccessible* msaaAccessible = NULL;
          document->GetNativeInterface((void**)&msaaAccessible); // does an addref
          if (msaaAccessible) {
            LRESULT result = ::LresultFromObject(IID_IAccessible, wParam,
                                                 msaaAccessible); // does an addref
            msaaAccessible->Release(); // release extra addref
            return result;
          }
        }
      }
      return 0;
    }
    case WM_NCHITTEST:
    {
      LRESULT lRet = ::DefWindowProc(hWnd, msg, wParam, lParam);
      if (HTCLIENT == lRet)
        lRet = HTTRANSPARENT;
      return lRet;
    }
  }

  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
