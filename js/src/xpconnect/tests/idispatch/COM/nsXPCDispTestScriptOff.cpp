// nsXPCDispTestScriptOff.cpp : Implementation of CXPCIDispatchTestApp and DLL registration.

#include "stdafx.h"
#include "XPCIDispatchTest.h"
#include "nsXPCDispTestScriptOff.h"

/////////////////////////////////////////////////////////////////////////////
//

STDMETHODIMP nsXPCDispTestScriptOff::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = 
    {
        &IID_nsIXPCDispTestScriptOff,
    };

    for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
    {
        if (InlineIsEqualGUID(*arr[i],riid))
            return S_OK;
    }
    return S_FALSE;
}
