/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "dllcom.h"
#include <shellapi.h>

HINSTANCE CComDll::m_hInstance = NULL;

#ifdef DLL_DEBUG
//  Set this to your module name if you need something
//      more specific in your derived constructor.
const char *CComDll::m_pTraceID = "CComDll";
#endif

HINSTANCE CComDll::GetInstanceHandle()
{
    //  Should be non null by the time this
    //      gets called.
    DLL_ASSERT(m_hInstance);
    return(m_hInstance);
}

CComDll::CComDll()
{
    //  No COM objects created yet.
    m_ulCount = 0;

    //  Be sure to register this instance with the
    //      process ID map.
    m_pEntry = new CProcessEntry(this);
    DLL_ASSERT(m_pEntry);

    //  Add a reference upon creation.
    AddRef();
}

CComDll::~CComDll()
{
    DLL_ASSERT(m_ulCount == 0);

    //  Unregister us from this particular process.
    if(m_pEntry)    {
        delete m_pEntry;
        m_pEntry = NULL;
    }
}

HRESULT CComDll::QueryInterface(REFIID rIid, void **ppObj)  {
    HRESULT hRetval = ResultFromScode(E_NOINTERFACE);
    *ppObj = NULL;

    //  Just check to see if we should return our IUnknown
    //      implementation.
    if(IsEqualIID(rIid, IID_IUnknown))  {
        AddRef();
        *ppObj = (void *)((IUnknown *)this);
        hRetval = ResultFromScode(S_OK);
    }

    return(hRetval);
}

DWORD CComDll::AddRef()
{
    m_ulCount++;

    return(m_ulCount);
}

DWORD CComDll::Release()
{
    m_ulCount--;

    DWORD ulRetval = m_ulCount;

    if(!m_ulCount)   {
        delete this;
    }

    //  DO NOTHING BETWEEN THE DELETE AND RETURN
    //      EXCEPT RETURN STACK DATA.

    return(ulRetval);
}

// Given a CLSID, copies the string for registering the CLSID as an in-proc server
// into the specified buffer
static HRESULT
BuildRegistryCLSIDKey(REFGUID rGuid, LPSTR szStringKey)
{
	//  Convert the CLSID to a string.
	LPSTR	lpszGuid = DLL_StringFromCLSID(rGuid);

	if (!lpszGuid)
		return ResultFromScode(E_OUTOFMEMORY);

	// Get the information into key format.
#ifdef _WIN32
	wsprintf(szStringKey, "CLSID\\%s\\InprocServer32", lpszGuid);
#else
	wsprintf(szStringKey, "CLSID\\%s\\InprocServer", lpszGuid);
#endif

	// Free off the string
	CoTaskMemFree(lpszGuid);
	return NOERROR;
}

// Make registry entries for implemented CLSIDs.
// As DllRegisterServer.
HRESULT CComDll::RegisterServer()
{
    HRESULT hres = ResultFromScode(E_UNEXPECTED);

    // Determine our module (dll) path and name.
    char	szModuleName[_MAX_PATH];
    DWORD 	dwModuleNameLen;
	 
	dwModuleNameLen = GetModuleFileName(szModuleName, sizeof(szModuleName));
    if (dwModuleNameLen == 0)
		return hres;
    
	// Get the list of CLSIDs
    const CLSID **ppCLSIDs = GetCLSIDs();

    if (ppCLSIDs) {
        const CLSID **ppTraverse = ppCLSIDs;
        char 	  	  szStringKey[80];

        while (*ppTraverse)    {
            // Get the information into registry key format.
			hres = BuildRegistryCLSIDKey(**ppTraverse, szStringKey);
			if (FAILED(hres))
				break;

            // Set the registry key.
            if (RegSetValue(HKEY_CLASSES_ROOT, szStringKey, REG_SZ, szModuleName, dwModuleNameLen) != ERROR_SUCCESS) {
                hres = ResultFromScode(/*SELFREG_E_CLASS*/ E_UNEXPECTED);
                break;
            }

			// Get the next CLSID
            ppTraverse++;
        }

        // Only if we made it all the way through the loop do we consider this a success
        if (*ppTraverse == NULL)
			hres = NOERROR;

        // Free off the list of CLSIDs
		CoTaskMemFree((LPVOID)ppCLSIDs);
    }

    return hres;
}

//  Return name of module.
DWORD CComDll::GetModuleFileName(char *pOutName, size_t stOutSize, BOOL bFullPath)
{
    DWORD dwRetval = 0;

    dwRetval = ::GetModuleFileName(GetInstanceHandle(), pOutName, stOutSize);
    if(dwRetval && FALSE == bFullPath)  {
        //  Don't return the full path, split it up to the filename only.
        char aName[_MAX_FNAME];
        char aExt[_MAX_EXT];
        _splitpath(pOutName, NULL, NULL, aName, aExt);
        strcpy(pOutName, aName);
        strcat(pOutName, aExt);
        dwRetval = strlen(pOutName);
    }

    return(dwRetval);
}

