// XPCIDispatchTest.cpp : Implementation of DLL Exports.


// Note: Proxy/Stub Information
//      To build a separate proxy/stub DLL, 
//      run nmake -f IDispatchTestps.mk in the project directory.

#include "stdafx.h"
#include "resource.h"
#include <initguid.h>
#include "XPCIDispatchTest.h"

#include "XPCIDispatchTest_i.c"
#include "nsXPCDispTestMethods.h"
#include "nsXPCDispSimple.h"
#include "nsXPCDispTestNoIDispatch.h"
#include "nsXPCDispTestProperties.h"
#include "nsXPCDispTestArrays.h"
#include "nsXPCDispTestScriptOn.h"
#include "nsXPCDispTestScriptOff.h"
#include "nsXPCDispTestWrappedJS.h"

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
OBJECT_ENTRY(CLSID_nsXPCDispTestMethods, nsXPCDispTestMethods)
OBJECT_ENTRY(CLSID_nsXPCDispSimple, nsXPCDispSimple)
OBJECT_ENTRY(CLSID_nsXPCDispTestNoIDispatch, nsXPCDispTestNoIDispatch)
OBJECT_ENTRY(CLSID_nsXPCDispTestProperties, nsXPCDispTestProperties)
OBJECT_ENTRY(CLSID_nsXPCDispTestArrays, nsXPCDispTestArrays)
OBJECT_ENTRY(CLSID_nsXPCDispTestScriptOn, nsXPCDispTestScriptOn)
OBJECT_ENTRY(CLSID_nsXPCDispTestScriptOff, nsXPCDispTestScriptOff)
OBJECT_ENTRY(CLSID_nsXPCDispTestWrappedJS, nsXPCDispTestWrappedJS)
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        _Module.Init(ObjectMap, hInstance, &LIBID_IDispatchTestLib);
        DisableThreadLibraryCalls(hInstance);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
        _Module.Term();
    return TRUE;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
    return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
    // registers object, typelib and all interfaces in typelib
    return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
    return _Module.UnregisterServer(TRUE);
}


#include "nsXPCDispTestWrappedJS.h"
