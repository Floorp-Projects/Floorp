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
#include "brpref.h"
#include "pages.h"
#include "brprefid.h"
#include "prefuiid.h"
#include "isppageo.h"
#include <assert.h>

// Create a new instance of our derived class and return it.
CComDll *
DLL_ConsumerCreateInstance()
{
    return new CGeneralPrefsDll;
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

// Abstract class that supports aggregation as an inner object
class CCategory : public IUnknown {
	public:
		CCategory();

		// IUnknown methods. Doesn't delegate (explicit IUnknown)
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj) PURE;
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();

	public:
		// Abstract nested class that implements ISpecifyPropertyPageObjects
		class CSpecifyPageObjects : public ISpecifyPropertyPageObjects {
			public:
				CSpecifyPageObjects(CCategory *pBackObj, LPUNKNOWN pUnkOuter);

				// IUnknown methods. Always delegates to controlling unknown
				STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
				STDMETHODIMP_(ULONG) AddRef();
				STDMETHODIMP_(ULONG) Release();

				// ISpecifyPropertyPageObjects methods
				STDMETHODIMP GetPageObjects(CAPPAGE FAR* pPages) PURE;
			
			private:
				CCategory  *m_pBackObj;
				LPUNKNOWN	m_pUnkOuter;
		};

	private:
		ULONG	m_uRef;
};

CCategory::CCategory()
{
	m_uRef = 0;
}

STDMETHODIMP_(ULONG)
CCategory::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CCategory::Release()
{
	if (--m_uRef == 0) {
#ifdef _DEBUG
		OutputDebugString("Destroying CCategory object.\n");
#endif
		delete this;
		return 0;
	}

	return m_uRef;
}

CCategory::CSpecifyPageObjects::CSpecifyPageObjects(CCategory *pBackObj, LPUNKNOWN pUnkOuter)
{
	assert(pBackObj);

	// Don't add references to either the back pointer or the controlling unknown
	m_pBackObj = pBackObj;
	m_pUnkOuter = pUnkOuter;

	// If we're not being aggregated then pUnkOuter will be NULL. In that case
	// delegate to the object in which we're contained
	if (!m_pUnkOuter)
		m_pUnkOuter = pBackObj;
}

STDMETHODIMP
CCategory::CSpecifyPageObjects::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	// Delegate to the controlling unknown
	assert(m_pUnkOuter);
	return m_pUnkOuter->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG)
CCategory::CSpecifyPageObjects::AddRef()
{
	// Delegate to the controlling unknown
	assert(m_pUnkOuter);
	return m_pUnkOuter->AddRef();
}

STDMETHODIMP_(ULONG)
CCategory::CSpecifyPageObjects::Release()
{
	// Delegate to the controlling unknown
	assert(m_pUnkOuter);
	return m_pUnkOuter->Release();
}

/////////////////////////////////////////////////////////////////////////////
// CAppearanceCategory

class CAppearanceCategory : public CCategory {
	public:
		CAppearanceCategory(LPUNKNOWN pUnkOuter);

		// IUnknown methods. Doesn't delegate (explicit IUnknown)
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);

	private:
		class CSpecifyAppearancePageObjects : public CCategory::CSpecifyPageObjects {
			public:
				CSpecifyAppearancePageObjects(CCategory *pBackObj, LPUNKNOWN pUnkOuter);

				// ISpecifyPropertyPageObjects methods
				STDMETHODIMP GetPageObjects(CAPPAGE *pPages);
		};

		CSpecifyAppearancePageObjects m_innerObj;
};

CAppearanceCategory::CAppearanceCategory(LPUNKNOWN pUnkOuter)
	: m_innerObj(this, pUnkOuter)
{
}

STDMETHODIMP
CAppearanceCategory::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	if (riid == IID_IUnknown) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;
	
	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		*ppvObj = (LPVOID)&m_innerObj;
		m_innerObj.AddRef();
		return NOERROR;
	
	} else {
		*ppvObj = NULL;
		return ResultFromScode(E_NOINTERFACE);
	}
}

