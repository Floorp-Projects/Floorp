// nsXPCDispSimple.cpp : Implementation of CXPCIDispatchTestApp and DLL registration.

#include "stdafx.h"
#include "XPCIDispatchTest.h"
#include "nsXPCDispSimple.h"

/////////////////////////////////////////////////////////////////////////////
//

STDMETHODIMP nsXPCDispSimple::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = 
    {
        &IID_nsIXPCDispSimple,
    };

    for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
    {
        if (InlineIsEqualGUID(*arr[i],riid))
            return S_OK;
    }
    return S_FALSE;
}

STDMETHODIMP nsXPCDispSimple::ClassName(BSTR * name)
{
    if (name == NULL)
        return E_POINTER;
    CComBSTR x("nsXPCDispSimple");
    *name = x.Detach();
    return S_OK;
}
STDMETHODIMP nsXPCDispSimple::get_Number(LONG * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = mNumber;
    return S_OK;
}
STDMETHODIMP nsXPCDispSimple::put_Number(LONG result)
{
    mNumber = result;
    return S_OK;
}
