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
#include <assert.h>
#include "prefui.h"
#include "prefpriv.h"
#include "framedlg.h"

HINSTANCE	g_hInstance;

/////////////////////////////////////////////////////////////////////////////
// Global new/delete operators

void *
operator new (size_t size)
{
#ifdef _WIN32
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
#else
	return (void *)(void NEAR *)LocalAlloc(LPTR, size);
#endif
}

void
operator delete (void *lpMem)
{
#ifdef _WIN32
	HeapFree(GetProcessHeap(), 0, lpMem);
#else
	LocalFree((HLOCAL)OFFSETOF(lpMem));
#endif
}

/////////////////////////////////////////////////////////////////////////////
// Helper routines

#ifndef _WIN32
static LPVOID
CoTaskMemAlloc(ULONG cb)
{
	LPMALLOC	pMalloc;
	
	if (SUCCEEDED(CoGetMalloc(MEMCTX_TASK, &pMalloc))) {
		LPVOID	pv;
		 
		pv = pMalloc->Alloc(cb);
		pMalloc->Release();
		return pv;
	}

	return NULL;
}

static void
CoTaskMemFree(LPVOID pv)
{
	if (pv) {
		LPMALLOC	pMalloc;
	
		if (SUCCEEDED(CoGetMalloc(MEMCTX_TASK, &pMalloc))) {
			pMalloc->Free(pv);
			pMalloc->Release();
		}
	}
}
#endif

// Converts an OLE string (UNICODE) to an ANSI string. The caller
// must use CoTaskMemFree to free the memory
static LPSTR
AllocTaskAnsiString(LPOLESTR lpszString)
{
	LPSTR	lpszResult;
	UINT	nBytes;

	if (lpszString == NULL)
		return NULL;

#ifdef _WIN32
	nBytes = (wcslen(lpszString) + 1) * 2;
#else
	nBytes = lstrlen(lpszString) + 1;  // Win 16 doesn't use any UNICODE
#endif
	lpszResult = (LPSTR)CoTaskMemAlloc(nBytes);

	if (lpszResult) {
#ifdef _WIN32
		WideCharToMultiByte(CP_ACP, 0, lpszString, -1, lpszResult, nBytes, NULL, NULL);
#else
		lstrcpy(lpszResult, lpszString);
#endif
	}

	return lpszResult;
}

/////////////////////////////////////////////////////////////////////////////
// CPropertyCategories implementation

// Destructor. Release all the interface pointers and free the
// counted arrays
CPropertyCategories::~CPropertyCategories()
{
	delete []m_pCategories;
}

// Create the individual property pages for each of the
// property categories
HRESULT
CPropertyCategories::Initialize(ULONG 						  nCategories,
								LPSPECIFYPROPERTYPAGEOBJECTS *lplpProviders,
								LPPROPERTYPAGESITE			  pSite)
{
	m_nCategories = nCategories;
	m_pCategories = new CPropertyCategory[nCategories];
	if (!m_pCategories)
		return ResultFromScode(E_OUTOFMEMORY);

	for (ULONG i = 0; i < nCategories; i++) {
		HRESULT	hres = m_pCategories[i].CreatePages(lplpProviders[i], pSite);

		if (FAILED(hres))
			return hres;
	}

	return NOERROR;
}

/////////////////////////////////////////////////////////////////////////////
// CPropertyCategory implementation

// Releases the reference to the interface for the property page provider,
// and destroys each of the property pages objects
CPropertyCategory::~CPropertyCategory()
{
	delete []m_pPages;
	if (m_pProvider)
		m_pProvider->Release();
}

// Gets the list of property page obejcts and initializes each of the pages
HRESULT
CPropertyCategory::CreatePages(LPSPECIFYPROPERTYPAGEOBJECTS pProvider,
							   LPPROPERTYPAGESITE	  		pSite)
{
	CAPPAGE		pages;
	HRESULT		hres;
	
	// Keep the pointer to the property page provider. This is passed
	// as the data object for each property page
	m_pProvider = pProvider;
	m_pProvider->AddRef();
	
	// Get the list of property pages objects
	hres = pProvider->GetPageObjects(&pages);
#ifdef _DEBUG
	if (FAILED(hres)) {
		OutputDebugString("Call to GetPageObjects() failed for property page provider\n");
	}
#endif

	if (SUCCEEDED(hres)) {
		m_pPages = new CPropertyPage[pages.cElems];
		if (!m_pPages) {
			for (ULONG i = 0; i < pages.cElems; i++)
				pages.pElems[i]->Release();
			CoTaskMemFree(pages.pElems);
			return ResultFromScode(E_OUTOFMEMORY);
		}

		// Initialize the array of property pages
		m_nPages = 0;
		for (ULONG i = 0; i < pages.cElems; i++) {
			LPPROPERTYPAGE	pPage = pages.pElems[m_nPages];
			 
			// Set the back pointer
			m_pPages[m_nPages].m_pCategory = this;
			
			// Initialize the property page
			hres = m_pPages[m_nPages].Initialize(pPage, pSite);

			// We're all done with the page
			pPage->Release();
			m_nPages++;
		}

		CoTaskMemFree(pages.pElems);
	}

	return hres;
}

