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
#include "cppageex.h"
#include "prefuiid.h"

// The code in this DLL was written to use ANSI internally, and
// will not work correctly if compiled with UNICODE defined
#if defined(_WIN32) && defined(UNICODE)
#error ERROR: Must be compiled for ANSI and not for UNICODE!
#endif

#if defined(_WIN32) && defined(OLE2ANSI)
#error ERROR: Must not be compiled with OLE2ANSI!
#endif

/////////////////////////////////////////////////////////////////////////////
// Helper routines

// Convert an ANSI string to an OLE string (UNICODE string)
static LPOLESTR NEAR
AllocTaskOleString(LPCSTR lpszString)
{
	LPOLESTR	lpszResult;
	UINT		nLen;

	if (lpszString == NULL)
		return NULL;

	// Convert from ANSI to UNICODE
	nLen = lstrlen(lpszString) + 1;
	lpszResult = (LPOLESTR)CoTaskMemAlloc(nLen * sizeof(OLECHAR));
	if (lpszResult)	{
#ifdef _WIN32
		MultiByteToWideChar(CP_ACP, 0, lpszString, -1, lpszResult, nLen);
#else
		lstrcpy(lpszResult, lpszString);  // Win 16 doesn't use any UNICODE
#endif
	}

	return lpszResult;
}