CAppearanceCategory::CSpecifyAppearancePageObjects::CSpecifyAppearancePageObjects(CCategory *pBackObj, LPUNKNOWN pUnkOuter)
	: CSpecifyPageObjects(pBackObj, pUnkOuter)
{
}

STDMETHODIMP
CAppearanceCategory::CSpecifyAppearancePageObjects::GetPageObjects(CAPPAGE *pPages)
{
	if (!pPages)
		return ResultFromScode(E_POINTER);

	pPages->cElems = 3;
	pPages->pElems = (LPPROPERTYPAGE *)CoTaskMemAlloc(pPages->cElems * sizeof(LPPROPERTYPAGE));
	if (!pPages->pElems)
		return ResultFromScode(E_OUTOFMEMORY);

	pPages->pElems[0] = new CAppearancePrefs;
	pPages->pElems[1] = new CFontsPrefs;
	pPages->pElems[2] = new CColorsPrefs;

	for (ULONG i = 0; i < pPages->cElems; i++)
		pPages->pElems[i]->AddRef();

	return NOERROR;
}

/////////////////////////////////////////////////////////////////////////////
// CBrowserCategory

class CBrowserCategory : public CCategory {
	public:
		CBrowserCategory(LPUNKNOWN pUnkOuter);

		// IUnknown methods. Doesn't delegate (explicit IUnknown)
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);

	private:
		class CSpecifyBrowserPageObjects : public CCategory::CSpecifyPageObjects {
			public:
				CSpecifyBrowserPageObjects(CCategory *pBackObj, LPUNKNOWN pUnkOuter);

				// ISpecifyPropertyPageObjects methods
				STDMETHODIMP GetPageObjects(CAPPAGE *pPages);
		};

		CSpecifyBrowserPageObjects m_innerObj;
};

CBrowserCategory::CBrowserCategory(LPUNKNOWN pUnkOuter)
	: m_innerObj(this, pUnkOuter)
{
}

STDMETHODIMP
CBrowserCategory::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	if (riid == IID_IUnknown) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;
	
	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		*ppvObj = (LPVOID)&m_innerObj;
		m_innerObj.AddRef();
		return NOERROR;
	
	} else {
		*ppvObj = NULL;
		return ResultFromScode(E_NOINTERFACE);
	}
}

CBrowserCategory::CSpecifyBrowserPageObjects::CSpecifyBrowserPageObjects(CCategory *pBackObj, LPUNKNOWN pUnkOuter)
	: CSpecifyPageObjects(pBackObj, pUnkOuter)
{
}

STDMETHODIMP
CBrowserCategory::CSpecifyBrowserPageObjects::GetPageObjects(CAPPAGE *pPages)
{
	if (!pPages)
		return ResultFromScode(E_POINTER);

	pPages->cElems = 4;
	pPages->pElems = (LPPROPERTYPAGE *)CoTaskMemAlloc(pPages->cElems * sizeof(LPPROPERTYPAGE));
	if (!pPages->pElems)
		return ResultFromScode(E_OUTOFMEMORY);

	pPages->pElems[0] = new CBrowserPrefs;
	pPages->pElems[1] = new CLanguagesPrefs;
	pPages->pElems[2] = new CApplicationsPrefs;
	pPages->pElems[3] = new CSmartBrowsingPrefs;
	
	for (ULONG i = 0; i < pPages->cElems; i++)
		pPages->pElems[i]->AddRef();
	
	return NOERROR;
}

/////////////////////////////////////////////////////////////////////////////
// CAdvancedCategory

class CAdvancedCategory : public CCategory {
	public:
		CAdvancedCategory(LPUNKNOWN pUnkOuter);