/////////////////////////////////////////////////////////////////////////////
// CPropertyPage implementation

// Releases the reference to the interface for the property page,
// set its page site to NULL, and frees any memory
CPropertyPage::~CPropertyPage()
{
	if (m_pPage) {
		m_pPage->SetPageSite(NULL);
		m_pPage->Release();
	}

	CoTaskMemFree((void *)m_lpszTitle);
	CoTaskMemFree((void *)m_lpszDescription);
	CoTaskMemFree((void *)m_lpszHelpTopic);
}

// Initializes the property page by setting the page site
// and getting the page info
HRESULT
CPropertyPage::Initialize(LPPROPERTYPAGE pPage, LPPROPERTYPAGESITE pSite)
{
	HRESULT	hres;

	// Hold a reference to the property page
	m_pPage = pPage;
	m_pPage->AddRef();

	// First thing we do is set the page site. This initializes
	// the property page
	hres = m_pPage->SetPageSite(pSite);
#ifdef _DEBUG
	if (FAILED(hres))
		OutputDebugString("Call to SetPageSite() failed\n");
#endif

	// Get information about the property page
	PROPPAGEINFO	pageInfo;

	pageInfo.cb = sizeof(pageInfo);
	hres = m_pPage->GetPageInfo(&pageInfo);
	if (FAILED(hres)) {
#ifdef _DEBUG
		OutputDebugString("Call to GetPageInfo() failed\n");
#endif
		return hres;
	}

	// The page must have a title
	if (!pageInfo.pszTitle) {
#ifdef _DEBUG
		OutputDebugString("Property page has NULL 'pszTitle'\n");
#endif
		hres = ResultFromScode(E_UNEXPECTED);
	}

	// Save the size for later
	m_size = pageInfo.size;

	// OLE strings are UNICODE and we need ANSI strings
    // XXX - use CString...
	m_lpszTitle = AllocTaskAnsiString(pageInfo.pszTitle);
	if (!m_lpszTitle) {
		hres = ResultFromScode(E_OUTOFMEMORY);
        goto done;
    }

	if (pageInfo.pszDocString) {
		m_lpszDescription = AllocTaskAnsiString(pageInfo.pszDocString);
		if (!m_lpszDescription) {
			hres = ResultFromScode(E_OUTOFMEMORY);
            goto done;
        }
	}

    if (pageInfo.pszHelpFile) {
        // NetHelp help topic is a string so it's stored in pszHelpFile
        // instead of using dwHelpContext
        m_lpszHelpTopic = AllocTaskAnsiString(pageInfo.pszHelpFile);
		if (!m_lpszHelpTopic) {
			hres = ResultFromScode(E_OUTOFMEMORY);
            goto done;
        }
    }

  done:
	// Free the memory
	CoTaskMemFree(pageInfo.pszTitle);
	CoTaskMemFree(pageInfo.pszDocString);
	CoTaskMemFree(pageInfo.pszHelpFile);
	return hres;
}

//
//  FUNCTION: NS_CreatePropertyFrame()
//
//  PURPOSE:  Called to create a property frame dialog. This function
//			  doesn't return until the dialog is closed.
//
//  PARAMETERS:
//    hwndOwner - parent window of property sheet dialog
//    x - horizontal position for dialog box, relative to hwndOwner
//    y - vertical position for dialog box, relative to hwndOwner
//    lpszCaption - dialog box caption
//    nCategories - number of property page providers in lplpProviders
//    lplpProviders - array of property page providers
//    nInitial - index of category to be initially displayed
//
STDAPI
NS_CreatePropertyFrame(HWND     			   		 hwndOwner,
					   int					   		 x,
					   int					   		 y,
					   LPCSTR   			   		 lpszCaption,
					   ULONG    			   		 nCategories,
					   LPSPECIFYPROPERTYPAGEOBJECTS *lplpProviders,
					   ULONG				   		 nInitial,
                       NETHELPFUNC                   lpfnNetHelp)
{
	HRESULT	hres;

	assert(nCategories > 0);
	if (nCategories == 0)
		return ResultFromScode(S_FALSE);

	// Create the property frame dialog
	CPropertyFrameDialog	dialog(hwndOwner, x, y, lpszCaption, lpfnNetHelp);

	// Initialize the property frame dialog
	hres = dialog.CreatePages(nCategories, lplpProviders, nInitial);
	if (FAILED(hres)) {
#ifdef _DEBUG
		OutputDebugString("CPropertyFrameDialog::CreatePages failed\n");
#endif
		return hres;
	}

	// Display the property frame as a modal dialog
	dialog.DoModal();
	return NOERROR;
}

#ifdef _WIN32
// Main entry point for the DLL. By defining the entry point ourselves
// and not using the Visual C++ runtime library entry point we can avoid
// linking in the runtime initialization code
BOOL WINAPI
DllEntryPoint(HINSTANCE hInstance, DWORD dwReason, LPVOID lpvReserved)
{
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            // The DLL is being loaded for the first time by a given process
			g_hInstance = hInstance;
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
    g_hInstance = hInstance;
    InitTreeViewControl();
    return TRUE;
}
#endif