// Returns TRUE if we're at the end of the tab list
static BOOL NEAR
AtEndOfTabList(HWND hwndDlg)
{
	HWND	hCtl;
	BOOL	bShift;
	 
	// See whether we should check for the end of the tab list or
	// the shift-tab list
	bShift = GetKeyState(VK_SHIFT) < 0;

	// Get the control that currently has focus
	hCtl = GetFocus();
	assert(IsChild(hwndDlg, hCtl));

	// Get the first-level child for the control. This matters for controls
	// like a combo box which have child windows
	while (GetParent(hCtl) != hwndDlg)
		hCtl = GetParent(hCtl);

	// Look for another child window that's enabled and wants TAB keys
	do {
		hCtl = bShift ? GetPrevSibling(hCtl) : GetNextSibling(hCtl);

		if (hCtl == NULL)
			return TRUE;  // no more child windows
	} while ((GetWindowStyle(hCtl) & (WS_DISABLED | WS_TABSTOP)) != WS_TABSTOP);

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Free store operators

LPVOID
CPropertyPageEx::operator new(size_t size)
{
#ifdef _WIN32
	return HeapAlloc(GetProcessHeap(), 0, size);
#else
	LPMALLOC	pMalloc;

	if (SUCCEEDED(CoGetMalloc(MEMCTX_TASK, &pMalloc))) {
		LPVOID	lpv = pMalloc->Alloc(size);

		pMalloc->Release();
		return lpv;
	}

	return NULL;
#endif
}

void
CPropertyPageEx::operator delete(LPVOID lpv)
{
#ifdef _WIN32
	HeapFree(GetProcessHeap(), 0, lpv);
#else
	LPMALLOC	pMalloc;

	if (SUCCEEDED(CoGetMalloc(MEMCTX_TASK, &pMalloc))) {
		pMalloc->Free(lpv);
		pMalloc->Release();
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////
// CPropertyPageEx implementation

// Constructor for CPropertPageEx
CPropertyPageEx::CPropertyPageEx(HINSTANCE hInstance, UINT nTemplateID, LPCSTR lpszHelpTopic)
	: CDialog(hInstance, nTemplateID)
{
	m_pPageSite = NULL;
	m_pObject = NULL;
	m_uRef = 0;
	m_bHasBeenActivated = FALSE;
    m_lpszHelpTopic = lpszHelpTopic;
}

// Destructor for CPropertPageEx
CPropertyPageEx::~CPropertyPageEx()
{
	// Make sure we were aksed to release any references to interface pointers we
	// were holding
	assert(!m_pPageSite);
	assert(!m_pObject);
}

// Override WM_INITDIALOG and set WS_GROUP and WS_TABSTOP for the page. This
// ensures that a tab key in the property frame will pass focus to the
// property page
BOOL
CPropertyPageEx::InitDialog()
{
	DWORD dwStyle = GetWindowStyle(m_hwndDlg);

	dwStyle |= WS_GROUP | WS_TABSTOP;
	SetWindowLong(m_hwndDlg, GWL_STYLE, (LONG)dwStyle);
	
	// Call the base class so we transfer data to the dialog
	CDialog::InitDialog();

	// We don't want to automatically take the focus
	return FALSE;
}

HRESULT
CPropertyPageEx::GetPageSize(SIZE &size)
{
	// Figure out how big the dialog box is by creating it, getting
	// its size, and then destroying it
	if (Create(GetDesktopWindow())) {
		RECT	rect;

		GetWindowRect(m_hwndDlg, &rect);
		size.cx = rect.right - rect.left;
		size.cy = rect.bottom - rect.top;
		DestroyWindow();
		return NOERROR;
	}

	return ResultFromScode(E_FAIL);
}

// *** IUnknown methods ***

// Returns a pointer to a specified interface on an object to which a client
// currently holds an interface pointer
STDMETHODIMP CPropertyPageEx::QueryInterface(REFIID riid, LPVOID FAR *ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_IPropertyPage || riid == IID_IPropertyPageEx)
		*ppvObj = (LPVOID)this;

	if (*ppvObj) {
		AddRef(); // we must call IUnknown::AddRef() on the pointer we return
		return NOERROR;
	}

	return ResultFromScode(E_NOINTERFACE);
}

// Increments the reference count for an interface on an object
STDMETHODIMP_(ULONG) CPropertyPageEx::AddRef()
{
	return ++m_uRef;
}

// Decrements the reference count for the calling interface on a object. If the
// reference count on the object falls to 0, the object is freed from memory
STDMETHODIMP_(ULONG) CPropertyPageEx::Release()
{
	if (--m_uRef == 0) {
#ifdef _DEBUG
		OutputDebugString("Destroying CPropertyPageEx object.\n");
#endif
		delete this;
		return 0;
	}

	return m_uRef;
}
		
// *** IPropertyPage methods ***

// Initializes a property page and provides the page with a pointer to the
// IPropertyPageSite interface through which the property page communicates
// with the property frame
STDMETHODIMP CPropertyPageEx::SetPageSite(LPPROPERTYPAGESITE pPageSite)
{
	HRESULT	hres = NOERROR;

	if (pPageSite) {
		assert(!m_pPageSite);
		if (m_pPageSite)
			return ResultFromScode(E_UNEXPECTED);

		m_pPageSite = pPageSite;
		m_pPageSite->AddRef();
		hres = GetPageSize(m_size);

	} else {
		assert(m_pPageSite);
		if (m_pPageSite) {
			m_pPageSite->Release();
			m_pPageSite = NULL;
		}
	}

	return hres;
}
		
// Creates the dialog box window for the property page
STDMETHODIMP CPropertyPageEx::Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal)
{
	assert(hwndParent);
	assert(lprc);
	if (!lprc)
		return ResultFromScode(E_POINTER);

	// We shouldn't get two calls to Activate without a Deactivate in between
	assert(!m_hwndDlg);
	if (m_hwndDlg)
		return ResultFromScode(E_UNEXPECTED);

	// Create the dialog window
	if (!Create(hwndParent)) {
		return ResultFromScode(E_OUTOFMEMORY);
	}

	assert(!IsWindowVisible(m_hwndDlg));

	// Display the dialog where requested
	SetWindowPos(m_hwndDlg, NULL, lprc->left, lprc->top, lprc->right -
		lprc->left, lprc->bottom - lprc->top, SWP_NOACTIVATE | SWP_NOZORDER |
		SWP_SHOWWINDOW);

	// Remember that we've been activated
	m_bHasBeenActivated = TRUE;

	return NOERROR;
}

// Destroys the window created with Activate()
STDMETHODIMP CPropertyPageEx::Deactivate()
{
	assert(m_hwndDlg);
	if (!m_hwndDlg)
		return ResultFromScode(E_UNEXPECTED);

	// Do data transfer. More than likely PageChanging() was called and we
	// transfered the data there as well, but to be safe do it again here
	DoTransfer(TRUE);

	DestroyWindow();
	return NOERROR;
}

// Returns information about the property page
STDMETHODIMP CPropertyPageEx::GetPageInfo(LPPROPPAGEINFO pPageInfo)
{
	assert(pPageInfo);
	if (!pPageInfo)
		return ResultFromScode(E_POINTER);

	pPageInfo->pszTitle = NULL;
	pPageInfo->size = m_size;
	pPageInfo->pszDocString = NULL;
	pPageInfo->pszHelpFile = NULL;
	pPageInfo->dwHelpContext = 0;

    // NetHelp expects the help topic to be a string so we can't use
    // dwHelpContext. Misuse pszHelpFile instead...
    if (m_lpszHelpTopic) {
        pPageInfo->pszHelpFile = AllocTaskOleString(m_lpszHelpTopic);
		
        if (!pPageInfo->pszHelpFile)
			return ResultFromScode(E_OUTOFMEMORY);
    }

	// The title and short description are stored as a string resource
	// (same resource ID as the dialog)
	char	szTitle[256];
	int		nLen;

	// Note that with LoadString(), nMaxBuf specifies the size of the buffer
	// in bytes (ANSI version) or characters (Unicode version). We're using
	// ANSI
	nLen = LoadString(m_hInstance, m_nTemplateID, szTitle, sizeof(szTitle));
	assert(nLen > 0);
	if (nLen > 0) {
		// We expect a string of this format: "title\ndescription"
		LPSTR	lpszDescription = strchr(szTitle, '\n');

		if (lpszDescription) {
			*lpszDescription = '\0';  // convert separator to null-terminator
			lpszDescription++;		  // skip separator
		}

		// Convert from ANSI to UNICODE (all OLE strings are UNICODE)
		pPageInfo->pszTitle = AllocTaskOleString(szTitle);
		if (!pPageInfo->pszTitle)
			return ResultFromScode(E_OUTOFMEMORY);

		if (lpszDescription) {
			pPageInfo->pszDocString = AllocTaskOleString(lpszDescription);

			if (!pPageInfo->pszDocString)
				return ResultFromScode(E_OUTOFMEMORY);
		}
	}

	return NOERROR;
}

// Provides the property page with an array of IUnknown pointers for objects
// associated with this property page. We only expect to be passed one object
STDMETHODIMP CPropertyPageEx::SetObjects(ULONG cObjects, LPUNKNOWN FAR* ppunk)
{
	if (cObjects == 0) {
		assert(m_pObject);
		if (!m_pObject)
			return ResultFromScode(E_UNEXPECTED);

		// Release the object
		m_pObject->Release();
		m_pObject = NULL;

	} else {
		// We only expect one object
		assert(cObjects == 1);
		if (cObjects > 1)
			return ResultFromScode(E_INVALIDARG);

		// We don't expect to be given a new object without being asked to
		// release the previous one
		if (m_pObject) {
			assert(FALSE);
			return ResultFromScode(E_UNEXPECTED);
		}

		m_pObject = ppunk[0];
		if (!m_pObject)
			return ResultFromScode(E_POINTER);

		// Hold a reference to the cached pointer
		m_pObject->AddRef();
	}

	return NOERROR;
}

// Makes the property page dialog box visible or invisible
STDMETHODIMP CPropertyPageEx::Show(UINT nCmdShow)
{
	assert(m_hwndDlg);
	if (!m_hwndDlg)
		return ResultFromScode(E_UNEXPECTED);

	ShowWindow(m_hwndDlg, nCmdShow);

	if (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNORMAL)
		SetFocus(m_hwndDlg);

	return NOERROR;
}

// Positions and resizes the property page dialog box within the frame
STDMETHODIMP CPropertyPageEx::Move(LPCRECT prect)
{
	assert(m_hwndDlg);
	if (!m_hwndDlg)
		return ResultFromScode(E_UNEXPECTED);

	assert(prect);
	if (!prect)
		return ResultFromScode(E_POINTER);

	SetWindowPos(m_hwndDlg, NULL, prect->left, prect->top, prect->right -
		prect->left, prect->bottom - prect->top, SWP_NOACTIVATE | SWP_NOZORDER);
	return NOERROR;
}

// Indicates whether the property page has changed since activated or since
// the most recent call to Apply()
STDMETHODIMP CPropertyPageEx::IsPageDirty()
{
	// We don't know whether the page is dirty or not. However, the property page
	// protocol says that the Apply() method should only be called if the page is
	// dirty. Assume that if it's been activated then it's dirty. Derived classes
	// that actually track the dirty state should override this member function
	return ResultFromScode(m_bHasBeenActivated ? S_OK : S_FALSE);
}

// Applies current property page values to underlying objects specified through
// SetObjects
STDMETHODIMP CPropertyPageEx::Apply()
{
	assert(m_pObject);
	assert(m_bHasBeenActivated);

	// Do data transfer
	if (m_hwndDlg)
		DoTransfer(TRUE);

	// Ask the page to apply the changes
	ApplyChanges();

	return NOERROR;
}

// Invoked in response to end-user request for help. If we fail this method
// then the property frame will use the information we provided in GetPageInfo()
STDMETHODIMP CPropertyPageEx::Help(LPCOLESTR lpszHelpDir)
{
	return ResultFromScode(E_NOTIMPL);
}

// Instructs the property page to translate the keystroke defined
// in lpMsg
//
// There are a bunch of things we need to accomplish here:
// - handle tabbing into the dialog
// - handle tabbing back out of the dialog
// - pass Esc and Enter keys to the property frame
// - pass mnemonics that we don't handle to the property frame
STDMETHODIMP CPropertyPageEx::TranslateAccelerator(LPMSG lpMsg)
{
	assert(m_hwndDlg);
	assert(m_pPageSite);

	if (!lpMsg)
		return ResultFromScode(E_POINTER);

	// We shouldn't be asked to translate messages that aren't intented
	// for us or one of our child windows with the exception of a TAB
	// where the property frame is giving us an opportunity to take the
	// focus
	assert((lpMsg->hwnd == m_hwndDlg) ||
		   (IsChild(m_hwndDlg, lpMsg->hwnd)) ||
		   (lpMsg->message == WM_KEYDOWN && lpMsg->wParam == VK_TAB));

	// We need to do some special pre-processing for keyboard events
	if (lpMsg->message == WM_KEYDOWN) {
		UINT	nCode;
		
		// First see if the control wants to handle it
		nCode = FORWARD_WM_GETDLGCODE(lpMsg->hwnd, lpMsg, SendMessage);
		if (nCode & (DLGC_WANTALLKEYS | DLGC_WANTMESSAGE))
			return ResultFromScode(S_FALSE);
					 
		switch (lpMsg->wParam) {
			case VK_TAB:
				if (IsChild(m_hwndDlg, GetFocus())) {
					// We already have focus. See if we should pass focus back to
					// the frame
					if (AtEndOfTabList(m_hwndDlg)) {
						// Give the property frame a chance to take the focus
						if (m_pPageSite->TranslateAccelerator(lpMsg) == S_OK)
							return NOERROR;
					}

				} else {
					HWND	hwnd;

					// The property frame wants us to take the focus. If the Shift key is
					// down give focus to the last control in the tab order
					if (GetKeyState(VK_SHIFT) < 0) {
						// Get the last control with a WS_TABSTOP
						hwnd = GetNextDlgTabItem(m_hwndDlg, GetFirstChild(m_hwndDlg), TRUE);
					} else {
						// Get the first control with a WS_TABSTOP
						hwnd = GetNextDlgTabItem(m_hwndDlg, NULL, FALSE);
					}
					FORWARD_WM_NEXTDLGCTL(m_hwndDlg, hwnd, TRUE, SendMessage);
					return NOERROR;
				}
				break;

			case VK_ESCAPE:
			case VK_CANCEL:
				// We don't have a Cancel button, but the property frame does so
				// let it handle it
				return m_pPageSite->TranslateAccelerator(lpMsg);

			case VK_EXECUTE:
			case VK_RETURN:
				// If the button with focus is a defpushbutton then drop thru and
				// let the dialog manager handle it; otherwise pass it to the property
				// frame so it can handle it
				nCode = (UINT)SendMessage(GetFocus(), WM_GETDLGCODE, 0, 0L);
				if (nCode & DLGC_DEFPUSHBUTTON)
					break;

				// Not a defpushbutton so let the property frame handle it
				return m_pPageSite->TranslateAccelerator(lpMsg);

			default:
				break;
		}
	}

	// Let the dialog manager handle mnemonics, tab, and cursor keys
	//
	// We do need to work around a peculiarity of the dialog manager:
	// IsDialogMessage() will return TRUE for all WM_SYSCHAR messages whether
	// or not there is a matching mnemonic, because it also calls DefWindowProc()
	// to check for menu bar mnemonics
	BOOL	bHandled;
	HWND	hFocus;

	if (lpMsg->message == WM_SYSCHAR) {
		// Remember the focus window so we can compare after calling IsDialogMessage()
		hFocus = GetFocus();
	}

	// Call the dialog manager
	bHandled = IsDialogMessage(m_hwndDlg, lpMsg);
	 
	if (lpMsg->message == WM_SYSCHAR) {
		// See if the focus window changed. If it did then IsDialogMessage()
		// found a control that matched the mnemonic. Don't do this if it was the
		// system menu though
		if (lpMsg->wParam != VK_SPACE && GetFocus() == hFocus) {
			// Didn't change the focus so there was not a matching mnemonic. Let
			// the property frame handle it
			return m_pPageSite->TranslateAccelerator(lpMsg);
		}
	}
	
	return bHandled ? NOERROR : ResultFromScode(S_FALSE);
}

// *** IPropertyPageEx methods ***

// Destroys the window created with Activate()
STDMETHODIMP CPropertyPageEx::PageChanging(LPPROPERTYPAGE pNewPage)
{
	// Do data transfer. If the validation fails return S_FALSE to prevent
	// the page from changing
	return DoTransfer(TRUE) ? NOERROR : ResultFromScode(S_FALSE);
}


