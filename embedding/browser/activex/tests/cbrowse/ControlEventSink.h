// ControlEventSink.h : Declaration of the CBrowseEventSink

#ifndef __CONTROLEVENTSINK_H_
#define __CONTROLEVENTSINK_H_

#include "CBrowseDlg.h"
#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CBrowseEventSink
class ATL_NO_VTABLE CBrowseEventSink : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CBrowseEventSink, &CLSID_ControlEventSink>,
	public IDispatch
{
public:
	CBrowseEventSink()
	{
		m_pBrowseDlg = NULL;
	}

	CBrowseDlg *m_pBrowseDlg;

DECLARE_REGISTRY_RESOURCEID(IDR_CONTROLEVENTSINK)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CBrowseEventSink)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY_IID(DIID_DWebBrowserEvents2, IDispatch)
END_COM_MAP()

// IDispatch
public:
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount( 
        /* [out] */ UINT __RPC_FAR *pctinfo);
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo( 
        /* [in] */ UINT iTInfo,
        /* [in] */ LCID lcid,
        /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames( 
        /* [in] */ REFIID riid,
        /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
        /* [in] */ UINT cNames,
        /* [in] */ LCID lcid,
        /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Invoke( 
        /* [in] */ DISPID dispIdMember,
        /* [in] */ REFIID riid,
        /* [in] */ LCID lcid,
        /* [in] */ WORD wFlags,
        /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
        /* [out] */ VARIANT __RPC_FAR *pVarResult,
        /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
        /* [out] */ UINT __RPC_FAR *puArgErr);
};

typedef CComObject<CBrowseEventSink> CBrowseEventSinkInstance;

#endif //__CONTROLEVENTSINK_H_