//  Remove registry entries for implemented CLSIDs.
//  As DllUnregisterServer.
HRESULT CComDll::UnregisterServer()
{
    HRESULT	hres = ResultFromScode(E_UNEXPECTED);

    //  Determine our module (dll) path and name.
    char	szModuleName[_MAX_PATH];
    DWORD	dwModuleNameLen;
	 
	dwModuleNameLen = GetModuleFileName(szModuleName, sizeof(szModuleName));
    if (dwModuleNameLen == 0)
        return hres;
    
	// Get the list of CLSIDs
    const CLSID **ppCLSIDs = GetCLSIDs();

    if (ppCLSIDs) {
        const CLSID **ppTraverse = ppCLSIDs;
        char 		  szStringKey[80];

        while (*ppTraverse)    {
			LONG	lSize;
            
			// Get the information into registry key format.
			hres = BuildRegistryCLSIDKey(**ppTraverse, szStringKey);
			if (FAILED(hres))
				break;

            // Figure out if it points to us.
            // Continue if err, as it simply may not exist.
            if (RegQueryValue(HKEY_CLASSES_ROOT, szStringKey, NULL, &lSize) == ERROR_SUCCESS && lSize != 0) {
                LPSTR	lpszValue = NULL;

                //  Allocate a buffer large enough for it.
				lpszValue = (LPSTR)CoTaskMemAlloc(lSize);
                if (!lpszValue)  {
                    hres = ResultFromScode(E_OUTOFMEMORY);
                    break;
                }

                // We do err on not being able to query the value if we got the size
                if (RegQueryValue(HKEY_CLASSES_ROOT, szStringKey, lpszValue, &lSize) != ERROR_SUCCESS) {
                    hres = ResultFromScode(E_UNEXPECTED);
					CoTaskMemFree(lpszValue);
                    break;
                }

                // Only delete if the same; ignore the case
                if (lstrcmpi(lpszValue, szModuleName) == 0) {
                    // Delete the registry key.
                    if (RegDeleteKey(HKEY_CLASSES_ROOT, szStringKey) != ERROR_SUCCESS) {
                        hres = ResultFromScode(/*SELFREG_E_CLASS*/ E_UNEXPECTED);
                        break;
                    }
                }

				// Free the buffer
				CoTaskMemFree(lpszValue);
            }

			// Get the next CLSID
            ppTraverse++;
        }

        // Only if we made it through the loop do we consider this a success
        if (*ppTraverse == NULL)
            hres = NOERROR;

        // Free off the list of CLSIDs
		CoTaskMemFree((LPVOID)ppCLSIDs);
    }

    return hres;
}

//  These routines contain the only entry points into your
//      end consumer DLL.
//  You must implement the functions which it calls to
//      your own satisfaction.

//  Standard COM callback to usually obtain an IClassFactory.
STDAPI DllGetClassObject(REFCLSID rClsid, REFIID rIid, LPVOID *ppObj)
{
    HRESULT hRetval;
    *ppObj = NULL;

    //  Under 16 bits, it is imperative to seperate out each task
    //      which uses this DLL so that they don't all use
    //      the same memory heap, but instead use task specific
    //      memory.
    //  Find the abstract Dll instance for the current task.
    CComDll *pDll = CProcess::GetProcessDll();
    if(pDll)    {
        //  Do it.
        hRetval = pDll->GetClassObject(rClsid, rIid, ppObj);

        //  Take off a reference here.
        //  This may also cause the instance data to self
        //      self destruct if no objects were created
        //      in the GetClassObject call.
        pDll->Release();
        pDll = NULL;
    }
    else    {
        hRetval = ResultFromScode(E_OUTOFMEMORY);
    }

    return(hRetval);
}

//  Standard COM callback to see if the DLL can now be unloaded.
//  This is actually never called on 16 bits (via the
//      CoFreeUnusedLibraries call), but would work if that
//      mechanism is ever fixed.
STDAPI DllCanUnloadNow(void)
{
    HRESULT hRetval = ResultFromScode(S_FALSE);

    //  We tell if we can unload by simply checking if
    //      our process list of instance DLL objects is
    //      empty.  If not empty, we can not unload.
    if(CProcess::CanUnloadNow()) {
        hRetval = ResultFromScode(S_OK);
        DLL_TRACE("DllCanUnloadNow is TRUE\n");
    }

    return(hRetval);
}

STDAPI DllRegisterServer(void)
{
    HRESULT hRetval;

    //  Find the abstract Dll instance for the current task.
    CComDll *pDll = CProcess::GetProcessDll();
    if(pDll)    {
        //  Do it.
        hRetval = pDll->RegisterServer();

        //  Take off a reference here.
        //  This should cause the instance data to self
        //      self destruct if no objects were created
        //      in the GetClassObject call.
        pDll->Release();
        pDll = NULL;
    }
    else    {
        hRetval = ResultFromScode(E_OUTOFMEMORY);
    }

    return(hRetval);
}

STDAPI DllUnregisterServer(void)
{
    HRESULT hRetval;

    //  Find the abstract Dll instance for the current task.
    CComDll *pDll = CProcess::GetProcessDll();
    if(pDll)    {
        //  Do it.
        hRetval = pDll->UnregisterServer();

        //  Take off a reference here.
        //  This should cause the instance data to self
        //      self destruct if no objects were created
        //      in the GetClassObject call.
        pDll->Release();
        pDll = NULL;
    }
    else    {
        hRetval = ResultFromScode(E_OUTOFMEMORY);
    }

    return(hRetval);
}

