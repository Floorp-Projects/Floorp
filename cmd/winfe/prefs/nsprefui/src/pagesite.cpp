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
#include "prefpriv.h"
#include "prefui.h"
#include "framedlg.h"
#include "pagesite.h"

/////////////////////////////////////////////////////////////////////////////
// CPropertyPageSite implementation

// Constructor
CPropertyPageSite::CPropertyPageSite(CPropertyFrameDialog *pFrame)
{
	m_pFrame = pFrame;
}

// *** IUnknown methods ***

// Returns a pointer to a specified interface on an object to which a client
// currently holds an interface pointer
STDMETHODIMP CPropertyPageSite::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;
 
	if (riid == IID_IUnknown || riid == IID_IPropertyPageSite)
		*ppvObj = (LPVOID)this;

	if (*ppvObj) {
		AddRef();
		return NOERROR;
	}

	return ResultFromScode(E_NOINTERFACE);
}

// Increments the reference count for an interface on an object
STDMETHODIMP_(ULONG) CPropertyPageSite::AddRef()
{
	return ++m_uRef;
}

// Decrements the reference count for the calling interface on a object. If the
// reference count on the object falls to 0, the object is freed from memory
STDMETHODIMP_(ULONG) CPropertyPageSite::Release()
{
	if (--m_uRef == 0) {
#ifdef _DEBUG
		OutputDebugString("Destroying property page site.\n");
#endif
   		delete this;
		return 0;
	}

	return m_uRef;
}

// *** IPropertyPageSite methods ***

// Informs the frame that the property page managed by this site has changed its
// state, that is, one or more property values have been changed in the page
STDMETHODIMP CPropertyPageSite::OnStatusChange(DWORD flags)
{
	return NOERROR;
}

// Returns the locale identifier (an LCID) that a property page can use to adjust
// itself to the language in use and other country-specific settings
STDMETHODIMP CPropertyPageSite::GetLocaleID(LCID FAR* pLocalID)
{
	return ResultFromScode(E_NOTIMPL);
}

// Returns an IUnknown pointer to the object representing the entire property
// frame dialog box that contains all the pages
STDMETHODIMP CPropertyPageSite::GetPageContainer(LPUNKNOWN FAR* ppUnk)
{
	// There are no "container" interfaces currently defined for this role
	return ResultFromScode(E_NOTIMPL);
}

// Called by the property page to give the property frame a chance
// to translate accelerators
STDMETHODIMP CPropertyPageSite::TranslateAccelerator(LPMSG lpMsg)
{
	assert(m_pFrame);
	return m_pFrame ? m_pFrame->TranslateAccelerator(lpMsg) : ResultFromScode(S_FALSE);
}

