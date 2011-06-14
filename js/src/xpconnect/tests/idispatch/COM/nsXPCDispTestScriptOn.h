// nsXPCDispTestScriptOn.h: Definition of the nsXPCDispTestScriptOn class
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NSXPCDISPTESTSCRIPTON_H__C037D462_C403_48FF_A02F_6C36544F0833__INCLUDED_)
#define AFX_NSXPCDISPTESTSCRIPTON_H__C037D462_C403_48FF_A02F_6C36544F0833__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"       // main symbols
#include <ATLCTL.H>

/////////////////////////////////////////////////////////////////////////////
// nsXPCDispTestScriptOn

class nsXPCDispTestScriptOn : 
    public IDispatchImpl<nsIXPCDispTestScriptOn, &IID_nsIXPCDispTestScriptOn, &LIBID_IDispatchTestLib>, 
    public ISupportErrorInfo,
    public CComObjectRoot,
    public CComCoClass<nsXPCDispTestScriptOn,&CLSID_nsXPCDispTestScriptOn>,
    public IObjectSafetyImpl<nsXPCDispTestScriptOn,
                             INTERFACESAFE_FOR_UNTRUSTED_CALLER | 
                                 INTERFACESAFE_FOR_UNTRUSTED_DATA>
{
public:
    nsXPCDispTestScriptOn();
BEGIN_COM_MAP(nsXPCDispTestScriptOn)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(nsIXPCDispTestScriptOn)
    COM_INTERFACE_ENTRY(IObjectSafety)
    COM_INTERFACE_ENTRY(ISupportErrorInfo)
END_COM_MAP()
DECLARE_NOT_AGGREGATABLE(nsXPCDispTestScriptOn) 

DECLARE_REGISTRY_RESOURCEID(IDR_nsXPCDispTestScriptOn)
// ISupportsErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// nsIXPCDispTestScriptOn
public:
};

#endif // !defined(AFX_NSXPCDISPTESTSCRIPTON_H__C037D462_C403_48FF_A02F_6C36544F0833__INCLUDED_)
