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
 *   Adam Lock <adamlock@netscape.com> 
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
#ifndef __NSURLDATACALLBACK_H_
#define __NSURLDATACALLBACK_H_

#include "resource.h"       // main symbols

#include "npapi.h"

class nsPluginHostCtrl;

/////////////////////////////////////////////////////////////////////////////
// nsURLDataCallback
class ATL_NO_VTABLE nsURLDataCallback : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<nsURLDataCallback, &CLSID_NULL>,
	public IBindStatusCallback
{
public:
	nsURLDataCallback();

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(nsURLDataCallback)
	COM_INTERFACE_ENTRY(IBindStatusCallback)
END_COM_MAP()

protected:
    virtual ~nsURLDataCallback();

protected:
    nsPluginHostCtrl *m_pOwner;
    void *m_pNotifyData;
    HGLOBAL m_hPostData;

    NPStream m_NPStream;
    unsigned long m_nDataPos;

    char *m_szContentType;
    char *m_szURL;

    CComPtr<IBinding> m_cpBinding;

public:
    void SetPostData(const void *pData, unsigned long nSize);
    void SetOwner(nsPluginHostCtrl *pOwner) { m_pOwner = pOwner; }
    void SetNotifyData(void *pNotifyData)   { m_pNotifyData = pNotifyData; }

// IBindStatusCallback
public:
    virtual HRESULT STDMETHODCALLTYPE OnStartBinding( 
        /* [in] */ DWORD dwReserved,
        /* [in] */ IBinding __RPC_FAR *pib);
    
    virtual HRESULT STDMETHODCALLTYPE GetPriority( 
        /* [out] */ LONG __RPC_FAR *pnPriority);
    
    virtual HRESULT STDMETHODCALLTYPE OnLowResource( 
        /* [in] */ DWORD reserved);
    
    virtual HRESULT STDMETHODCALLTYPE OnProgress( 
        /* [in] */ ULONG ulProgress,
        /* [in] */ ULONG ulProgressMax,
        /* [in] */ ULONG ulStatusCode,
        /* [in] */ LPCWSTR szStatusText);
    
    virtual HRESULT STDMETHODCALLTYPE OnStopBinding( 
        /* [in] */ HRESULT hresult,
        /* [unique][in] */ LPCWSTR szError);
    
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE GetBindInfo( 
        /* [out] */ DWORD __RPC_FAR *grfBINDF,
        /* [unique][out][in] */ BINDINFO __RPC_FAR *pbindinfo);
    
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE OnDataAvailable( 
        /* [in] */ DWORD grfBSCF,
        /* [in] */ DWORD dwSize,
        /* [in] */ FORMATETC __RPC_FAR *pformatetc,
        /* [in] */ STGMEDIUM __RPC_FAR *pstgmed);
    
    virtual HRESULT STDMETHODCALLTYPE OnObjectAvailable( 
        /* [in] */ REFIID riid,
        /* [iid_is][in] */ IUnknown __RPC_FAR *punk);
};

#endif //__NSURLDATACALLBACK_H_
