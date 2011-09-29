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

#ifndef _nsAccessNodeWrap_H_
#define _nsAccessNodeWrap_H_

// Avoid warning C4509:
// nonstandard extension used: 'nsAccessibleWrap::[methodname]' 
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

typedef LRESULT (STDAPICALLTYPE *LPFNNOTIFYWINEVENT)(DWORD event,HWND hwnd,LONG idObjectType,LONG idObject);
typedef LRESULT (STDAPICALLTYPE *LPFNGETGUITHREADINFO)(DWORD idThread, GUITHREADINFO* pgui);

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
  nsAccessNodeWrap(nsIContent *aContent, nsIWeakReference *aShell);
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

    /// the accessible library and cached methods
    static HINSTANCE gmAccLib;
    static HINSTANCE gmUserLib;
    static LPFNACCESSIBLEOBJECTFROMWINDOW gmAccessibleObjectFromWindow;
    static LPFNLRESULTFROMOBJECT gmLresultFromObject;
    static LPFNNOTIFYWINEVENT gmNotifyWinEvent;
    static LPFNGETGUITHREADINFO gmGetGUIThreadInfo;

    static int FilterA11yExceptions(unsigned int aCode, EXCEPTION_POINTERS *aExceptionInfo);

    static bool IsOnlyMsaaCompatibleJawsPresent();

    static void TurnOffNewTabSwitchingForJawsAndWE();

    static void DoATSpecificProcessing();

  static STDMETHODIMP_(LRESULT) LresultFromObject(REFIID riid, WPARAM wParam, LPUNKNOWN pAcc);

  static LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg,
                                     WPARAM WParam, LPARAM lParam);

  static nsRefPtrHashtable<nsVoidPtrHashKey, nsDocAccessible> sHWNDCache;

protected:

  /**
   * Return ISimpleDOMNode instance for existing accessible object or
   * creates new nsAccessNode instance if the accessible doesn't exist.
   *
   * @note ISimpleDOMNode is returned addrefed
   */
  ISimpleDOMNode *MakeAccessNode(nsINode *aNode);

    /**
     * Used to determine whether an IAccessible2 compatible screen reader is
     * loaded. Currently used for JAWS versions older than 8.0.2173.
     */
     static bool gIsIA2Disabled;

    /**
     * It is used in nsHyperTextAccessibleWrap for IA2::newText/oldText
     * implementation.
     */
    static AccTextChangeEvent* gTextEvent;
};

/**
 * Converts nsresult to HRESULT.
 */
HRESULT GetHRESULT(nsresult aResult);

#endif

