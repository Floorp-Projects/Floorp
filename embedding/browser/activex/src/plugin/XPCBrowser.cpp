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

#include "StdAfx.h"

#include "npapi.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMLocation.h"

#include "XPCBrowser.h"

IEBrowser::IEBrowser()
{
}

IEBrowser::~IEBrowser()
{
}

HRESULT IEBrowser::Init(PluginInstanceData *pData)
{
    mData = pData;

    // Get the location URL
    nsCOMPtr<nsIDOMWindow> window;
    NPN_GetValue(mData->pPluginInstance, NPNVDOMWindow, (void *) &window);
    if (window)
    {
        nsCOMPtr<nsIDOMWindowInternal> windowInternal = do_QueryInterface(window);
        if (windowInternal)
        {
            nsCOMPtr<nsIDOMLocation> location;
            nsAutoString href;
            windowInternal->GetLocation(getter_AddRefs(location));
            if (location &&
                NS_SUCCEEDED(location->GetHref(href)))
            {
                const PRUnichar *s = href.get();
                mLocationURL.Attach(::SysAllocString(s));
            }
        }
    }
    
    return S_OK;
}

// IWebBrowser

HRESULT STDMETHODCALLTYPE IEBrowser::GoBack(void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::GoForward(void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::GoHome(void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::GoSearch(void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::Navigate(
    BSTR URL,
    VARIANT *Flags,
    VARIANT *TargetFrameName,
    VARIANT *PostData,
    VARIANT *Headers)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::Refresh(void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::Refresh2(
    VARIANT *Level)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::Stop(void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Application(
    IDispatch **ppDisp)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Parent(
    IDispatch **ppDisp)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Container(
    IDispatch **ppDisp)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Document(
    IDispatch **ppDisp)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_TopLevelContainer(
    VARIANT_BOOL *pBool)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Type(
    BSTR *Type)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Left(
    long *pl)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_Left(
    long Left)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Top(
    long *pl)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_Top(
long Top)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Width(
    long *pl)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_Width(
    long Width)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Height(
    long *pl)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_Height(
    long Height)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_LocationName(
    BSTR *LocationName)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_LocationURL(
    BSTR *LocationURL)
{
    *LocationURL = mLocationURL.Copy();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Busy(
    VARIANT_BOOL *pBool)
{
    return E_NOTIMPL;
}

// IWebBrowserApp
HRESULT STDMETHODCALLTYPE IEBrowser::Quit(void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::ClientToWindow(
    int *pcx,
    int *pcy)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::PutProperty(
    BSTR Property,
    VARIANT vtValue)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::GetProperty(
    BSTR Property,
    VARIANT *pvtValue)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Name(
    BSTR *Name)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_HWND(
    long __RPC_FAR *pHWND)
{
    HWND hwnd = NULL;
    NPN_GetValue(mData->pPluginInstance, NPNVnetscapeWindow, &hwnd);
    *((HWND *)pHWND) = hwnd;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_FullName(
    BSTR *FullName)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Path(
    BSTR *Path)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Visible(
    VARIANT_BOOL *pBool)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_Visible(
    VARIANT_BOOL Value)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_StatusBar(
    VARIANT_BOOL *pBool)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_StatusBar(
    VARIANT_BOOL Value)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_StatusText(
    BSTR *StatusText)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_StatusText(
    BSTR StatusText)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_ToolBar(
    int *Value)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_ToolBar(
    int Value)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_MenuBar(
    VARIANT_BOOL *Value)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_MenuBar(
    VARIANT_BOOL Value)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_FullScreen(
    VARIANT_BOOL *pbFullScreen)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_FullScreen(
    VARIANT_BOOL bFullScreen)
{
    return E_NOTIMPL;
}



// IWebBrowser2
HRESULT STDMETHODCALLTYPE IEBrowser::Navigate2(
    VARIANT *URL,
    VARIANT *Flags,
    VARIANT *TargetFrameName,
    VARIANT *PostData,
    VARIANT *Headers)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::QueryStatusWB(
    OLECMDID cmdID,
    OLECMDF *pcmdf)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::ExecWB(
    OLECMDID cmdID,
    OLECMDEXECOPT cmdexecopt,
    VARIANT *pvaIn,
    VARIANT *pvaOut)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::ShowBrowserBar(
    VARIANT *pvaClsid,
    VARIANT *pvarShow,
    VARIANT *pvarSize)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_ReadyState(
    READYSTATE *plReadyState)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Offline(
    VARIANT_BOOL *pbOffline)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_Offline(
    VARIANT_BOOL bOffline)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Silent(
    VARIANT_BOOL *pbSilent)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_Silent(
    VARIANT_BOOL bSilent)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_RegisterAsBrowser(
    VARIANT_BOOL *pbRegister)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_RegisterAsBrowser(
    VARIANT_BOOL bRegister)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_RegisterAsDropTarget(
    VARIANT_BOOL *pbRegister)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_RegisterAsDropTarget(
    VARIANT_BOOL bRegister)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_TheaterMode(
    VARIANT_BOOL *pbRegister)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_TheaterMode(
    VARIANT_BOOL bRegister)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_AddressBar(
    VARIANT_BOOL *Value)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_AddressBar(
    VARIANT_BOOL Value)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::get_Resizable(
    VARIANT_BOOL *Value)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE IEBrowser::put_Resizable(
    VARIANT_BOOL Value)
{
    return E_NOTIMPL;
}
