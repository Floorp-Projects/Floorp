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

#include <mshtml.h>

#include "XPConnect.h"
#include "LegacyPlugin.h"

#include "npapi.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMLocation.h"

class HTMLDocument :
    public CComObjectRootEx<CComSingleThreadModel>,
    public IHTMLDocument2,
    public IServiceProvider
{
public:
    PluginInstanceData *mData;

    HTMLDocument()
    {
        xpc_AddRef();
    }

    virtual ~HTMLDocument()
    {
        xpc_Release();
    }

BEGIN_COM_MAP(HTMLDocument)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLDocument)
    COM_INTERFACE_ENTRY(IHTMLDocument2)
    COM_INTERFACE_ENTRY(IServiceProvider)
END_COM_MAP()

// IServiceProvider
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE QueryService( 
        /* [in] */ REFGUID guidService,
        /* [in] */ REFIID riid,
        /* [out] */ void **ppvObject)
    {
#ifdef DEBUG
        ATLTRACE(_T("HTMLDocument::QueryService\n"));
        if (IsEqualIID(riid, __uuidof(IWebBrowser)))
        {
            ATLTRACE(_T("  IWebBrowser\n"));
        }
        else if (IsEqualIID(riid, __uuidof(IWebBrowser2)))
        {
            ATLTRACE(_T("  IWebBrowser2\n"));
        }
        else if (IsEqualIID(riid, __uuidof(IWebBrowserApp)))
        {
            ATLTRACE(_T("  IWebBrowserApp\n"));
        }
        else if (IsEqualIID(riid, __uuidof(IHTMLWindow2)))
        {
            ATLTRACE(_T("  IHTMLWindow2\n"));
        }
        else if (IsEqualIID(riid, __uuidof(IHTMLDocument2)))
        {
            ATLTRACE(_T("  IHTMLDocument2\n"));
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

// IDispatch
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount( 
        /* [out] */ UINT *pctinfo)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo( 
        /* [in] */ UINT iTInfo,
        /* [in] */ LCID lcid,
        /* [out] */ ITypeInfo **ppTInfo)
    {
        return E_NOTIMPL;
    }


    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames( 
        /* [in] */ REFIID riid,
        /* [size_is][in] */ LPOLESTR *rgszNames,
        /* [in] */ UINT cNames,
        /* [in] */ LCID lcid,
        /* [size_is][out] */ DISPID *rgDispId)
    {
        return E_NOTIMPL;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Invoke( 
        /* [in] */ DISPID dispIdMember,
        /* [in] */ REFIID riid,
        /* [in] */ LCID lcid,
        /* [in] */ WORD wFlags,
        /* [out][in] */ DISPPARAMS *pDispParams,
        /* [out] */ VARIANT *pVarResult,
        /* [out] */ EXCEPINFO *pExcepInfo,
        /* [out] */ UINT *puArgErr)
    {
        return E_NOTIMPL;
    }
        
// IHTMLDocument
    virtual /* [nonbrowsable][hidden][id][propget] */ HRESULT STDMETHODCALLTYPE get_Script( 
        /* [out][retval] */ IDispatch **p)
    {
        return E_NOTIMPL;
    }

// IHTMLDocument2
    virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_all( 
        /* [out][retval] */ IHTMLElementCollection **p)
    {
        return E_NOTIMPL;
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
        return E_NOTIMPL;
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
        *p = NULL;
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
                    *p = ::SysAllocString(s);
                    return S_OK;
                }
            }
        }
        return E_FAIL;
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
        return E_NOTIMPL;
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
};

HRESULT xpc_GetServiceProvider(PluginInstanceData *pData, IServiceProvider **pSP)
{
    CComObject<HTMLDocument> *pDoc = NULL;
    CComObject<HTMLDocument>::CreateInstance(&pDoc);
    pDoc->mData = pData;
    return pDoc->QueryInterface(_uuidof(IServiceProvider), (void **) pSP);
}