		// IUnknown methods. Doesn't delegate (explicit IUnknown)
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);

	private:
		class CSpecifyAdvancedPageObjects : public CCategory::CSpecifyPageObjects {
			public:
				CSpecifyAdvancedPageObjects(CCategory *pBackObj, LPUNKNOWN pUnkOuter);

				// ISpecifyPropertyPageObjects methods
				STDMETHODIMP GetPageObjects(CAPPAGE *pPages);
		};

		CSpecifyAdvancedPageObjects m_innerObj;
};

CAdvancedCategory::CAdvancedCategory(LPUNKNOWN pUnkOuter)
	: m_innerObj(this, pUnkOuter)
{
}

STDMETHODIMP
CAdvancedCategory::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	if (riid == IID_IUnknown) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;
	
	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		*ppvObj = (LPVOID)&m_innerObj;
		m_innerObj.AddRef();
		return NOERROR;
	
	} else {
		*ppvObj = NULL;
		return ResultFromScode(E_NOINTERFACE);
	}
}

CAdvancedCategory::CSpecifyAdvancedPageObjects::CSpecifyAdvancedPageObjects(CCategory *pBackObj, LPUNKNOWN pUnkOuter)
	: CSpecifyPageObjects(pBackObj, pUnkOuter)
{
}

STDMETHODIMP
CAdvancedCategory::CSpecifyAdvancedPageObjects::GetPageObjects(CAPPAGE *pPages)
{
	if (!pPages)
		return ResultFromScode(E_POINTER);

#ifdef MOZ_MAIL_NEWS
	pPages->cElems = 4;
#else
	pPages->cElems = 3;
#endif /* MOZ_MAIL_NEWS */
	pPages->pElems = (LPPROPERTYPAGE *)CoTaskMemAlloc(pPages->cElems * sizeof(LPPROPERTYPAGE));
	if (!pPages->pElems)
		return ResultFromScode(E_OUTOFMEMORY);

	pPages->pElems[0] = new CAdvancedPrefs;
	pPages->pElems[1] = new CCachePrefs;
	pPages->pElems[2] = new CProxiesPrefs;
#ifdef MOZ_MAIL_NEWS   
	pPages->pElems[3] = new CDiskSpacePrefs;
#endif /* MOZ_MAIL_NEWS */
	
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
	// When requesting aggregation, the outer object must explicitly ask
	// for IUnknown
	if (pUnkOuter && riid != IID_IUnknown)
		return ResultFromScode(CLASS_E_NOAGGREGATION);

#ifdef _DEBUG
	OutputDebugString("CPropertyPageFactory::CreateInstance() called.\n");
#endif
	LPUNKNOWN pCategory;
	 
	if (m_clsid == CLSID_AppearancePrefs)
		pCategory = new CAppearanceCategory(pUnkOuter);
	else if (m_clsid == CLSID_BrowserPrefs)
		pCategory = new CBrowserCategory(pUnkOuter);
	else if (m_clsid == CLSID_AdvancedPrefs)
		pCategory = new CAdvancedCategory(pUnkOuter);
	
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
// CGeneralPrefsDll implementation

HRESULT
CGeneralPrefsDll::GetClassObject(REFCLSID rClsid, REFIID riid, LPVOID *ppObj)
{
    HRESULT hres = ResultFromScode(E_UNEXPECTED);
    *ppObj = NULL;

#ifdef _DEBUG
	OutputDebugString("CGeneralPrefsDll::GetClassObject() called.\n");
#endif

    // See if we have that particular class object.
    if (rClsid == CLSID_AppearancePrefs ||
		rClsid == CLSID_BrowserPrefs ||
		rClsid == CLSID_AdvancedPrefs) {

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
CGeneralPrefsDll::GetCLSIDs()
{
    const CLSID **ppRetval = (const CLSID **)CoTaskMemAlloc(sizeof(CLSID *) * 4);

    if (ppRetval) {
        ppRetval[0] = &CLSID_AppearancePrefs;
        ppRetval[1] = &CLSID_BrowserPrefs;
        ppRetval[2] = &CLSID_AdvancedPrefs;
        ppRetval[3] = NULL;
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

