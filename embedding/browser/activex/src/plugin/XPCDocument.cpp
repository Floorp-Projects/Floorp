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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
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

#include "StdAfx.h"

#include <mshtml.h>
#include <hlink.h>

// A barely documented interface called ITargetFrame from IE
// This is needed for targeted Hlink* calls (e.g. HlinkNavigateString) to
// work. During the call, the Hlink API QIs and calls ITargetFrame::FindFrame
// to determine the named target frame before calling IHlinkFrame::Navigate.
//
// MS mentions the methods at the url below:
//
// http://msdn.microsoft.com/workshop/browser/browser/Reference/IFaces/ITargetFrame/ITargetFrame.asp
//
// The interface is defined in very recent versions of Internet SDK part of
// the PlatformSDK, from Dec 2002 onwards.
//
// http://www.microsoft.com/msdownload/platformsdk/sdkupdate/ 
//
// Neither Visual C++ 6.0 or 7.0 ship with this interface.
//
#ifdef USE_HTIFACE
#include <htiface.h>
#endif
#ifndef __ITargetFrame_INTERFACE_DEFINED__
// No ITargetFrame so make a binary compatible one
MIDL_INTERFACE("d5f78c80-5252-11cf-90fa-00AA0042106e")
IMozTargetFrame : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetFrameName(
        /* [in] */ LPCWSTR pszFrameName) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFrameName(
        /* [out] */ LPWSTR *ppszFrameName) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetParentFrame(
        /* [out] */ IUnknown **ppunkParent) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindFrame( 
        /* [in] */ LPCWSTR pszTargetName,
        /* [in] */ IUnknown *ppunkContextFrame,
        /* [in] */ DWORD dwFlags,
        /* [out] */ IUnknown **ppunkTargetFrame) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFrameSrc( 
        /* [in] */ LPCWSTR pszFrameSrc) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFrameSrc( 
        /* [out] */ LPWSTR *ppszFrameSrc) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFramesContainer( 
        /* [out] */ IOleContainer **ppContainer) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFrameOptions( 
        /* [in] */ DWORD dwFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFrameOptions( 
        /* [out] */ DWORD *pdwFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFrameMargins( 
        /* [in] */ DWORD dwWidth,
        /* [in] */ DWORD dwHeight) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFrameMargins( 
        /* [out] */ DWORD *pdwWidth,
        /* [out] */ DWORD *pdwHeight) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoteNavigate( 
        /* [in] */ ULONG cLength,
        /* [size_is][in] */ ULONG *pulData) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnChildFrameActivate( 
        /* [in] */ IUnknown *pUnkChildFrame) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnChildFrameDeactivate( 
        /* [in] */ IUnknown *pUnkChildFrame) = 0;
};
#define __ITargetFrame_INTERFACE_DEFINED__
#define ITargetFrame IMozTargetFrame
#endif

#include "npapi.h"

#include "nsCOMPtr.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsString.h"
#include "nsNetUtil.h"

#include "nsIURI.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMLocation.h"
#include "nsIWebNavigation.h"
#include "nsILinkHandler.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIPrincipal.h"

#include "XPConnect.h"
#include "XPCBrowser.h"
#include "LegacyPlugin.h"

#include "IEHtmlElementCollection.h"
#include "IHTMLLocationImpl.h"

/*
 * This file contains partial implementations of various IE objects and
 * interfaces that many ActiveX controls expect to be able to obtain and
 * call from their control site. Typically controls will use these methods
 * in order to integrate themselves with the browser, e.g. a control
 * might want to initiate a load, or obtain the user agent.
 */

class IELocation :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IHTMLLocationImpl<IELocation>
{
public:
BEGIN_COM_MAP(IELocation)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLLocation)
END_COM_MAP()

    PluginInstanceData *mData;
    nsCOMPtr<nsIDOMLocation> mDOMLocation;

    IELocation() : mData(NULL)
    {
    }

    HRESULT Init(PluginInstanceData *pData)
    {
        NS_PRECONDITION(pData != nsnull, "null ptr");

        mData = pData;

        // Get the DOM window
        nsCOMPtr<nsIDOMWindow> domWindow;
        NPN_GetValue(mData->pPluginInstance, NPNVDOMWindow, 
                     NS_STATIC_CAST(nsIDOMWindow **, getter_AddRefs(domWindow)));
        if (!domWindow)
        {
            return E_FAIL;
        }
        nsCOMPtr<nsIDOMWindowInternal> windowInternal = do_QueryInterface(domWindow);
        if (windowInternal)
        {
            windowInternal->GetLocation(getter_AddRefs(mDOMLocation));
        }
        if (!mDOMLocation)
            return E_FAIL;

        return S_OK;
    }

    virtual nsresult GetDOMLocation(nsIDOMLocation **aLocation)
    {
        *aLocation = mDOMLocation;
        NS_IF_ADDREF(*aLocation);
        return NS_OK;
    }
};

