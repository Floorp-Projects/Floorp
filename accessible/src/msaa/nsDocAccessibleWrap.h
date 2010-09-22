/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  nsDocAccessibleWrap(nsIDocument *aDocument, nsIContent *aRootContent,
                      nsIWeakReference *aShell);
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
  virtual PRBool Init();
  virtual void Shutdown();

  // nsAccessibleWrap
  virtual nsAccessible *GetXPAccessibleFor(const VARIANT& varChild);

  // nsDocAccessible
  virtual void* GetNativeWindow() const;

protected:
  void* mHWND;
};

#endif
