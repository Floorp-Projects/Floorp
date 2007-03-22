// nsXPCDispSimple.h: Definition of the nsXPCDispSimple class
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NSXPCDISPSIMPLE_H__5502A675_46D9_4762_A7F9_1A023A052152__INCLUDED_)
#define AFX_NSXPCDISPSIMPLE_H__5502A675_46D9_4762_A7F9_1A023A052152__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// nsXPCDispSimple

class nsXPCDispSimple : 
    public ISupportErrorInfo,
    public CComObjectRoot,
    public CComCoClass<nsXPCDispSimple,&CLSID_nsXPCDispSimple>,
    public IDispatchImpl<nsIXPCDispSimple, &IID_nsIXPCDispSimple, &LIBID_IDispatchTestLib>
{
public:
    nsXPCDispSimple() : mNumber(5) {}
BEGIN_CATEGORY_MAP(nsXPCDispSimple)
    IMPLEMENTED_CATEGORY(CATID_SafeForScripting)
END_CATEGORY_MAP()
BEGIN_COM_MAP(nsXPCDispSimple)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(ISupportErrorInfo)
    COM_INTERFACE_ENTRY(nsIXPCDispSimple)
END_COM_MAP()
DECLARE_NOT_AGGREGATABLE(nsXPCDispSimple) 

DECLARE_REGISTRY_RESOURCEID(IDR_nsXPCDispSimple)
// ISupportsErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// nsIXPCDispSimple
public:
// nsIXPCDispSimple
    STDMETHOD(ClassName)(BSTR * name);
    STDMETHOD(get_Number)(LONG * result);
    STDMETHOD(put_Number)(LONG result);
    template <class T>
    static HRESULT CreateInstance(T ** result)
    {
        return CoCreateInstance(CLSID_nsXPCDispSimple, 0, CLSCTX_ALL,
                                __uuidof(T),
                                reinterpret_cast<void**>(result));
    }
private:
    long mNumber;
};

#endif // !defined(AFX_NSXPCDISPSIMPLE_H__5502A675_46D9_4762_A7F9_1A023A052152__INCLUDED_)