// Note: corresponds to the window.navigator property in the IE DOM
class IENavigator :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IDispatchImpl<IOmNavigator, &__uuidof(IOmNavigator), &LIBID_MSHTML>
{
public:
BEGIN_COM_MAP(IENavigator)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IOmNavigator)
END_COM_MAP()

    PluginInstanceData *mData;
    CComBSTR mUserAgent;

    HRESULT Init(PluginInstanceData *pData)
    {
        mData = pData;
        USES_CONVERSION;
        const char *userAgent = NPN_UserAgent(mData->pPluginInstance);
        mUserAgent.Attach(::SysAllocString(A2CW(userAgent)));
        return S_OK;
    }

// IOmNavigator
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_appCodeName( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_appName( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_appVersion( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }

    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_userAgent( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        *p = mUserAgent.Copy();
        return S_OK;
    }

    virtual /* [id] */ HRESULT STDMETHODCALLTYPE javaEnabled( 
        /* [out][retval] */ VARIANT_BOOL __RPC_FAR *enabled)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE taintEnabled( 
        /* [out][retval] */ VARIANT_BOOL __RPC_FAR *enabled)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_mimeTypes( 
        /* [out][retval] */ IHTMLMimeTypesCollection __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_plugins( 
        /* [out][retval] */ IHTMLPluginsCollection __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_cookieEnabled( 
        /* [out][retval] */ VARIANT_BOOL __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_opsProfile( 
        /* [out][retval] */ IHTMLOpsProfile __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE toString( 
        /* [out][retval] */ BSTR __RPC_FAR *string)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_cpuClass( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_systemLanguage( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [hidden][id][propget] */ HRESULT STDMETHODCALLTYPE get_browserLanguage( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_userLanguage( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_platform( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_appMinorVersion( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [hidden][id][propget] */ HRESULT STDMETHODCALLTYPE get_connectionSpeed( 
        /* [out][retval] */ long __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_onLine( 
        /* [out][retval] */ VARIANT_BOOL __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_userProfile( 
        /* [out][retval] */ IHTMLOpsProfile __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
};


// Note: Corresponds to the window object in the IE DOM
class IEWindow :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IDispatchImpl<IHTMLWindow2, &__uuidof(IHTMLWindow2), &LIBID_MSHTML>
{
public:
    PluginInstanceData *mData;
    CComObject<IENavigator> *mNavigator;
    CComObject<IELocation>  *mLocation;

    IEWindow() : mNavigator(NULL), mLocation(NULL), mData(NULL)
    {
    }

    HRESULT Init(PluginInstanceData *pData)
    {
        mData = pData;

        CComObject<IENavigator>::CreateInstance(&mNavigator);
        if (!mNavigator)
        {
            return E_UNEXPECTED;
        }
        mNavigator->AddRef();
        mNavigator->Init(mData);

        CComObject<IELocation>::CreateInstance(&mLocation);
        if (!mLocation)
        {
            return E_UNEXPECTED;
        }
        mLocation->AddRef();
        mLocation->Init(mData);

        return S_OK;
    }

protected:
    virtual ~IEWindow()
    {
        if (mNavigator)
        {
            mNavigator->Release();
        }
        if (mLocation)
        {
            mLocation->Release();
        }
    }

public:
    
BEGIN_COM_MAP(IEWindow)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLWindow2)
    COM_INTERFACE_ENTRY(IHTMLFramesCollection2)
    COM_INTERFACE_ENTRY_BREAK(IHlinkFrame)
END_COM_MAP()

//IHTMLFramesCollection2
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE item( 
        /* [in] */ VARIANT __RPC_FAR *pvarIndex,
        /* [out][retval] */ VARIANT __RPC_FAR *pvarResult)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_length( 
        /* [out][retval] */ long __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }

// IHTMLWindow2
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_frames( 
        /* [out][retval] */ IHTMLFramesCollection2 __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_defaultStatus( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_defaultStatus( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_status( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_status( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE setTimeout( 
        /* [in] */ BSTR expression,
        /* [in] */ long msec,
        /* [in][optional] */ VARIANT __RPC_FAR *language,
        /* [out][retval] */ long __RPC_FAR *timerID)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE clearTimeout( 
        /* [in] */ long timerID)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE alert( 
        /* [in][defaultvalue] */ BSTR message)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE confirm( 
        /* [in][defaultvalue] */ BSTR message,
        /* [out][retval] */ VARIANT_BOOL __RPC_FAR *confirmed)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE prompt( 
        /* [in][defaultvalue] */ BSTR message,
        /* [in][defaultvalue] */ BSTR defstr,
        /* [out][retval] */ VARIANT __RPC_FAR *textdata)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_Image( 
        /* [out][retval] */ IHTMLImageElementFactory __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_location( 
        /* [out][retval] */ IHTMLLocation __RPC_FAR *__RPC_FAR *p)
    {
        if (mLocation)
            return mLocation->QueryInterface(__uuidof(IHTMLLocation), (void **) p);
        return E_FAIL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_history( 
        /* [out][retval] */ IOmHistory __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE close( void)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_opener( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_opener( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_navigator( 
        /* [out][retval] */ IOmNavigator __RPC_FAR *__RPC_FAR *p)
    {
        if (mNavigator)
            return mNavigator->QueryInterface(__uuidof(IOmNavigator), (void **) p);
        return E_FAIL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_name( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_name( 
        /* [out][retval] */ BSTR __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_parent( 
        /* [out][retval] */ IHTMLWindow2 __RPC_FAR *__RPC_FAR *p)
    {
        *p = NULL;
        return S_OK;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE open( 
        /* [in][defaultvalue] */ BSTR url,
        /* [in][defaultvalue] */ BSTR name,
        /* [in][defaultvalue] */ BSTR features,
        /* [in][defaultvalue] */ VARIANT_BOOL replace,
        /* [out][retval] */ IHTMLWindow2 __RPC_FAR *__RPC_FAR *pomWindowResult)
    {
        *pomWindowResult = NULL;
        return E_FAIL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_self( 
        /* [out][retval] */ IHTMLWindow2 __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_top( 
        /* [out][retval] */ IHTMLWindow2 __RPC_FAR *__RPC_FAR *p)
    {
        *p = NULL;
        return S_OK;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_window( 
        /* [out][retval] */ IHTMLWindow2 __RPC_FAR *__RPC_FAR *p)
    {
        *p = NULL;
        return S_OK;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE navigate( 
        /* [in] */ BSTR url)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onfocus( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onfocus( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onblur( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }

    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onblur( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onload( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onload( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onbeforeunload( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onbeforeunload( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onunload( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onunload( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onhelp( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onhelp( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onerror( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onerror( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onresize( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onresize( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onscroll( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onscroll( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [source][id][propget] */ HRESULT STDMETHODCALLTYPE get_document( 
        /* [out][retval] */ IHTMLDocument2 __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_event( 
        /* [out][retval] */ IHTMLEventObj __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [restricted][hidden][id][propget] */ HRESULT STDMETHODCALLTYPE get__newEnum( 
        /* [out][retval] */ IUnknown __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE showModalDialog( 
        /* [in] */ BSTR dialog,
        /* [in][optional] */ VARIANT __RPC_FAR *varArgIn,
        /* [in][optional] */ VARIANT __RPC_FAR *varOptions,
        /* [out][retval] */ VARIANT __RPC_FAR *varArgOut)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE showHelp( 
        /* [in] */ BSTR helpURL,
        /* [in][optional] */ VARIANT helpArg,
        /* [in][defaultvalue] */ BSTR features)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_screen( 
        /* [out][retval] */ IHTMLScreen __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_Option( 
        /* [out][retval] */ IHTMLOptionElementFactory __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE focus( void)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_closed( 
        /* [out][retval] */ VARIANT_BOOL __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE blur( void)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE scroll( 
        /* [in] */ long x,
        /* [in] */ long y)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_clientInformation( 
        /* [out][retval] */ IOmNavigator __RPC_FAR *__RPC_FAR *p)
    {
        return get_navigator(p);
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE setInterval( 
        /* [in] */ BSTR expression,
        /* [in] */ long msec,
        /* [in][optional] */ VARIANT __RPC_FAR *language,
        /* [out][retval] */ long __RPC_FAR *timerID)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE clearInterval( 
        /* [in] */ long timerID)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_offscreenBuffering( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_offscreenBuffering( 
        /* [out][retval] */ VARIANT __RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE execScript( 
        /* [in] */ BSTR code,
        /* [in][defaultvalue] */ BSTR language,
        /* [out][retval] */ VARIANT __RPC_FAR *pvarRet)
    {
        nsresult rv;

        nsCOMPtr<nsIDOMWindow> domWindow;
        NPN_GetValue(mData->pPluginInstance, NPNVDOMWindow, 
                     NS_STATIC_CAST(nsIDOMWindow **, getter_AddRefs(domWindow)));
        if (!domWindow)
        {
            return E_UNEXPECTED;
        }

        // Now get the DOM Document.  Accessing the document will create one
        // if necessary.  So, basically, this call ensures that a document gets
        // created -- if necessary.
        nsCOMPtr<nsIDOMDocument> domDocument;
        rv = domWindow->GetDocument(getter_AddRefs(domDocument));
        NS_ASSERTION(domDocument, "No DOMDocument!");
        if (NS_FAILED(rv)) {
            return E_UNEXPECTED;
        }

        nsCOMPtr<nsIScriptGlobalObject> globalObject(do_QueryInterface(domWindow));
        if (!globalObject)
            return E_UNEXPECTED;

        nsCOMPtr<nsIScriptContext> scriptContext = globalObject->GetContext();
        if (!scriptContext)
            return E_UNEXPECTED;

        nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDocument));
        if (!doc)
            return E_UNEXPECTED;

        nsIPrincipal *principal = doc->GetPrincipal();
        if (!principal)
            return E_UNEXPECTED;

        // Execute the script.
        //
        // Note: The script context takes care of the JS stack and of ensuring
        //       nothing is executed when JS is disabled.
        //
        nsAutoString scriptString(code);
        NS_NAMED_LITERAL_CSTRING(url, "javascript:axplugin");
        nsAutoString result;
        rv = scriptContext->EvaluateString(scriptString,
                                           nsnull,      // obj
                                           principal,
                                           url.get(),   // url
                                           1,           // line no
                                           nsnull,
                                           result,
                                           nsnull);

        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;

        return S_OK;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE toString( 
        /* [out][retval] */ BSTR __RPC_FAR *String)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE scrollBy( 
        /* [in] */ long x,
        /* [in] */ long y)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE scrollTo( 
        /* [in] */ long x,
        /* [in] */ long y)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE moveTo( 
        /* [in] */ long x,
        /* [in] */ long y)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE moveBy( 
        /* [in] */ long x,
        /* [in] */ long y)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE resizeTo( 
        /* [in] */ long x,
        /* [in] */ long y)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE resizeBy( 
        /* [in] */ long x,
        /* [in] */ long y)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_external( 
        /* [out][retval] */ IDispatch __RPC_FAR *__RPC_FAR *p)
    {
        return E_NOTIMPL;
    }
        
};


// Note: Corresponds to the document object in the IE DOM
class IEDocument :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IDispatchImpl<IHTMLDocument2, &__uuidof(IHTMLDocument2), &LIBID_MSHTML>,
    public IServiceProvider,
    public IOleContainer,
    public IBindHost,
    public IHlinkFrame,
    public ITargetFrame
{
public:
    PluginInstanceData *mData;

    nsCOMPtr<nsIDOMWindow> mDOMWindow;
    nsCOMPtr<nsIDOMDocument> mDOMDocument;
    CComObject<IEWindow> *mWindow;
    CComObject<IEBrowser> *mBrowser;
    CComBSTR mURL;
    BSTR mUseTarget;

    IEDocument() :
        mWindow(NULL),
        mBrowser(NULL),
        mData(NULL),
        mUseTarget(NULL)
    {
        MozAxPlugin::AddRef();
    }

    HRESULT Init(PluginInstanceData *pData)
    {
        mData = pData;
        nsCOMPtr<nsIDOMElement> element;

        // Get the DOM document
        NPN_GetValue(mData->pPluginInstance, NPNVDOMElement, 
                     NS_STATIC_CAST(nsIDOMElement **, getter_AddRefs(element)));
        if (element)
        {
            element->GetOwnerDocument(getter_AddRefs(mDOMDocument));
        }

        // Get the DOM window
        NPN_GetValue(mData->pPluginInstance, NPNVDOMWindow, 
                     NS_STATIC_CAST(nsIDOMWindow **, getter_AddRefs(mDOMWindow)));
        if (mDOMWindow)
        {
            nsCOMPtr<nsIDOMWindowInternal> windowInternal = do_QueryInterface(mDOMWindow);
            if (windowInternal)
            {
                nsCOMPtr<nsIDOMLocation> location;
                nsAutoString href;
                windowInternal->GetLocation(getter_AddRefs(location));
                if (location &&
                    NS_SUCCEEDED(location->GetHref(href)))
                {
                    const PRUnichar *s = href.get();
                    mURL.Attach(::SysAllocString(s));
                }
            }
        }

        CComObject<IEWindow>::CreateInstance(&mWindow);
        ATLASSERT(mWindow);
        if (!mWindow)
        {
            return E_OUTOFMEMORY;
        }
        mWindow->AddRef();
        mWindow->Init(mData);

        CComObject<IEBrowser>::CreateInstance(&mBrowser);
        ATLASSERT(mBrowser);
        if (!mBrowser)
        {
            return E_OUTOFMEMORY;
        }
        mBrowser->AddRef();
        mBrowser->Init(mData);
 
        return S_OK;
    }

    virtual ~IEDocument()
    {
        if (mUseTarget)
        {
            SysFreeString(mUseTarget);
        }
        if (mBrowser)
        {
            mBrowser->Release();
        }
        if (mWindow)
        {
            mWindow->Release();
        }
        MozAxPlugin::Release();
    }

BEGIN_COM_MAP(IEDocument)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLDocument)
    COM_INTERFACE_ENTRY(IHTMLDocument2)
    COM_INTERFACE_ENTRY(IParseDisplayName)
    COM_INTERFACE_ENTRY(IOleContainer)
    COM_INTERFACE_ENTRY(IBindHost)
    COM_INTERFACE_ENTRY_BREAK(IHlinkTarget)
    COM_INTERFACE_ENTRY(IHlinkFrame)
    COM_INTERFACE_ENTRY(ITargetFrame)
END_COM_MAP()

// IServiceProvider
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE QueryService( 
        /* [in] */ REFGUID guidService,
        /* [in] */ REFIID riid,
        /* [out] */ void **ppvObject)
    {
#ifdef DEBUG
        ATLTRACE(_T("IEDocument::QueryService\n"));
        if (IsEqualIID(riid, __uuidof(IWebBrowser)) ||
            IsEqualIID(riid, __uuidof(IWebBrowser2)) ||
            IsEqualIID(riid, __uuidof(IWebBrowserApp)))
        {
            ATLTRACE(_T("  IWebBrowserApp\n"));
            if (mBrowser)
            {
                return mBrowser->QueryInterface(riid, ppvObject);
            }
        }
        else if (IsEqualIID(riid, __uuidof(IHTMLWindow2)))
        {
            ATLTRACE(_T("  IHTMLWindow2\n"));
            if (mWindow)
            {
                return mWindow->QueryInterface(riid, ppvObject);
            }
        }
        else if (IsEqualIID(riid, __uuidof(IHTMLDocument2)))
        {
            ATLTRACE(_T("  IHTMLDocument2\n"));
        }
        else if (IsEqualIID(riid, __uuidof(IBindHost)))
        {
            ATLTRACE(_T("  IBindHost\n"));
        }
        else
        {
            USES_CONVERSION;
            LPOLESTR szClsid = NULL;
            StringFromIID(riid, &szClsid);
            ATLTRACE(_T("  Unknown interface %s\n"), OLE2T(szClsid));
            CoTaskMemFree(szClsid);
        }
#endif
        return QueryInterface(riid, ppvObject);
    }

// IHTMLDocument
    virtual /* [nonbrowsable][hidden][id][propget] */ HRESULT STDMETHODCALLTYPE get_Script( 
        /* [out][retval] */ IDispatch **p)
    {
        *p = NULL;
        return S_OK;
    }

// IHTMLDocument2
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_all( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        // Validate parameters
        if (p == NULL)
        {
            return E_INVALIDARG;
        }

        *p = NULL;

        // Create a collection object
        CIEHtmlElementCollectionInstance *pCollection = NULL;
        CIEHtmlElementCollectionInstance::CreateInstance(&pCollection);
        if (pCollection == NULL)
        {
            return E_OUTOFMEMORY;
        }

        // Initialise and populate the collection
        nsCOMPtr<nsIDOMNode> docNode = do_QueryInterface(mDOMDocument);
        pCollection->PopulateFromDOMNode(docNode, PR_TRUE);
        pCollection->QueryInterface(IID_IHTMLElementCollection, (void **) p);

        return *p ? S_OK : E_UNEXPECTED;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_body( 
        /* [out][retval] */ IHTMLElement **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_activeElement( 
        /* [out][retval] */ IHTMLElement **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_images( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_applets( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_links( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_forms( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_anchors( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_title( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_title( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_scripts( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [hidden][id][propput] */ HRESULT STDMETHODCALLTYPE put_designMode( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [hidden][id][propget] */ HRESULT STDMETHODCALLTYPE get_designMode( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_selection( 
        /* [out][retval] */ IHTMLSelectionObject **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][id][propget] */ HRESULT STDMETHODCALLTYPE get_readyState( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_frames( 
        /* [out][retval] */ IHTMLFramesCollection2 **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_embeds( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_plugins( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_alinkColor( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_alinkColor( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_bgColor( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_bgColor( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_fgColor( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_fgColor( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_linkColor( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_linkColor( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_vlinkColor( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_vlinkColor( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_referrer( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_location( 
        /* [out][retval] */ IHTMLLocation **p)
    {
        if (mWindow)
            return mWindow->get_location(p);
        return E_FAIL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_lastModified( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_URL( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_URL( 
        /* [out][retval] */ BSTR *p)
    {
        *p = mURL.Copy();
        return S_OK;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_domain( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_domain( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_cookie( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_cookie( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [hidden][bindable][id][propput] */ HRESULT STDMETHODCALLTYPE put_expando( 
        /* [in] */ VARIANT_BOOL v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [hidden][bindable][id][propget] */ HRESULT STDMETHODCALLTYPE get_expando( 
        /* [out][retval] */ VARIANT_BOOL *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [hidden][id][propput] */ HRESULT STDMETHODCALLTYPE put_charset( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [hidden][id][propget] */ HRESULT STDMETHODCALLTYPE get_charset( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_defaultCharset( 
        /* [in] */ BSTR v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_defaultCharset( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_mimeType( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_fileSize( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_fileCreatedDate( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_fileModifiedDate( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_fileUpdatedDate( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_security( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_protocol( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_nameProp( 
        /* [out][retval] */ BSTR *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][vararg] */ HRESULT STDMETHODCALLTYPE write( 
        /* [in] */ SAFEARRAY * psarray)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][vararg] */ HRESULT STDMETHODCALLTYPE writeln( 
        /* [in] */ SAFEARRAY * psarray)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE open( 
        /* [in][defaultvalue] */ BSTR url,
        /* [in][optional] */ VARIANT name,
        /* [in][optional] */ VARIANT features,
        /* [in][optional] */ VARIANT replace,
        /* [out][retval] */ IDispatch **pomWindowResult)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE close( void)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE clear( void)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE queryCommandSupported( 
        /* [in] */ BSTR cmdID,
        /* [out][retval] */ VARIANT_BOOL *pfRet)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE queryCommandEnabled( 
        /* [in] */ BSTR cmdID,
        /* [out][retval] */ VARIANT_BOOL *pfRet)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE queryCommandState( 
        /* [in] */ BSTR cmdID,
        /* [out][retval] */ VARIANT_BOOL *pfRet)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE queryCommandIndeterm( 
        /* [in] */ BSTR cmdID,
        /* [out][retval] */ VARIANT_BOOL *pfRet)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE queryCommandText( 
        /* [in] */ BSTR cmdID,
        /* [out][retval] */ BSTR *pcmdText)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE queryCommandValue( 
        /* [in] */ BSTR cmdID,
        /* [out][retval] */ VARIANT *pcmdValue)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE execCommand( 
        /* [in] */ BSTR cmdID,
        /* [in][defaultvalue] */ VARIANT_BOOL showUI,
        /* [in][optional] */ VARIANT value,
        /* [out][retval] */ VARIANT_BOOL *pfRet)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE execCommandShowHelp( 
        /* [in] */ BSTR cmdID,
        /* [out][retval] */ VARIANT_BOOL *pfRet)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE createElement( 
        /* [in] */ BSTR eTag,
        /* [out][retval] */ IHTMLElement **newElem)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onhelp( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onhelp( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onclick( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onclick( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_ondblclick( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_ondblclick( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onkeyup( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onkeyup( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onkeydown( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onkeydown( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onkeypress( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onkeypress( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onmouseup( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onmouseup( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onmousedown( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onmousedown( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onmousemove( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onmousemove( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onmouseout( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onmouseout( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onmouseover( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onmouseover( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onreadystatechange( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onreadystatechange( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onafterupdate( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onafterupdate( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onrowexit( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onrowexit( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onrowenter( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onrowenter( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_ondragstart( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_ondragstart( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onselectstart( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onselectstart( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE elementFromPoint( 
        /* [in] */ long x,
        /* [in] */ long y,
        /* [out][retval] */ IHTMLElement **elementHit)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_parentWindow( 
        /* [out][retval] */ IHTMLWindow2 **p)
    {
        return mWindow->QueryInterface(_uuidof(IHTMLWindow2), (void **) p);
    }
    
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_styleSheets( 
        /* [out][retval] */ IHTMLStyleSheetsCollection **p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onbeforeupdate( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onbeforeupdate( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propput] */ HRESULT STDMETHODCALLTYPE put_onerrorupdate( 
        /* [in] */ VARIANT v)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [bindable][displaybind][id][propget] */ HRESULT STDMETHODCALLTYPE get_onerrorupdate( 
        /* [out][retval] */ VARIANT *p)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE toString( 
        /* [out][retval] */ BSTR *String)
    {
        return E_NOTIMPL;
    }
    
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE createStyleSheet( 
        /* [in][defaultvalue] */ BSTR bstrHref,
        /* [in][defaultvalue] */ long lIndex,
        /* [out][retval] */ IHTMLStyleSheet **ppnewStyleSheet)
    {
        return E_NOTIMPL;
    }

// IParseDisplayName
    virtual HRESULT STDMETHODCALLTYPE ParseDisplayName( 
        /* [unique][in] */ IBindCtx *pbc,
        /* [in] */ LPOLESTR pszDisplayName,
        /* [out] */ ULONG *pchEaten,
        /* [out] */ IMoniker **ppmkOut)
    {
        return E_NOTIMPL;
    }

// IOleContainer
    virtual HRESULT STDMETHODCALLTYPE EnumObjects( 
        /* [in] */ DWORD grfFlags,
        /* [out] */ IEnumUnknown **ppenum)
    {
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE LockContainer( 
        /* [in] */ BOOL fLock)
    {
        return E_NOTIMPL;
    }


// IHlinkFrame
    virtual HRESULT STDMETHODCALLTYPE SetBrowseContext( 
        /* [unique][in] */ IHlinkBrowseContext *pihlbc)
    {
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE GetBrowseContext( 
        /* [out] */ IHlinkBrowseContext **ppihlbc)
    {
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE Navigate( 
        /* [in] */ DWORD grfHLNF,
        /* [unique][in] */ LPBC pbc,
        /* [unique][in] */ IBindStatusCallback *pibsc,
        /* [unique][in] */ IHlink *pihlNavigate)
    {
        if (!pihlNavigate) return E_INVALIDARG;
        // TODO check grfHLNF for type of link
        LPWSTR szTarget = NULL;
        LPWSTR szLocation = NULL;
        LPWSTR szTargetFrame = NULL;
        HRESULT hr;
        hr = pihlNavigate->GetStringReference(HLINKGETREF_DEFAULT, &szTarget, &szLocation);
        hr = pihlNavigate->GetTargetFrameName(&szTargetFrame);
        if (szTarget && szTarget[0] != WCHAR('\0'))
        {
            nsCAutoString spec = NS_ConvertUCS2toUTF8(szTarget);
            nsCOMPtr<nsIURI> uri;
            nsresult rv = NS_NewURI(getter_AddRefs(uri), spec);
            if (NS_SUCCEEDED(rv) && uri)
            {
                nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(mDOMWindow);
                if (webNav)
                {
                    nsCOMPtr<nsILinkHandler> lh = do_QueryInterface(webNav);
                    if (lh)
                    {
                        lh->OnLinkClick(nsnull, eLinkVerb_Replace,
                            uri, szTargetFrame ? szTargetFrame : mUseTarget);
                    }
                }
                hr = S_OK;
            }
            else
            {
                hr = E_FAIL;
            }
        }
        else
        {
            hr = E_FAIL;
        }
        if (szTarget)
            CoTaskMemFree(szTarget);
        if (szLocation)
            CoTaskMemFree(szLocation);
        if (szTargetFrame)
            CoTaskMemFree(szTargetFrame);
        if (mUseTarget)
        {
            SysFreeString(mUseTarget);
            mUseTarget = NULL;
        }
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE OnNavigate( 
        /* [in] */ DWORD grfHLNF,
        /* [unique][in] */ IMoniker *pimkTarget,
        /* [unique][in] */ LPCWSTR pwzLocation,
        /* [unique][in] */ LPCWSTR pwzFriendlyName,
        /* [in] */ DWORD dwreserved)
    {
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE UpdateHlink( 
        /* [in] */ ULONG uHLID,
        /* [unique][in] */ IMoniker *pimkTarget,
        /* [unique][in] */ LPCWSTR pwzLocation,
        /* [unique][in] */ LPCWSTR pwzFriendlyName)
    {
        return E_NOTIMPL;
    }

// IBindHost
    virtual HRESULT STDMETHODCALLTYPE CreateMoniker( 
        /* [in] */ LPOLESTR szName,
        /* [in] */ IBindCtx *pBC,
        /* [out] */ IMoniker **ppmk,
        /* [in] */ DWORD dwReserved)
    {
        if (!szName || !ppmk) return E_POINTER;
        if (!*szName) return E_INVALIDARG;

        *ppmk = NULL;

        // Get the BASE url
        CComPtr<IMoniker> baseURLMoniker;
        nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDOMDocument));
        if (doc)
        {
            nsIURI *baseURI = doc->GetBaseURI();
            nsCAutoString spec;
            if (baseURI &&
                NS_SUCCEEDED(baseURI->GetSpec(spec)))
            {
                USES_CONVERSION;
                if (FAILED(CreateURLMoniker(NULL, T2CW(spec.get()), &baseURLMoniker)))
                    return E_UNEXPECTED;
            }
        }

        // Make the moniker
        HRESULT hr = CreateURLMoniker(baseURLMoniker, szName, ppmk);
        if (SUCCEEDED(hr) && !*ppmk)
            hr = E_FAIL;
        return hr;
    }
    
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE MonikerBindToStorage( 
        /* [in] */ IMoniker *pMk,
        /* [in] */ IBindCtx *pBC,
        /* [in] */ IBindStatusCallback *pBSC,
        /* [in] */ REFIID riid,
        /* [out] */ void **ppvObj)
    {
        if (!pMk || !ppvObj) return E_POINTER;

        *ppvObj = NULL;
        HRESULT hr = S_OK;
        CComPtr<IBindCtx> spBindCtx;
        if (pBC)
        {
            spBindCtx = pBC;
            if (pBSC)
            {
                hr = RegisterBindStatusCallback(spBindCtx, pBSC, NULL, 0);
                if (FAILED(hr))
                    return hr;
            }
        }
        else
        {
            if (pBSC)
                hr = CreateAsyncBindCtx(0, pBSC, NULL, &spBindCtx);
            else
                hr = CreateBindCtx(0, &spBindCtx);
            if (SUCCEEDED(hr) && !spBindCtx)
                hr = E_FAIL;
            if (FAILED(hr))
                return hr;
        }
        return pMk->BindToStorage(spBindCtx, NULL, riid, ppvObj);
    }
    
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE MonikerBindToObject( 
        /* [in] */ IMoniker *pMk,
        /* [in] */ IBindCtx *pBC,
        /* [in] */ IBindStatusCallback *pBSC,
        /* [in] */ REFIID riid,
        /* [out] */ void **ppvObj)
    {
        return E_NOTIMPL;
    }

// ITargetFrame
    virtual HRESULT STDMETHODCALLTYPE SetFrameName(
        /* [in] */ LPCWSTR pszFrameName)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::SetFrameName is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetFrameName(
        /* [out] */ LPWSTR *ppszFrameName)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::GetFrameName is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetParentFrame(
        /* [out] */ IUnknown **ppunkParent)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::GetParentFrame is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE FindFrame( 
        /* [in] */ LPCWSTR pszTargetName,
        /* [in] */ IUnknown *ppunkContextFrame,
        /* [in] */ DWORD dwFlags,
        /* [out] */ IUnknown **ppunkTargetFrame)
    {
        if (dwFlags & 0x1) // TODO test if the named frame exists and presumably return NULL if it doesn't
        {
        }

        if (mUseTarget)
        {
            SysFreeString(mUseTarget);
            mUseTarget = NULL;
        }
        if (pszTargetName)
        {
            mUseTarget = SysAllocString(pszTargetName);
        }

        QueryInterface(__uuidof(IUnknown), (void **) ppunkTargetFrame);
        return S_OK;;
    }

    virtual HRESULT STDMETHODCALLTYPE SetFrameSrc( 
        /* [in] */ LPCWSTR pszFrameSrc)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::SetFrameSrc is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetFrameSrc( 
        /* [out] */ LPWSTR *ppszFrameSrc)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::GetFrameSrc is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetFramesContainer( 
        /* [out] */ IOleContainer **ppContainer)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::GetFramesContainer is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE SetFrameOptions( 
        /* [in] */ DWORD dwFlags)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::SetFrameOptions is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetFrameOptions( 
        /* [out] */ DWORD *pdwFlags)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::GetFrameOptions is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE SetFrameMargins( 
        /* [in] */ DWORD dwWidth,
        /* [in] */ DWORD dwHeight)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::SetFrameMargins is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetFrameMargins( 
        /* [out] */ DWORD *pdwWidth,
        /* [out] */ DWORD *pdwHeight)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::GetFrameMargins is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE RemoteNavigate( 
        /* [in] */ ULONG cLength,
        /* [size_is][in] */ ULONG *pulData)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::RemoteNavigate is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE OnChildFrameActivate( 
        /* [in] */ IUnknown *pUnkChildFrame)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::OnChildFrameActivate is not implemented");
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE OnChildFrameDeactivate( 
        /* [in] */ IUnknown *pUnkChildFrame)
    {
        NS_ASSERTION(FALSE, "ITargetFrame::OnChildFrameDeactivate is not implemented");
        return E_NOTIMPL;
    }
};

HRESULT MozAxPlugin::GetServiceProvider(PluginInstanceData *pData, IServiceProvider **pSP)
{
    // Note this should be called on the main NPAPI thread
    CComObject<IEDocument> *pDoc = NULL;
    CComObject<IEDocument>::CreateInstance(&pDoc);
    ATLASSERT(pDoc);
    if (!pDoc)
    {
        return E_OUTOFMEMORY;
    }
    pDoc->Init(pData);
    // QI does the AddRef
    return pDoc->QueryInterface(_uuidof(IServiceProvider), (void **) pSP);
}
