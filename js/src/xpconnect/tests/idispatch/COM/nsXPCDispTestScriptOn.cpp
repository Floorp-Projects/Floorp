// nsXPCDispTestScriptOn.cpp : Implementation of CXPCIDispatchTestApp and DLL registration.

#include "stdafx.h"
#include "XPCIDispatchTest.h"
#include "nsXPCDispTestScriptOn.h"

/////////////////////////////////////////////////////////////////////////////
//

STDMETHODIMP nsXPCDispTestScriptOn::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = 
    {
        &IID_nsIXPCDispTestScriptOn,
    };

    for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
    {
        if (InlineIsEqualGUID(*arr[i],riid))
            return S_OK;
    }
    return S_FALSE;
}

nsXPCDispTestScriptOn::nsXPCDispTestScriptOn()
{
    m_dwCurrentSafety = INTERFACESAFE_FOR_UNTRUSTED_CALLER |
        INTERFACESAFE_FOR_UNTRUSTED_DATA;
}