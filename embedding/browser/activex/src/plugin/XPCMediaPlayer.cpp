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

#include "stdafx.h"

#include "XPCMediaPlayer.h"

#include "nsString.h"

//#############################################################################
// IMPORTANT!!!!!
// I am using Windows Media Player to test how the plugin copes with arbitrary
// scriptability interfaces and to exercise the IHTMLDocument2 and IWebBrowser
// interfaces that many controls expect.
// THIS CODE ISN'T GOING STAY SO DON'T RELY ON IT!!!
//#############################################################################


const IID IID_IWMPCore = 
    { 0xD84CCA99, 0xCCE2, 0x11d2, { 0x9E, 0xCC, 0x00, 0x00, 0xF8, 0x08, 0x59, 0x81 } };

/* Header file */
class nsWMPControls :
    public nsIWMPControls,
    public nsIClassInfoImpl<nsWMPControls>
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWMPCONTROLS

    nsWMPScriptablePeer *mOwner;
    HRESULT GetIWMPControls(IWMPControls **pc);

    nsWMPControls(nsWMPScriptablePeer *pOwner);
    virtual ~nsWMPControls();
    /* additional members */
};

/* Header file */
class nsWMPSettings :
    public nsIWMPSettings,
    public nsIClassInfoImpl<nsWMPControls>
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWMPSETTINGS

    nsWMPScriptablePeer *mOwner;
    HRESULT GetIWMPSettings(IWMPSettings **ps);

    nsWMPSettings(nsWMPScriptablePeer *pOwner);
    virtual ~nsWMPSettings();
    /* additional members */
};

nsWMPScriptablePeer::nsWMPScriptablePeer()
{
    NS_INIT_ISUPPORTS();
    mControls = new nsWMPControls(this);
    mControls->AddRef();
    mSettings = new nsWMPSettings(this);
    mSettings->AddRef();
}

nsWMPScriptablePeer::~nsWMPScriptablePeer()
{
    mSettings->Release();
    mControls->Release();
}

NS_IMPL_ADDREF_INHERITED(nsWMPScriptablePeer, nsScriptablePeer)
NS_IMPL_RELEASE_INHERITED(nsWMPScriptablePeer, nsScriptablePeer)

NS_INTERFACE_MAP_BEGIN(nsWMPScriptablePeer)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIWMPCore, nsIWMPPlayer2)
    NS_INTERFACE_MAP_ENTRY(nsIWMPPlayer2)
NS_INTERFACE_MAP_END_INHERITING(nsScriptablePeer)

HRESULT
nsWMPScriptablePeer::GetIWMPCore(IWMPCore **pwmpc)
{
    *pwmpc = NULL;
    CComPtr<IDispatch> disp;
    HRESULT hr = GetIDispatch(&disp);
    if (FAILED(hr)) return hr;
    return disp->QueryInterface(IID_IWMPCore, (void **) pwmpc);
}

///////////////////////////////////////////////////////////////////////////////
// nsIWMPCore

/* attribute AString URL; */
NS_IMETHODIMP nsWMPScriptablePeer::GetURL(nsAString & aURL)
{
    CComPtr<IWMPCore> wmpc;
    HRESULT hr = GetIWMPCore(&wmpc);
    if (FAILED(hr)) return hr;
    CComBSTR bstrURL;
    hr = wmpc->get_URL(&bstrURL);
    if (SUCCEEDED(hr))
    {
        aURL.Assign(bstrURL.m_str);
    }
    return HR2NS(hr);
}
NS_IMETHODIMP nsWMPScriptablePeer::SetURL(const nsAString & aURL)
{
    CComPtr<IWMPCore> wmpc;
    HRESULT hr = GetIWMPCore(&wmpc);
    if (FAILED(hr)) return hr;

    nsAutoString url(aURL);
    CComBSTR bstrURL(url.get());
    hr = wmpc->put_URL(bstrURL);

    // DEBUG
    nsAutoString urlTest;
    GetURL(urlTest);
    nsAutoString urlBaseTest;
    mSettings->GetBaseURL(urlBaseTest);

    return HR2NS(hr);
}

