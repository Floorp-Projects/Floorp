// XMLElementCollection.h : Declaration of the CXMLElementCollection

#ifndef __XMLELEMENTCOLLECTION_H_
#define __XMLELEMENTCOLLECTION_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CXMLElementCollection
class ATL_NO_VTABLE CXMLElementCollection : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CXMLElementCollection, &CLSID_MozXMLElementCollection>,
	public IDispatchImpl<IXMLElementCollection, &IID_IXMLElementCollection, &LIBID_MozActiveXMLLib>
{
	// List of elements
	ElementList m_cElements;

public:
	CXMLElementCollection();
	virtual ~CXMLElementCollection();

DECLARE_REGISTRY_RESOURCEID(IDR_XMLELEMENTCOLLECTION)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CXMLElementCollection)
	COM_INTERFACE_ENTRY(IXMLElementCollection)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

// IXMLElementCollection
	virtual HRESULT STDMETHODCALLTYPE put_length(/* [in] */ long v);
	virtual HRESULT STDMETHODCALLTYPE get_length(/* [out][retval] */ long __RPC_FAR *p);
	virtual HRESULT STDMETHODCALLTYPE get__newEnum(/* [out][retval] */ IUnknown __RPC_FAR *__RPC_FAR *ppUnk);
	virtual HRESULT STDMETHODCALLTYPE item(/* [in][optional] */ VARIANT var1, /* [in][optional] */ VARIANT var2, /* [out][retval] */ IDispatch __RPC_FAR *__RPC_FAR *ppDisp);

public:
	HRESULT Add(IXMLElement *pElement);
};

typedef CComObject<CXMLElementCollection> CXMLElementCollectionInstance;

#endif //__XMLELEMENTCOLLECTION_H_
