// nsXPCDispTestNoIDispatch.h: Definition of the nsXPCDispTestNoIDispatch class
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NSXPCDISPTESTNOIDISPATCH_H__E4B74F67_BA6B_4654_8674_E60E487129F7__INCLUDED_)
#define AFX_NSXPCDISPTESTNOIDISPATCH_H__E4B74F67_BA6B_4654_8674_E60E487129F7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// nsXPCDispTestNoIDispatch

class nsXPCDispTestNoIDispatch :
    public nsIXPCDispTestNoIDispatch,
    public ISupportErrorInfo,
    public CComObjectRoot,
    public CComCoClass<nsXPCDispTestNoIDispatch,&CLSID_nsXPCDispTestNoIDispatch>
{
public:
    nsXPCDispTestNoIDispatch() {}
BEGIN_COM_MAP(nsXPCDispTestNoIDispatch)
    COM_INTERFACE_ENTRY(nsIXPCDispTestNoIDispatch)
    COM_INTERFACE_ENTRY(ISupportErrorInfo)
END_COM_MAP()
//DECLARE_NOT_AGGREGATABLE(nsXPCDispTestNoIDispatch) 
// Remove the comment from the line above if you don't want your object to 
// support aggregation. 

DECLARE_REGISTRY_RESOURCEID(IDR_nsXPCDispTestNoIDispatch)
// ISupportsErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// nsIXPCDispTestNoIDispatch
public:
};

#endif // !defined(AFX_NSXPCDISPTESTNOIDISPATCH_H__E4B74F67_BA6B_4654_8674_E60E487129F7__INCLUDED_)
