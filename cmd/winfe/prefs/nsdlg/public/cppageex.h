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

#ifndef __CPPAGEEX_H_
#define __CPPAGEEX_H_

#include "cdialog.h"
#include "ippageex.h"

class CPropertyPageEx : public IPropertyPageEx, protected CDialog {
	public:
		CPropertyPageEx(HINSTANCE hInstance, UINT nTemplateID, LPCSTR lpszHelpTopic = NULL);
		virtual ~CPropertyPageEx();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();

		// IPropertyPage methods. CPropertyPageEx has no way to know if
		// the page is dirty. IsPageDirty() returns TRUE if the page has
		// ever been activated and FALSE otherwise. This assures that the
		// Apply() method will be called by the preferences UI framework
		STDMETHODIMP SetPageSite(LPPROPERTYPAGESITE pPageSite);
		STDMETHODIMP Activate(HWND hwndParent, LPCRECT lprc, BOOL bModal);
		STDMETHODIMP Deactivate();
		STDMETHODIMP GetPageInfo(LPPROPPAGEINFO pPageInfo);
		STDMETHODIMP SetObjects(ULONG cObjects, LPUNKNOWN FAR* ppunk);
		STDMETHODIMP Show(UINT nCmdShow);
		STDMETHODIMP Move(LPCRECT prect);
		STDMETHODIMP IsPageDirty();
		STDMETHODIMP Apply();
		STDMETHODIMP Help(LPCOLESTR lpszHelpDir);
		STDMETHODIMP TranslateAccelerator(LPMSG lpMsg);

		// IPropertyPageEx methods
		STDMETHODIMP PageChanging(LPPROPERTYPAGE pNewPage);

		// Free store operators
		LPVOID	operator new(size_t);
		void	operator delete(LPVOID);

	protected:
		// Dialog data transfer. Override this routine and transfer data to/from
		// the dialog box. Called from these member functions:
		// - CDialog::InitDialog()
		// - PageChanging()
		// - Apply()
		//
		// When transfering data from the dialog box, return FALSE if validation
		// is unsuccessful. PageChanging() will return S_FALSE which will prevent
		// the page from losing the activation
		virtual BOOL	DoTransfer(BOOL bSaveAndValidate) = 0;

		// Called from Apply(). Override this to apply any changes the user has
		// made. Return TRUE if successful and FALSE otherwise
		virtual BOOL	ApplyChanges() = 0;

		// Called by SetPageSite() to get the size of the page. This routine creates
		// the dialog, gets its window size, and then destroys the dialog
		//
		// If all your property pages are the same size you can override this routine,
		// compute the size once, and then return the cached size
		virtual HRESULT	GetPageSize(SIZE &);
	
	protected:
		// Event processing
		virtual BOOL	InitDialog();

	protected:
		LPPROPERTYPAGESITE	m_pPageSite;
		LPUNKNOWN			m_pObject;
		ULONG				m_uRef;
		SIZE				m_size;
		BOOL				m_bHasBeenActivated;  // TRUE if activated at least one time
        LPCSTR              m_lpszHelpTopic;  // name of the help topic for this page
};

#endif /* __CPPAGEEX_H_ */

