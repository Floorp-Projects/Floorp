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
 * Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _nsAccessNodeWrap_H_
#define _nsAccessNodeWrap_H_

#include "nsCOMPtr.h"
#include "nsIAccessible.h"
#include "ISimpleDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsAccessNode.h"
#include "OLEIDL.H"
#include "OLEACC.H"
#include "winable.h"
#undef ERROR /// Otherwise we can't include nsIDOMNSEvent.h if we include this

typedef LRESULT (STDAPICALLTYPE *LPFNNOTIFYWINEVENT)(DWORD event,HWND hwnd,LONG idObjectType,LONG idObject);
typedef LRESULT (STDAPICALLTYPE *LPFNGETGUITHREADINFO)(DWORD idThread, GUITHREADINFO* pgui);

class nsAccessNodeWrap :  public nsAccessNode, public ISimpleDOMNode
{
  public: // construction, destruction
    nsAccessNodeWrap(nsIDOMNode *, nsIWeakReference* aShell);
    virtual ~nsAccessNodeWrap();

    // IUnknown methods - see iunknown.h for documentation
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
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
        
    static void InitAccessibility();
    static void ShutdownAccessibility();

    /// the accessible library and cached methods
    static HINSTANCE gmAccLib;
    static HINSTANCE gmUserLib;
    static LPFNACCESSIBLEOBJECTFROMWINDOW gmAccessibleObjectFromWindow;
    static LPFNNOTIFYWINEVENT gmNotifyWinEvent;
    static LPFNGETGUITHREADINFO gmGetGUIThreadInfo;

  protected:
    void GetAccessibleFor(nsIDOMNode *node, nsIAccessible **newAcc);
    ISimpleDOMNode* MakeAccessNode(nsIDOMNode *node);
    NS_IMETHOD GetComputedStyleDeclaration(nsIDOMCSSStyleDeclaration **aCssDecl, PRUint32 *aLength);

    static PRBool gIsEnumVariantSupportDisabled;
};

#endif

