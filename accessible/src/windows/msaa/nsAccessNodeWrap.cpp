/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccessNodeWrap.h"

#include "DocAccessible.h"
#include "nsWinUtils.h"

#include "nsPIDOMWindow.h"
#include "nsIServiceManager.h"

using namespace mozilla;
using namespace mozilla::a11y;

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

NS_IMPL_ISUPPORTS_INHERITED0(nsAccessNodeWrap, nsAccessNode)

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
          IAccessible* msaaAccessible = nullptr;
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
