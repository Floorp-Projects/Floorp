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

#define WM_NPP_NEWSTREAM      WM_USER 
#define WM_NPP_DESTROYSTREAM  WM_USER + 1
#define WM_NPP_URLNOTIFY      WM_USER + 2
#define WM_NPP_WRITEREADY     WM_USER + 3
#define WM_NPP_WRITE          WM_USER + 4

#define WM_CLASS_CLEANUP      WM_USER + 10
#define WM_CLASS_CREATEPLUGININSTANCE WM_USER + 11

struct _DestroyStreamData
{
    NPP npp;
    NPStream *stream;
    NPReason reason;
};

struct _UrlNotifyData
{
    NPP npp;
    char *url;
    NPReason reason;
    void *notifydata;
};

struct _NewStreamData
{
    NPP npp;
    char *contenttype;
    NPStream *stream;
    NPBool seekable;
    uint16 *stype;
};

struct _WriteReadyData
{
    NPP npp;
    NPStream *stream;
    int32 result;
};

struct _WriteData
{
    NPP npp;
    NPStream *stream;
    int32 offset;
    int32 len;
    void* buffer;
};

/////////////////////////////////////////////////////////////////////////////
// nsURLDataCallback
class ATL_NO_VTABLE nsURLDataCallback : 
	public CComObjectRootEx<CComMultiThreadModel>,
    public CWindowImpl<nsURLDataCallback, CWindow, CNullTraits>,
	public CComCoClass<nsURLDataCallback, &CLSID_NULL>,
	public IBindStatusCallback
{
public:
	nsURLDataCallback();

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(nsURLDataCallback)
	COM_INTERFACE_ENTRY(IBindStatusCallback)
END_COM_MAP()

    DECLARE_WND_CLASS(_T("MozStreamWindow"))

BEGIN_MSG_MAP(nsURLDataCallback)
    MESSAGE_HANDLER(WM_NPP_NEWSTREAM, OnNPPNewStream)
    MESSAGE_HANDLER(WM_NPP_DESTROYSTREAM, OnNPPDestroyStream)
    MESSAGE_HANDLER(WM_NPP_URLNOTIFY, OnNPPURLNotify)
    MESSAGE_HANDLER(WM_NPP_WRITEREADY, OnNPPWriteReady)
    MESSAGE_HANDLER(WM_NPP_WRITE, OnNPPWrite)
    MESSAGE_HANDLER(WM_CLASS_CLEANUP, OnClassCleanup)
    MESSAGE_HANDLER(WM_CLASS_CREATEPLUGININSTANCE, OnClassCreatePluginInstance)
END_MSG_MAP()

    LRESULT OnNPPNewStream(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnNPPDestroyStream(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnNPPURLNotify(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnNPPWriteReady(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnNPPWrite(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

    LRESULT OnClassCreatePluginInstance(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnClassCleanup(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

protected:
    virtual ~nsURLDataCallback();

protected:
    nsPluginHostCtrl *m_pOwner;
    void *m_pNotifyData;
    HGLOBAL m_hPostData;

    NPStream m_NPStream;
    unsigned long m_nDataPos;
    unsigned long m_nDataMax;

    char *m_szContentType;
    char *m_szURL;

    CComPtr<IBinding> m_cpBinding;

    void SetURL(const char *szURL)
    {
        if (m_szURL) { free(m_szURL); m_szURL = NULL; }
        if (szURL) { m_szURL = strdup(szURL); }
    }
    void SetContentType(const char *szContentType)
    {
        if (m_szContentType) { free(m_szContentType); m_szContentType = NULL; }
        if (szContentType) { m_szContentType = strdup(szContentType); }
    }
    void SetPostData(const void *pData, unsigned long nSize);
    void SetOwner(nsPluginHostCtrl *pOwner) { m_pOwner = pOwner; }
    void SetNotifyData(void *pNotifyData)   { m_pNotifyData = pNotifyData; }
    
    static void __cdecl StreamThread(void *pThis);

public:
    static HRESULT OpenURL(nsPluginHostCtrl *pOwner, const TCHAR *szURL, void *pNotifyData, const void *pData, unsigned long nSize);

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
