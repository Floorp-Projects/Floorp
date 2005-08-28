/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *
 *   Adam Lock <adamlock@eircom.net> 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "stdafx.h"

#include "Pluginhostctrl.h"
#include "nsPluginHostCtrl.h"

/////////////////////////////////////////////////////////////////////////////
// nsPluginHostCtrl

nsPluginHostCtrl::nsPluginHostCtrl()
{
    m_bWindowOnly = TRUE;
}

nsPluginHostCtrl::~nsPluginHostCtrl()
{
}


HRESULT nsPluginHostCtrl::GetWebBrowserApp(IWebBrowserApp **pBrowser)
{
    ATLASSERT(pBrowser);
    if (!pBrowser)
    {
        return E_INVALIDARG;
    }

    // Get the web browser through the site the control is attached to.
    // Note: The control could be running in some other container than IE
    //       so code shouldn't expect this function to work all the time.

    CComPtr<IWebBrowserApp> cpWebBrowser;
    CComQIPtr<IServiceProvider, &IID_IServiceProvider> cpServiceProvider = m_spClientSite;

    HRESULT hr;
    if (cpServiceProvider)
    {
        hr = cpServiceProvider->QueryService(IID_IWebBrowserApp, &cpWebBrowser);
    }
    if (!cpWebBrowser)
    {
        return E_FAIL;
    }

    *pBrowser = cpWebBrowser;
    (*pBrowser)->AddRef();

    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// IMozPluginHostCtrl

STDMETHODIMP nsPluginHostCtrl::Load(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog)
{
    CComQIPtr<IPropertyBag2> cpPropBag2 = pPropBag;
    if (cpPropBag2)
    {
        // Read *all* the properties via IPropertyBag2 and store them somewhere
        // so they can be fed into the plugin instance at creation..
        ULONG nProperties;
        cpPropBag2->CountProperties(&nProperties);
        if (nProperties > 0)
        {
            PROPBAG2 *pProperties = (PROPBAG2 *) malloc(sizeof(PROPBAG2) * nProperties);
            ULONG nPropertiesGotten = 0;
            cpPropBag2->GetPropertyInfo(0, nProperties, pProperties, &nPropertiesGotten);
            for (ULONG i = 0; i < nPropertiesGotten; i++)
            {
                if (pProperties[i].vt == VT_BSTR)
                {
                    USES_CONVERSION;
                    CComVariant v;
                    HRESULT hrRead;
                    cpPropBag2->Read(1, &pProperties[i], NULL, &v, &hrRead);
                    AddPluginParam(OLE2A(pProperties[i].pstrName), OLE2A(v.bstrVal));
                }
                if (pProperties[i].pstrName)
                {
                    CoTaskMemFree(pProperties[i].pstrName);
                }
            }
            free(pProperties);
        }
    }
    return IPersistPropertyBagImpl<nsPluginHostCtrl>::Load(pPropBag, pErrorLog);
}

///////////////////////////////////////////////////////////////////////////////
// IMozPluginHostCtrl

STDMETHODIMP nsPluginHostCtrl::get_PluginContentType(BSTR *pVal)
{
    return GetPluginContentType(pVal);
}

STDMETHODIMP nsPluginHostCtrl::put_PluginContentType(BSTR newVal)
{
    return SetPluginContentType(newVal);
}

STDMETHODIMP nsPluginHostCtrl::get_PluginSource(BSTR *pVal)
{
    return GetPluginSource(pVal);
}

STDMETHODIMP nsPluginHostCtrl::put_PluginSource(BSTR newVal)
{
    return SetPluginSource(newVal);
}

STDMETHODIMP nsPluginHostCtrl::get_PluginsPage(BSTR *pVal)
{
    return GetPluginsPage(pVal);
}

STDMETHODIMP nsPluginHostCtrl::put_PluginsPage(BSTR newVal)
{
    return SetPluginsPage(newVal);
}
