// nsXPCDispTestWrappedJS.h: Definition of the nsXPCDispTestWrappedJS class
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NSXPCDISPTESTWRAPPEDJS_H__DAAB3C99_1894_40C2_B12B_A360739F8977__INCLUDED_)
#define AFX_NSXPCDISPTESTWRAPPEDJS_H__DAAB3C99_1894_40C2_B12B_A360739F8977__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// nsXPCDispTestWrappedJS

class nsXPCDispTestWrappedJS : 
	public IDispatchImpl<nsIXPCDispTestWrappedJS, &IID_nsIXPCDispTestWrappedJS, &LIBID_IDispatchTestLib>, 
	public ISupportErrorInfo,
	public CComObjectRoot,
	public CComCoClass<nsXPCDispTestWrappedJS,&CLSID_nsXPCDispTestWrappedJS>
{
public:
	nsXPCDispTestWrappedJS() {}
BEGIN_CATEGORY_MAP(nsXPCDispTestWrappedJS)
    IMPLEMENTED_CATEGORY(CATID_SafeForScripting)
END_CATEGORY_MAP()
BEGIN_COM_MAP(nsXPCDispTestWrappedJS)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(nsIXPCDispTestWrappedJS)
	COM_INTERFACE_ENTRY(ISupportErrorInfo)
END_COM_MAP()
DECLARE_NOT_AGGREGATABLE(nsXPCDispTestWrappedJS) 

DECLARE_REGISTRY_RESOURCEID(IDR_nsXPCDispTestWrappedJS)
// ISupportsErrorInfo
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// nsIXPCDispTestWrappedJS
public:
    /**
     * This tests interacting with JS Objects via IDispatch
     * @param obj the object being tested
     * @param errMsg string receiving any error message, blank if no error
     */
	STDMETHOD(TestParamTypes)(/*[in]*/ IDispatch * obj, 
                              /*[out]*/BSTR * errMsg);
};

#endif // !defined(AFX_NSXPCDISPTESTWRAPPEDJS_H__DAAB3C99_1894_40C2_B12B_A360739F8977__INCLUDED_)
