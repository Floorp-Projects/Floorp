/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef IEBrowser_H
#define IEBrowser_H

#include <docobj.h>
#include <ExDisp.h>

class IEBrowser :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IDispatchImpl<IWebBrowser2, &IID_IWebBrowser2, &LIBID_MSHTML>
{
public:
BEGIN_COM_MAP(IEBrowser)
    COM_INTERFACE_ENTRY(IWebBrowser)
    COM_INTERFACE_ENTRY(IWebBrowser2)
    COM_INTERFACE_ENTRY(IWebBrowserApp)
END_COM_MAP()

    IEBrowser();
    virtual ~IEBrowser();

public:
// IWebBrowser
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE GoBack(void);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE GoForward(void);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE GoHome(void);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE GoSearch(void);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE Navigate(
        /* [in] */ BSTR URL,
        /* [optional][in] */ VARIANT *Flags,
        /* [optional][in] */ VARIANT *TargetFrameName,
        /* [optional][in] */ VARIANT *PostData,
        /* [optional][in] */ VARIANT *Headers);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE Refresh(void);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE Refresh2(
        /* [optional][in] */ VARIANT *Level);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE Stop(void);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Application(
        /* [retval][out] */ IDispatch **ppDisp);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Parent(
        /* [retval][out] */ IDispatch **ppDisp);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Container(
        /* [retval][out] */ IDispatch **ppDisp);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Document(
        /* [retval][out] */ IDispatch **ppDisp);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_TopLevelContainer(
        /* [retval][out] */ VARIANT_BOOL *pBool);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Type(
        /* [retval][out] */ BSTR *Type);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Left(
        /* [retval][out] */ long *pl);
    
    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Left(
        /* [in] */ long Left);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Top(
        /* [retval][out] */ long *pl);
    
    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Top(
        /* [in] */ long Top);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Width(
        /* [retval][out] */ long *pl);
    
    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Width(
        /* [in] */ long Width);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Height(
        /* [retval][out] */ long *pl);
    
    virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Height(
        /* [in] */ long Height);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_LocationName(
        /* [retval][out] */ BSTR *LocationName);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_LocationURL(
        /* [retval][out] */ BSTR *LocationURL);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Busy(
        /* [retval][out] */ VARIANT_BOOL *pBool);
        
// IWebBrowserApp
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE Quit(void);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE ClientToWindow(
        /* [out][in] */ int *pcx,
        /* [out][in] */ int *pcy);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE PutProperty(
        /* [in] */ BSTR Property,
        /* [in] */ VARIANT vtValue);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE GetProperty(
        /* [in] */ BSTR Property,
        /* [retval][out] */ VARIANT *pvtValue);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Name(
        /* [retval][out] */ BSTR *Name);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_HWND(
        /* [retval][out] */ long __RPC_FAR *pHWND);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_FullName(
        /* [retval][out] */ BSTR *FullName);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Path(
        /* [retval][out] */ BSTR *Path);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Visible(
        /* [retval][out] */ VARIANT_BOOL *pBool);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Visible(
        /* [in] */ VARIANT_BOOL Value);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_StatusBar(
        /* [retval][out] */ VARIANT_BOOL *pBool);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_StatusBar(
        /* [in] */ VARIANT_BOOL Value);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_StatusText(
        /* [retval][out] */ BSTR *StatusText);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_StatusText(
        /* [in] */ BSTR StatusText);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_ToolBar(
        /* [retval][out] */ int *Value);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_ToolBar(
        /* [in] */ int Value);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_MenuBar(
        /* [retval][out] */ VARIANT_BOOL *Value);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_MenuBar(
        /* [in] */ VARIANT_BOOL Value);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_FullScreen(
        /* [retval][out] */ VARIANT_BOOL *pbFullScreen);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_FullScreen(
        /* [in] */ VARIANT_BOOL bFullScreen);
    
// IWebBrowser2
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE Navigate2(
        /* [in] */ VARIANT *URL,
        /* [optional][in] */ VARIANT *Flags,
        /* [optional][in] */ VARIANT *TargetFrameName,
        /* [optional][in] */ VARIANT *PostData,
        /* [optional][in] */ VARIANT *Headers);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE QueryStatusWB(
        /* [in] */ OLECMDID cmdID,
        /* [retval][out] */ OLECMDF *pcmdf);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE ExecWB(
        /* [in] */ OLECMDID cmdID,
        /* [in] */ OLECMDEXECOPT cmdexecopt,
        /* [optional][in] */ VARIANT *pvaIn,
        /* [optional][in][out] */ VARIANT *pvaOut);
    
    virtual /* [helpcontext][helpstring][id] */ HRESULT STDMETHODCALLTYPE ShowBrowserBar(
        /* [in] */ VARIANT *pvaClsid,
        /* [optional][in] */ VARIANT *pvarShow,
        /* [optional][in] */ VARIANT *pvarSize);
    
    virtual /* [bindable][propget][id] */ HRESULT STDMETHODCALLTYPE get_ReadyState(
        /* [out][retval] */ READYSTATE *plReadyState);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Offline(
        /* [retval][out] */ VARIANT_BOOL *pbOffline);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Offline(
        /* [in] */ VARIANT_BOOL bOffline);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Silent(
        /* [retval][out] */ VARIANT_BOOL *pbSilent);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Silent(
        /* [in] */ VARIANT_BOOL bSilent);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_RegisterAsBrowser(
        /* [retval][out] */ VARIANT_BOOL *pbRegister);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_RegisterAsBrowser(
        /* [in] */ VARIANT_BOOL bRegister);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_RegisterAsDropTarget(
        /* [retval][out] */ VARIANT_BOOL *pbRegister);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_RegisterAsDropTarget(
        /* [in] */ VARIANT_BOOL bRegister);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_TheaterMode(
        /* [retval][out] */ VARIANT_BOOL *pbRegister);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_TheaterMode(
        /* [in] */ VARIANT_BOOL bRegister);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_AddressBar(
        /* [retval][out] */ VARIANT_BOOL *Value);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_AddressBar(
        /* [in] */ VARIANT_BOOL Value);
    
    virtual /* [helpcontext][helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Resizable(
        /* [retval][out] */ VARIANT_BOOL *Value);
    
    virtual /* [helpcontext][helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Resizable(
        /* [in] */ VARIANT_BOOL Value);
};


#endif