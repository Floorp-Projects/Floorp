// XMLDocument.h : Declaration of the CXMLDocument

#ifndef __XMLDOCUMENT_H_
#define __XMLDOCUMENT_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CXMLDocument
class ATL_NO_VTABLE CXMLDocument : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CXMLDocument, &CLSID_MozXMLDocument>,
	public ISupportErrorInfo,
	public IDispatchImpl<IXMLDocument, &IID_IXMLDocument, &LIBID_MozActiveXMLLib>,
	public IPersistMoniker,
	public IPersistStreamInit
{
public:
	CXMLDocument();
	virtual ~CXMLDocument();


DECLARE_REGISTRY_RESOURCEID(IDR_XMLDOCUMENT)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CXMLDocument)
	COM_INTERFACE_ENTRY(IXMLDocument)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IPersistMoniker)
	COM_INTERFACE_ENTRY(IPersistStreamInit)
//	COM_INTERFACE_ENTRY(ISupportErrorInfo)
END_COM_MAP()

	LONG m_nReadyState;
	std::string m_szURL;
	CComQIPtr<IXMLElement, &IID_IXMLElement> m_spRoot;

// ISupportsErrorInfo
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// IPersistStreamInit
	//virtual HRESULT STDMETHODCALLTYPE IsDirty(void);
	HRESULT STDMETHODCALLTYPE Load(/* [in] */ LPSTREAM pStm);
	HRESULT STDMETHODCALLTYPE Save(/* [in] */ LPSTREAM pStm, /* [in] */ BOOL fClearDirty);
	HRESULT STDMETHODCALLTYPE GetSizeMax(/* [out] */ ULARGE_INTEGER __RPC_FAR *pCbSize);
	HRESULT STDMETHODCALLTYPE InitNew(void);

// IPersistMoniker
	HRESULT STDMETHODCALLTYPE GetClassID(/* [out] */ CLSID __RPC_FAR *pClassID);
	HRESULT STDMETHODCALLTYPE IsDirty(void);
	HRESULT STDMETHODCALLTYPE Load(/* [in] */ BOOL fFullyAvailable, /* [in] */ IMoniker __RPC_FAR *pimkName, /* [in] */ LPBC pibc, /* [in] */ DWORD grfMode);
	HRESULT STDMETHODCALLTYPE Save(/* [in] */ IMoniker __RPC_FAR *pimkName, /* [in] */ LPBC pbc, /* [in] */ BOOL fRemember);
	HRESULT STDMETHODCALLTYPE SaveCompleted(/* [in] */ IMoniker __RPC_FAR *pimkName, /* [in] */ LPBC pibc);
	HRESULT STDMETHODCALLTYPE GetCurMoniker(/* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppimkName);

// IXMLError
	HRESULT STDMETHODCALLTYPE GetErrorInfo(XML_ERROR __RPC_FAR *pErrorReturn);

// IXMLDocument
	HRESULT STDMETHODCALLTYPE get_root(/* [out][retval] */ IXMLElement __RPC_FAR *__RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE get_fileSize(/* [out][retval] */ BSTR __RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE get_fileModifiedDate(/* [out][retval] */ BSTR __RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE get_fileUpdatedDate(/* [out][retval] */ BSTR __RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE get_URL(/* [out][retval] */ BSTR __RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE put_URL(/* [in] */ BSTR p);
	HRESULT STDMETHODCALLTYPE get_mimeType(/* [out][retval] */ BSTR __RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE get_readyState(/* [out][retval] */ long __RPC_FAR *pl);
	HRESULT STDMETHODCALLTYPE get_charset(/* [out][retval] */ BSTR __RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE put_charset(/* [in] */ BSTR p);
	HRESULT STDMETHODCALLTYPE get_version(/* [out][retval] */ BSTR __RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE get_doctype(/* [out][retval] */ BSTR __RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE get_dtdURL(/* [out][retval] */ BSTR __RPC_FAR *p);
	HRESULT STDMETHODCALLTYPE createElement(/* [in] */ VARIANT vType, /* [in][optional] */ VARIANT var1, /* [out][retval] */ IXMLElement __RPC_FAR *__RPC_FAR *ppElem);
public:
};

typedef CComObject<CXMLDocument> CXMLDocumentInstance;

#endif //__XMLDOCUMENT_H_
