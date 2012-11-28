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
#pragma warning( disable : 4509 )

#include "nsCOMPtr.h"
#include "nsIAccessible.h"
#include "nsIAccessibleEvent.h"
#include "nsIWinAccessNode.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsAccessNode.h"
#include "OLEIDL.H"
#include "OLEACC.H"
#include <winuser.h>
#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#endif

#include "nsRefPtrHashtable.h"

#define A11Y_TRYBLOCK_BEGIN                                                    \
  __try {

#define A11Y_TRYBLOCK_END                                                      \
  } __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),      \
                                                    GetExceptionInformation()))\
  { }                                                                          \
  return E_FAIL;

namespace mozilla {
namespace a11y {

class AccTextChangeEvent;

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


    static void InitAccessibility();
    static void ShutdownAccessibility();

    static int FilterA11yExceptions(unsigned int aCode, EXCEPTION_POINTERS *aExceptionInfo);

  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg,
                                     WPARAM WParam, LPARAM lParam);

  static nsRefPtrHashtable<nsPtrHashKey<void>, DocAccessible> sHWNDCache;

protected:

    /**
     * It is used in HyperTextAccessibleWrap for IA2::newText/oldText
     * implementation.
     */
    static AccTextChangeEvent* gTextEvent;
};

} // namespace a11y
} // namespace mozilla

/**
 * Converts nsresult to HRESULT.
 */
HRESULT GetHRESULT(nsresult aResult);

#endif

