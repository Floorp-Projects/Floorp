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

#include "pch.h"
#include "edpref.h"
#include "pages.h"
#include "edprefid.h"
#include "prefuiid.h"
#include "isppageo.h"
#include <assert.h>
#define NUM_EDITOR_PAGES 3

// Create a new instance of our derived class and return it.
CComDll *
DLL_ConsumerCreateInstance()
{
    return new CEditorPrefsDll;
}

/////////////////////////////////////////////////////////////////////////////
// Global new/delete operators

// Override the global new/delete operators to allocate memory owned by the
// application and not by the DLL; _fmalloc calls GlobalAlloc with GMEM_SHARE
// for DLLs and that means the memory will not be freed until the DLL is
// unloaded. _fmalloc only maintains one heap for a DLL which is shared by
// all applications that use the DLL
#ifndef _WIN32
void *
operator new (size_t size)
{
	return CoTaskMemAlloc(size);
}

void
operator delete (void *lpMem)
{
	CoTaskMemFree(lpMem);
}
#endif

/////////////////////////////////////////////////////////////////////////////
// CSpecifyPropertyPageObjects

class CSpecifyPropertyPageObjects : public ISpecifyPropertyPageObjects {
	public:
		CSpecifyPropertyPageObjects();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();

	private:
		ULONG	m_uRef;
};

CSpecifyPropertyPageObjects::CSpecifyPropertyPageObjects()
{
	m_uRef = 0;
}

STDMETHODIMP
CSpecifyPropertyPageObjects::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_ISpecifyPropertyPageObjects)
		*ppvObj = (LPVOID)this;

	if (*ppvObj) {
		AddRef();
		return NOERROR;
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CSpecifyPropertyPageObjects::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CSpecifyPropertyPageObjects::Release()
{
	if (--m_uRef == 0) {
#ifdef _DEBUG
		OutputDebugString("Destroying CSpecifyPropertyPageObjects object.\n");
#endif
		delete this;
		return 0;
	}

	return m_uRef;
}

/////////////////////////////////////////////////////////////////////////////
// CEditorCategory

class CEditorCategory : public CSpecifyPropertyPageObjects {
	public:
		// ISpecifyPropertyPageObjects methods
		STDMETHODIMP GetPageObjects(CAPPAGE *pPages);
};

STDMETHODIMP
CEditorCategory::GetPageObjects(CAPPAGE *pPages)
{
	if (!pPages)
		return ResultFromScode(E_POINTER);

	pPages->cElems = NUM_EDITOR_PAGES;
	pPages->pElems = (LPPROPERTYPAGE *)CoTaskMemAlloc(pPages->cElems * sizeof(LPPROPERTYPAGE));
	if (!pPages->pElems)
		return ResultFromScode(E_OUTOFMEMORY);

	pPages->pElems[0] = new CEditorPrefs;
	pPages->pElems[1] = new CPublishPrefs;
	pPages->pElems[2] = new CEditorPrefs2;
	
	for (ULONG i = 0; i < pPages->cElems; i++)
		pPages->pElems[i]->AddRef();
	
	return NOERROR;
}

/////////////////////////////////////////////////////////////////////////////
// Class CPropertyPageFactory

// Class factory for our property pages. We use the same C++ class
// to handle all of our CLSIDs
class CPropertyPageFactory : public IClassFactory {
	public:
		CPropertyPageFactory(REFCLSID rClsid);

		// *** IUnknown methods ***
		STDMETHODIMP 			QueryInterface(REFIID, LPVOID FAR*);
		STDMETHODIMP_(ULONG) 	AddRef();
		STDMETHODIMP_(ULONG) 	Release();
	 
		// *** IClassFactory methods ***
		STDMETHODIMP 			CreateInstance(LPUNKNOWN, REFIID, LPVOID FAR*);
		STDMETHODIMP 			LockServer(BOOL bLock);

	private:
		CRefDll	m_refDll;
		ULONG   m_uRef;
		CLSID	m_clsid;
};

/////////////////////////////////////////////////////////////////////////////
// CPropertyPageFactory implementation

CPropertyPageFactory::CPropertyPageFactory(REFCLSID rClsid)
{
	m_uRef = 0;
	m_clsid = rClsid;
}

// *** IUnknown methods ***
STDMETHODIMP CPropertyPageFactory::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;
 
	if (riid == IID_IUnknown || riid == IID_IClassFactory)
		*ppvObj = (LPVOID)this;

	if (*ppvObj) {
		AddRef();
		return NOERROR;
	}

	return ResultFromScode(E_NOINTERFACE);
}


