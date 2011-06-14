// nsXPCDispTestScriptOff.h: Definition of the nsXPCDispTestScriptOff class
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NSXPCDISPTESTSCRIPTOFF_H__EEC88DE0_F502_4893_9918_4E435DBC9815__INCLUDED_)
#define AFX_NSXPCDISPTESTSCRIPTOFF_H__EEC88DE0_F502_4893_9918_4E435DBC9815__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"       // main symbols
#include <ATLCTL.H>

/////////////////////////////////////////////////////////////////////////////
// nsXPCDispTestScriptOff

class nsXPCDispTestScriptOff : 
    public IDispatchImpl<nsIXPCDispTestScriptOff, &IID_nsIXPCDispTestScriptOff, &LIBID_IDispatchTestLib>, 
    public ISupportErrorInfo,
    public CComObjectRoot,
    public CComCoClass<nsXPCDispTestScriptOff,&CLSID_nsXPCDispTestScriptOff>,
    public IObjectSafetyImpl<nsXPCDispTestScriptOff, 0>
{
public:
    nsXPCDispTestScriptOff() {}
BEGIN_COM_MAP(nsXPCDispTestScriptOff)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(nsIXPCDispTestScriptOff)
    COM_INTERFACE_ENTRY(IObjectSafety)
    COM_INTERFACE_ENTRY(ISupportErrorInfo)
END_COM_MAP()
DECLARE_NOT_AGGREGATABLE(nsXPCDispTestScriptOff) 

DECLARE_REGISTRY_RESOURCEID(IDR_nsXPCDispTestScriptOff)
// ISupportsErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// nsIXPCDispTestScriptOff
public:
};

#endif // !defined(AFX_NSXPCDISPTESTSCRIPTOFF_H__EEC88DE0_F502_4893_9918_4E435DBC9815__INCLUDED_)
