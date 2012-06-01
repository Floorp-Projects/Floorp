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
#include "ISimpleDOMNode.h"
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


class AccTextChangeEvent;

class nsAccessNodeWrap :  public nsAccessNode,
                          public nsIWinAccessNode,
                          public ISimpleDOMNode,
                          public IServiceProvider
{
  public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIWINACCESSNODE

  public: // IServiceProvider
    STDMETHODIMP QueryService(REFGUID guidService, REFIID riid, void** ppv);

public: // construction, destruction
  nsAccessNodeWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~nsAccessNodeWrap();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID, void**);

  public:

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nodeInfo( 
        /* [out] */ BSTR __RPC_FAR *tagName,
        /* [out] */ short __RPC_FAR *nameSpaceID,
        /* [out] */ BSTR __RPC_FAR *nodeValue,
        /* [out] */ unsigned int __RPC_FAR *numChildren,
        /* [out] */ unsigned int __RPC_FAR *aUniqueID,
        /* [out][retval] */ unsigned short __RPC_FAR *nodeType);
  
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_attributes( 
        /* [in] */ unsigned short maxAttribs,
        /* [length_is][size_is][out] */ BSTR __RPC_FAR *attribNames,
        /* [length_is][size_is][out] */ short __RPC_FAR *nameSpaceID,
        /* [length_is][size_is][out] */ BSTR __RPC_FAR *attribValues,
        /* [out][retval] */ unsigned short __RPC_FAR *numAttribs);
  
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_attributesForNames( 
        /* [in] */ unsigned short maxAttribs,
        /* [length_is][size_is][in] */ BSTR __RPC_FAR *attribNames,
        /* [length_is][size_is][in] */ short __RPC_FAR *nameSpaceID,
        /* [length_is][size_is][retval] */ BSTR __RPC_FAR *attribValues);
  
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_computedStyle( 
        /* [in] */ unsigned short maxStyleProperties,
        /* [in] */ boolean useAlternateView,
        /* [length_is][size_is][out] */ BSTR __RPC_FAR *styleProperties,
        /* [length_is][size_is][out] */ BSTR __RPC_FAR *styleValues,
        /* [out][retval] */ unsigned short __RPC_FAR *numStyleProperties);
  
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_computedStyleForProperties( 
        /* [in] */ unsigned short numStyleProperties,
        /* [in] */ boolean useAlternateView,
        /* [length_is][size_is][in] */ BSTR __RPC_FAR *styleProperties,
        /* [length_is][size_is][out][retval] */ BSTR __RPC_FAR *styleValues);
        
    virtual HRESULT STDMETHODCALLTYPE scrollTo(/* [in] */ boolean scrollTopLeft);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_parentNode(ISimpleDOMNode __RPC_FAR *__RPC_FAR *node);
    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_firstChild(ISimpleDOMNode __RPC_FAR *__RPC_FAR *node);
    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_lastChild(ISimpleDOMNode __RPC_FAR *__RPC_FAR *node);
    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_previousSibling(ISimpleDOMNode __RPC_FAR *__RPC_FAR *node);
    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nextSibling(ISimpleDOMNode __RPC_FAR *__RPC_FAR *node);
    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_childAt(unsigned childIndex,
                                                                  ISimpleDOMNode __RPC_FAR *__RPC_FAR *node);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_innerHTML(
        /* [out][retval] */ BSTR __RPC_FAR *innerHTML);

    virtual /* [local][propget] */ HRESULT STDMETHODCALLTYPE get_localInterface( 
        /* [retval][out] */ void __RPC_FAR *__RPC_FAR *localInterface);
        
    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_language(
        /* [out][retval] */ BSTR __RPC_FAR *language);

    static void InitAccessibility();
    static void ShutdownAccessibility();

    static int FilterA11yExceptions(unsigned int aCode, EXCEPTION_POINTERS *aExceptionInfo);

  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg,
                                     WPARAM WParam, LPARAM lParam);

  static nsRefPtrHashtable<nsPtrHashKey<void>, DocAccessible> sHWNDCache;

protected:

  /**
   * Return ISimpleDOMNode instance for existing accessible object or
   * creates new nsAccessNode instance if the accessible doesn't exist.
   *
   * @note ISimpleDOMNode is returned addrefed
   */
  ISimpleDOMNode *MakeAccessNode(nsINode *aNode);

    /**
     * It is used in HyperTextAccessibleWrap for IA2::newText/oldText
     * implementation.
     */
    static AccTextChangeEvent* gTextEvent;
};

/**
 * Converts nsresult to HRESULT.
 */
HRESULT GetHRESULT(nsresult aResult);

#endif

