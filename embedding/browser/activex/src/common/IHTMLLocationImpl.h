/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Adam Lock <adamlock@eircom.net>
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

#ifndef IHTMLLOCATIONIMPL_H
#define IHTMLLOCATIONIMPL_H

#include "nsIDOMLocation.h"

#define IHTMLLOCATION_GET_IMPL(prop) \
    if (!p) return E_INVALIDARG; \
    nsCOMPtr<nsIDOMLocation> location; \
    if (NS_FAILED(GetDOMLocation(getter_AddRefs(location))) || !location) \
        return E_UNEXPECTED; \
    nsAutoString value; \
    NS_ENSURE_SUCCESS(location->Get ## prop(value), E_UNEXPECTED); \
    *p = ::SysAllocString(value.get()); \
    return (*p) ? S_OK : E_OUTOFMEMORY;

#define IHTMLLOCATION_PUT_IMPL(prop) \
    return E_NOTIMPL; // For now

template<class T>
class IHTMLLocationImpl :
    public IDispatchImpl<IHTMLLocation, &__uuidof(IHTMLLocation), &LIBID_MSHTML>
{
protected:
// Methods to be implemented by the derived class
    virtual nsresult GetDOMLocation(nsIDOMLocation **aLocation) = 0;
public:
    
// IHTMLLocation
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_href( 
        /* [in] */ BSTR v)
    {
        IHTMLLOCATION_PUT_IMPL(Href);
    }
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_href( 
        /* [out][retval] */ BSTR *p)
    {
        IHTMLLOCATION_GET_IMPL(Href);
    }
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_protocol( 
        /* [in] */ BSTR v)
    {
        IHTMLLOCATION_PUT_IMPL(Protocol);
    }
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_protocol( 
        /* [out][retval] */ BSTR *p)
    {
        IHTMLLOCATION_GET_IMPL(Protocol);
    }
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_host( 
        /* [in] */ BSTR v)
    {
        IHTMLLOCATION_PUT_IMPL(Host);
    }
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_host( 
        /* [out][retval] */ BSTR *p)
    {
        IHTMLLOCATION_GET_IMPL(Host);
    }
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_hostname( 
        /* [in] */ BSTR v)
    {
        IHTMLLOCATION_PUT_IMPL(Hostname);
    }
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_hostname( 
        /* [out][retval] */ BSTR *p)
    {
        IHTMLLOCATION_GET_IMPL(Hostname);
    }
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_port( 
        /* [in] */ BSTR v)
    {
        IHTMLLOCATION_PUT_IMPL(Port);
    }
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_port( 
        /* [out][retval] */ BSTR *p)
    {
        IHTMLLOCATION_GET_IMPL(Port);
    }
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_pathname( 
        /* [in] */ BSTR v)
    {
        IHTMLLOCATION_PUT_IMPL(Pathname);
    }
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_pathname( 
        /* [out][retval] */ BSTR *p)
    {
        IHTMLLOCATION_GET_IMPL(Pathname);
    }
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_search( 
        /* [in] */ BSTR v)
    {
        IHTMLLOCATION_PUT_IMPL(Search);
    }
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_search( 
        /* [out][retval] */ BSTR *p)
    {
        IHTMLLOCATION_GET_IMPL(Search);
    }
    virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_hash( 
        /* [in] */ BSTR v)
    {
        IHTMLLOCATION_PUT_IMPL(Hash);
    }
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_hash( 
        /* [out][retval] */ BSTR *p)
    {
        IHTMLLOCATION_GET_IMPL(Hash);
    }
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE reload( 
        /* [in][defaultvalue] */ VARIANT_BOOL flag)
    {
        nsCOMPtr<nsIDOMLocation> location;
        if (NS_FAILED(GetDOMLocation(getter_AddRefs(location))) || !location)
            return E_UNEXPECTED;
        return NS_SUCCEEDED(location->Reload(flag)) ? S_OK : E_FAIL;
    }
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE replace( 
        /* [in] */ BSTR bstr)
    {
        nsCOMPtr<nsIDOMLocation> location;
        if (NS_FAILED(GetDOMLocation(getter_AddRefs(location))) || !location)
            return E_UNEXPECTED;
        nsAutoString value(bstr);
        return NS_SUCCEEDED(location->Replace(value)) ? S_OK : E_FAIL;
    }
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE assign( 
        /* [in] */ BSTR bstr)
    {
        nsCOMPtr<nsIDOMLocation> location;
        if (NS_FAILED(GetDOMLocation(getter_AddRefs(location))) || !location)
            return E_UNEXPECTED;
        nsAutoString value(bstr);
        return NS_SUCCEEDED(location->Assign(value)) ? S_OK : E_FAIL;
    }
    virtual /* [id] */ HRESULT STDMETHODCALLTYPE toString( 
        /* [out][retval] */ BSTR *string)
    {
        if (!string) return E_INVALIDARG;
        nsCOMPtr<nsIDOMLocation> location;
        if (NS_FAILED(GetDOMLocation(getter_AddRefs(location))) || !location)
            return E_UNEXPECTED;
        nsAutoString value;
        NS_ENSURE_SUCCESS(location->ToString(value), E_UNEXPECTED);
        *string = ::SysAllocString(value.get());
        return (*string) ? S_OK : E_OUTOFMEMORY;
    }
};

#endif