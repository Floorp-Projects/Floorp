/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation of the accessibility architecture,
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _nsAccessNodeWrap_H_
#define _nsAccessNodeWrap_H_

// Avoid warning C4509:
// nonstandard extension used: 'AccessibleWrap::[methodname]' 
// uses SEH and 'xpAccessible' has destructor
// At this point we're catching a crash which is of much greater
// importance than the missing dereference for the nsCOMPtr<>
#ifdef _MSC_VER
#pragma warning( disable : 4509 )
#endif

#include "nsCOMPtr.h"
#include "nsIAccessible.h"
#include "nsIAccessibleEvent.h"
#include "nsIWinAccessNode.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsAccessNode.h"
#include "oleidl.h"
#include "oleacc.h"
#include <winuser.h>
#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#endif

#include "nsRefPtrHashtable.h"

#define A11Y_TRYBLOCK_BEGIN                                                    \
  MOZ_SEH_TRY {

#define A11Y_TRYBLOCK_END                                                             \
  } MOZ_SEH_EXCEPT(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),       \
                                                          GetExceptionInformation())) \
  { }                                                                                 \
  return E_FAIL;

namespace mozilla {
namespace a11y {

#ifdef __GNUC__
// Inheriting from both XPCOM and MSCOM interfaces causes a lot of warnings
// about virtual functions being hidden by each other. This is done by
// design, so silence the warning.
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

class nsAccessNodeWrap : public nsAccessNode,
                         public nsIWinAccessNode,
                         public IServiceProvider
{
  public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIWINACCESSNODE

public: // construction, destruction
  nsAccessNodeWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~nsAccessNodeWrap();

  // IUnknown
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID aIID,
                                                   void** aInstancePtr);

  // IServiceProvider
  virtual HRESULT STDMETHODCALLTYPE QueryService(REFGUID aGuidService,
                                                 REFIID aIID,
                                                 void** aInstancePtr);

    static int FilterA11yExceptions(unsigned int aCode, EXCEPTION_POINTERS *aExceptionInfo);

  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg,
                                     WPARAM WParam, LPARAM lParam);

  static nsRefPtrHashtable<nsPtrHashKey<void>, DocAccessible> sHWNDCache;
};

} // namespace a11y
} // namespace mozilla

/**
 * Converts nsresult to HRESULT.
 */
HRESULT GetHRESULT(nsresult aResult);

#endif