/* readonly attribute nsIWMPControls controls; */
NS_IMETHODIMP nsWMPScriptablePeer::GetControls(nsIWMPControls * *aControls)
{
    return mControls->QueryInterface(NS_GET_IID(nsIWMPControls), (void **) aControls);
}

/* readonly attribute nsIWMPSettings settings; */
NS_IMETHODIMP nsWMPScriptablePeer::GetSettings(nsIWMPSettings * *aSettings)
{
    return mSettings->QueryInterface(NS_GET_IID(nsIWMPSettings), (void **) aSettings);
}

///////////////////////////////////////////////////////////////////////////////
// nsIWMPPlayer2

/* attribute boolean stretchToFit; */
NS_IMETHODIMP nsWMPScriptablePeer::GetStretchToFit(PRBool *aStretchToFit)
{
    CComPtr<IWMPCore> wmpc;
    HRESULT hr = GetIWMPCore(&wmpc);
    if (FAILED(hr)) return hr;
    CComQIPtr<IWMPPlayer2> wmpp2 = wmpc;
    VARIANT_BOOL bStretchToFit = VARIANT_FALSE;
    if (wmpp2)
        wmpp2->get_stretchToFit(&bStretchToFit);
    *aStretchToFit = (bStretchToFit == VARIANT_TRUE) ? PR_TRUE : PR_FALSE;
    return NS_OK;
}
NS_IMETHODIMP nsWMPScriptablePeer::SetStretchToFit(PRBool aStretchToFit)
{
    CComPtr<IWMPCore> wmpc;
    HRESULT hr = GetIWMPCore(&wmpc);
    if (FAILED(hr)) return hr;
    CComQIPtr<IWMPPlayer2> wmpp2 = wmpc;
    if (wmpp2)
        wmpp2->put_stretchToFit(aStretchToFit ? VARIANT_TRUE : VARIANT_FALSE);
    return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Some sanity saving macros to remove some drudgery of this work.

#define IMPL_COMMON(iface) \
    CComPtr<iface> i; \
    HRESULT hr = Get ## iface (&i); \
    if (FAILED(hr)) return mOwner->HR2NS(hr);

#define IMPL_GETPROP_BOOL_NAMED(iface, propname, item, retval) \
    IMPL_COMMON(iface) \
    VARIANT_BOOL bValue = VARIANT_FALSE; \
    nsAutoString strItem(item); \
    CComBSTR bstrItem(strItem.get()); \
    hr = i->get_ ## propname(bstrItem, &bValue); \
    if (FAILED(hr)) return mOwner->HR2NS(hr); \
    *(retval) = (bValue == VARIANT_TRUE) ? PR_TRUE : PR_FALSE; \
    return NS_OK;

#define IMPL_SETPROP_BOOL_NAMED(iface, propname, item, retval) \
    IMPL_COMMON(iface) \
    VARIANT_BOOL bValue = VARIANT_FALSE; \
    nsAutoString strItem(item); \
    CComBSTR bstrItem(strItem.get()); \
    hr = i->put_ ## propname(bstrItem, bValue); \
    if (FAILED(hr)) return mOwner->HR2NS(hr); \
    return NS_OK;

#define IMPL_GETPROP_BOOL(iface, propname, retval) \
    IMPL_COMMON(iface) \
    VARIANT_BOOL bValue = VARIANT_FALSE; \
    hr = i->get_ ## propname(&bValue); \
    if (FAILED(hr)) return mOwner->HR2NS(hr); \
    *(retval) = (bValue == VARIANT_TRUE) ? PR_TRUE : PR_FALSE; \
    return NS_OK;

#define IMPL_SETPROP_BOOL(iface, propname, value) \
    IMPL_COMMON(iface) \
    hr = i->put_ ## propname((value) ? VARIANT_TRUE : VARIANT_FALSE); \
    if (FAILED(hr)) return mOwner->HR2NS(hr); \
    return NS_OK;

#define IMPL_GETPROP_NUM(iface, propname, nstype, oletype, retval) \
    IMPL_COMMON(iface) \
    oletype nValue = 0; \
    hr = i->get_ ## propname(&nValue); \
    if (FAILED(hr)) return mOwner->HR2NS(hr); \
    *(retval) = (nstype) nValue; \
    return NS_OK;

#define IMPL_SETPROP_NUM(iface, propname, nstype, oletype, value) \
    IMPL_COMMON(iface) \
    hr = i->put_ ## propname((oletype)(value)); \
    if (FAILED(hr)) return mOwner->HR2NS(hr); \
    return NS_OK;

#define IMPL_GETPROP_DOUBLE(iface, propname, retval) \
    IMPL_GETPROP_NUM(iface, propname, double, double, retval)

#define IMPL_SETPROP_DOUBLE(iface, propname, retval) \
    IMPL_SETPROP_NUM(iface, propname, double, double, retval)

#define IMPL_GETPROP_LONG(iface, propname, retval) \
    IMPL_GETPROP_NUM(iface, propname, PRInt32, long, retval)

#define IMPL_SETPROP_LONG(iface, propname, retval) \
    IMPL_SETPROP_NUM(iface, propname, PRInt32, long, retval)

#define IMPL_GETPROP_BSTR(iface, propname, retval) \
    IMPL_COMMON(iface) \
    CComBSTR bstrVal; \
    hr = i->get_ ## propname(&bstrVal); \
    if (FAILED(hr)) return mOwner->HR2NS(hr); \
    retval.Assign(bstrVal.m_str); \
    return NS_OK;

#define IMPL_SETPROP_BSTR(iface, propname, value) \
    IMPL_COMMON(iface) \
    nsAutoString val(value); \
    CComBSTR bstrVal(val.get()); \
    hr = i->put_ ## propname (bstrVal); \
    if (FAILED(hr)) return mOwner->HR2NS(hr); \
    return NS_OK;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


/* Implementation file */
NS_IMPL_ISUPPORTS2(nsWMPControls, nsIWMPControls, nsIClassInfo)

nsWMPControls::nsWMPControls(nsWMPScriptablePeer *pOwner) :
    mOwner(pOwner)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsWMPControls::~nsWMPControls()
{
  /* destructor code */
}

HRESULT
nsWMPControls::GetIWMPControls(IWMPControls **pc)
{
    *pc = NULL;
    CComPtr<IWMPCore> wmpc;
    HRESULT hr = mOwner->GetIWMPCore(&wmpc);
    if (FAILED(hr)) return hr;
    return wmpc->get_controls(pc);
}

///////////////////////////////////////////////////////////////////////////////
// nsIWMPControls

/* boolean isAvailable (in AString item); */
NS_IMETHODIMP nsWMPControls::IsAvailable(const nsAString &aItem, PRBool *_retval)
{
    IMPL_GETPROP_BOOL_NAMED(IWMPControls, isAvailable, aItem, _retval);
}

/* void play (); */
NS_IMETHODIMP nsWMPControls::Play()
{
    IMPL_COMMON(IWMPControls);
    return mOwner->HR2NS(i->play());
}

/* void stop (); */
NS_IMETHODIMP nsWMPControls::Stop()
{
    IMPL_COMMON(IWMPControls);
    return mOwner->HR2NS(i->stop());
}

/* void pause (); */
NS_IMETHODIMP nsWMPControls::Pause()
{
    IMPL_COMMON(IWMPControls);
    return mOwner->HR2NS(i->pause());
}

/* void fastForward (); */
NS_IMETHODIMP nsWMPControls::FastForward()
{
    IMPL_COMMON(IWMPControls);
    return mOwner->HR2NS(i->fastForward());
}

/* void fastReverse (); */
NS_IMETHODIMP nsWMPControls::FastReverse()
{
    IMPL_COMMON(IWMPControls);
    return mOwner->HR2NS(i->fastReverse());
}

/* attribute double currentPosition; */
NS_IMETHODIMP nsWMPControls::GetCurrentPosition(double *aCurrentPosition)
{
    IMPL_GETPROP_DOUBLE(IWMPControls, currentPosition, aCurrentPosition);
}
NS_IMETHODIMP nsWMPControls::SetCurrentPosition(double aCurrentPosition)
{
    IMPL_SETPROP_DOUBLE(IWMPControls, currentPosition, aCurrentPosition);
}

/* readonly attribute AString currentPositionString; */
NS_IMETHODIMP nsWMPControls::GetCurrentPositionString(nsAString &aCurrentPositionString)
{
    IMPL_GETPROP_BSTR(IWMPControls, currentPositionString, aCurrentPositionString);
}

/* void next (); */
NS_IMETHODIMP nsWMPControls::Next()
{
    IMPL_COMMON(IWMPControls);
    return mOwner->HR2NS(i->next());
}

/* void previous (); */
NS_IMETHODIMP nsWMPControls::Previous()
{
    IMPL_COMMON(IWMPControls);
    return mOwner->HR2NS(i->previous());
}

/* attribute nsIWMPMedia currentItem; */
NS_IMETHODIMP nsWMPControls::GetCurrentItem(nsIWMPMedia * *aCurrentItem)
{
    IMPL_COMMON(IWMPControls);
    // TODO
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsWMPControls::SetCurrentItem(nsIWMPMedia * aCurrentItem)
{
    IMPL_COMMON(IWMPControls);
    // TODO
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long currentMarker; */
NS_IMETHODIMP nsWMPControls::GetCurrentMarker(PRInt32 *aCurrentMarker)
{
    IMPL_GETPROP_LONG(IWMPControls, currentMarker, aCurrentMarker);
}
NS_IMETHODIMP nsWMPControls::SetCurrentMarker(PRInt32 aCurrentMarker)
{
    IMPL_SETPROP_LONG(IWMPControls, currentMarker, aCurrentMarker);
}

/* void playItem (in nsIWMPMedia aItem); */
NS_IMETHODIMP nsWMPControls::PlayItem(nsIWMPMedia *aItem)
{
    IMPL_COMMON(IWMPControls);
    // TODO
    return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


/* Implementation file */
NS_IMPL_ISUPPORTS2(nsWMPSettings, nsIWMPSettings, nsIClassInfo)

nsWMPSettings::nsWMPSettings(nsWMPScriptablePeer *pOwner) :
    mOwner(pOwner)
{
    NS_INIT_ISUPPORTS();
    /* member initializers and constructor code */
}

nsWMPSettings::~nsWMPSettings()
{
    /* destructor code */
}

HRESULT
nsWMPSettings::GetIWMPSettings(IWMPSettings **ps)
{
    *ps = NULL;
    CComPtr<IWMPCore> wmpc;
    HRESULT hr = mOwner->GetIWMPCore(&wmpc);
    if (FAILED(hr)) return hr;
    return wmpc->get_settings(ps);
}

///////////////////////////////////////////////////////////////////////////////
// nsIWMPControls

/* boolean isAvailable (in AString aItem); */
NS_IMETHODIMP nsWMPSettings::IsAvailable(const nsAString & aItem, PRBool *_retval)
{
    IMPL_GETPROP_BOOL_NAMED(IWMPSettings, isAvailable, aItem, _retval);
}

/* attribute boolean autoStart; */
NS_IMETHODIMP nsWMPSettings::GetAutoStart(PRBool *aAutoStart)
{
    IMPL_GETPROP_BOOL(IWMPSettings, autoStart, aAutoStart);
}
NS_IMETHODIMP nsWMPSettings::SetAutoStart(PRBool aAutoStart)
{
    IMPL_SETPROP_BOOL(IWMPSettings, autoStart, aAutoStart);
}

/* attribute AString baseURL; */
NS_IMETHODIMP nsWMPSettings::GetBaseURL(nsAString & aBaseURL)
{
    IMPL_GETPROP_BSTR(IWMPSettings, baseURL, aBaseURL);
}
NS_IMETHODIMP nsWMPSettings::SetBaseURL(const nsAString & aBaseURL)
{
    IMPL_SETPROP_BSTR(IWMPSettings, baseURL, aBaseURL);
}

/* attribute AString defaultFrame; */
NS_IMETHODIMP nsWMPSettings::GetDefaultFrame(nsAString & aDefaultFrame)
{
    IMPL_GETPROP_BSTR(IWMPSettings, defaultFrame, aDefaultFrame);
}
NS_IMETHODIMP nsWMPSettings::SetDefaultFrame(const nsAString & aDefaultFrame)
{
    IMPL_SETPROP_BSTR(IWMPSettings, defaultFrame, aDefaultFrame);
}

/* attribute boolean invokeURLs; */
NS_IMETHODIMP nsWMPSettings::GetInvokeURLs(PRBool *aInvokeURLs)
{
    IMPL_GETPROP_BOOL(IWMPSettings, invokeURLs, aInvokeURLs);
}
NS_IMETHODIMP nsWMPSettings::SetInvokeURLs(PRBool aInvokeURLs)
{
    IMPL_SETPROP_BOOL(IWMPSettings, invokeURLs, aInvokeURLs);
}

/* attribute boolean mute; */
NS_IMETHODIMP nsWMPSettings::GetMute(PRBool *aMute)
{
    IMPL_GETPROP_BOOL(IWMPSettings, mute, aMute);
}
NS_IMETHODIMP nsWMPSettings::SetMute(PRBool aMute)
{
    IMPL_SETPROP_BOOL(IWMPSettings, mute, aMute);
}

/* attribute long playCount; */
NS_IMETHODIMP nsWMPSettings::GetPlayCount(PRInt32 *aPlayCount)
{
    IMPL_GETPROP_LONG(IWMPSettings, playCount, aPlayCount);
}
NS_IMETHODIMP nsWMPSettings::SetPlayCount(PRInt32 aPlayCount)
{
    IMPL_SETPROP_LONG(IWMPSettings, playCount, aPlayCount);
}

/* attribute double rate; */
NS_IMETHODIMP nsWMPSettings::GetRate(double *aRate)
{
    IMPL_GETPROP_DOUBLE(IWMPSettings, rate, aRate);
}
NS_IMETHODIMP nsWMPSettings::SetRate(double aRate)
{
    IMPL_SETPROP_DOUBLE(IWMPSettings, rate, aRate);
}

/* attribute long balance; */
NS_IMETHODIMP nsWMPSettings::GetBalance(PRInt32 *aBalance)
{
    IMPL_GETPROP_LONG(IWMPSettings, balance, aBalance);
}
NS_IMETHODIMP nsWMPSettings::SetBalance(PRInt32 aBalance)
{
    IMPL_SETPROP_LONG(IWMPSettings, balance, aBalance);
}

/* attribute long volume; */
NS_IMETHODIMP nsWMPSettings::GetVolume(PRInt32 *aVolume)
{
    IMPL_GETPROP_LONG(IWMPSettings, volume, aVolume);
}
NS_IMETHODIMP nsWMPSettings::SetVolume(PRInt32 aVolume)
{
    IMPL_SETPROP_LONG(IWMPSettings, volume, aVolume);
}

/* boolean getMode (in AString aMode); */
NS_IMETHODIMP nsWMPSettings::GetMode(const nsAString & aMode, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setMode (in AString aMode, in boolean aValue); */
NS_IMETHODIMP nsWMPSettings::SetMode(const nsAString & aMode, PRBool aValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute boolean enableErrorDialogs; */
NS_IMETHODIMP nsWMPSettings::GetEnableErrorDialogs(PRBool *aEnableErrorDialogs)
{
    IMPL_GETPROP_BOOL(IWMPSettings, enableErrorDialogs, aEnableErrorDialogs);
}
NS_IMETHODIMP nsWMPSettings::SetEnableErrorDialogs(PRBool aEnableErrorDialogs)
{
    IMPL_SETPROP_BOOL(IWMPSettings, enableErrorDialogs, aEnableErrorDialogs);
}

