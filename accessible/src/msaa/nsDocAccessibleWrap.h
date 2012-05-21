/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _nsDocAccessibleWrap_H_
#define _nsDocAccessibleWrap_H_

#include "ISimpleDOMDocument.h"

#include "nsAccUtils.h"
#include "nsDocAccessible.h"
#include "nsIDocShellTreeItem.h"

class nsDocAccessibleWrap: public nsDocAccessible,
                           public ISimpleDOMDocument
{
public:
  nsDocAccessibleWrap(nsIDocument* aDocument, nsIContent* aRootContent,
                      nsIPresShell* aPresShell);
  virtual ~nsDocAccessibleWrap();

    // IUnknown
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP      QueryInterface(REFIID, void**);

    // ISimpleDOMDocument
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_URL( 
        /* [out] */ BSTR __RPC_FAR *url);
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_title( 
        /* [out] */ BSTR __RPC_FAR *title);
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_mimeType( 
        /* [out] */ BSTR __RPC_FAR *mimeType);
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_docType( 
        /* [out] */ BSTR __RPC_FAR *docType);
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nameSpaceURIForID( 
        /* [in] */ short nameSpaceID,
        /* [out] */ BSTR __RPC_FAR *nameSpaceURI);

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE put_alternateViewMediaTypes( 
        /* [in] */ BSTR __RPC_FAR *commaSeparatedMediaTypes);

    // IAccessible

    // Override get_accValue to provide URL when no other value is available
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accValue( 
        /* [optional][in] */ VARIANT varChild,
        /* [retval][out] */ BSTR __RPC_FAR *pszValue);

  // nsAccessNode
  virtual void Shutdown();

  // nsDocAccessible
  virtual void* GetNativeWindow() const;

protected:
  // nsDocAccessible
  virtual void DoInitialUpdate();

protected:
  void* mHWND;
};

#endif