STDMETHODIMP_(ULONG) CPropertyPageFactory::AddRef()
{
	return ++m_uRef;
}


STDMETHODIMP_(ULONG) CPropertyPageFactory::Release(void)
{
	if (--m_uRef == 0) {
#ifdef _DEBUG
		OutputDebugString("Destroying CPropertyPageFactory class object.\n");
#endif
   		delete this;
		return 0;
	}

	return m_uRef;
}
 

// *** IClassFactory methods ***
STDMETHODIMP CPropertyPageFactory::CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, LPVOID FAR* ppvObj)
{
	// We do not support aggregation
	if (pUnkOuter)
		return ResultFromScode(CLASS_E_NOAGGREGATION);

#ifdef _DEBUG
	OutputDebugString("CPropertyPageFactory::CreateInstance() called.\n");
#endif
	LPSPECIFYPROPERTYPAGEOBJECTS pCategory;
	 
	if (m_clsid == CLSID_EditorPrefs)
		pCategory = new CEditorCategory;

	if (!pCategory)
		return ResultFromScode(E_OUTOFMEMORY);

	pCategory->AddRef();
	HRESULT	hRes = pCategory->QueryInterface(riid, ppvObj);
	pCategory->Release();
	return hRes;
}


STDMETHODIMP CPropertyPageFactory::LockServer(BOOL bLock)
{
	CComDll	*pDll = CProcess::GetProcessDll();
	HRESULT	 hres;

	assert(pDll);
	hres = CoLockObjectExternal(pDll, bLock, TRUE);
	pDll->Release();
	return hres;
}

/////////////////////////////////////////////////////////////////////////////
// CEditorPrefsDll implementation

HRESULT
CEditorPrefsDll::GetClassObject(REFCLSID rClsid, REFIID riid, LPVOID *ppObj)
{
    HRESULT hres = ResultFromScode(E_UNEXPECTED);
    *ppObj = NULL;

#ifdef _DEBUG
	OutputDebugString("CEditorPrefsDll::GetClassObject() called.\n");
#endif

    // See if we have that particular class object.
    if (rClsid == CLSID_EditorPrefs) {
        // Create a class object
        CPropertyPageFactory *pFactory = new CPropertyPageFactory(rClsid);

        if (!pFactory)
            return ResultFromScode(E_OUTOFMEMORY);
            
		// Get the desired interface. Note if the QueryInterface fails, the Release
		// will delete the class object
		pFactory->AddRef();
		hres = pFactory->QueryInterface(riid, ppObj);
		pFactory->Release(); 

    } else {
        hres = ResultFromScode(CLASS_E_CLASSNOTAVAILABLE);
    }

    return hres;
}

// Return array of implemented CLSIDs by this DLL. Allocated
// memory freed by caller.
const CLSID **
CEditorPrefsDll::GetCLSIDs()
{
    const CLSID **ppRetval = (const CLSID **)CoTaskMemAlloc(sizeof(CLSID *) * 2);

    if (ppRetval) {
        ppRetval[0] = &CLSID_EditorPrefs;
        ppRetval[1] = NULL;
    }

    return ppRetval;
}

#ifdef _WIN32
BOOL WINAPI
DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            // The DLL is being loaded for the first time by a given process
			CComDll::m_hInstance = hInstance;
            break;

        case DLL_PROCESS_DETACH:
            // The DLL is being unloaded by a given process
            break;

        case DLL_THREAD_ATTACH:
            // A thread is being created in a process that has already loaded
            // this DLL
            break;

        case DLL_THREAD_DETACH:
            // A thread is exiting cleanly in a process that has already
            // loaded this DLL
            break;
    }

    return TRUE;
}
#else
extern "C" int CALLBACK
LibMain(HINSTANCE hInstance, WORD wDataSeg, WORD cbHeapSize, LPSTR)
{
    CComDll::m_hInstance = hInstance;
    return TRUE;
}
#endif

