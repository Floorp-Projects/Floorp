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

#ifndef __PPAGEEX_H_
#define __PPAGEEX_H_

/////////////////////////////////////////////////////////////////////////////
// IPropertyPageEx interface

#ifdef __cplusplus
interface IPropertyPageEx;
#else
typedef interface IPropertyPageEx IPropertyPageEx;
#endif

typedef IPropertyPageEx FAR* LPPROPERTYPAGEEX;

#undef  INTERFACE
#define INTERFACE IPropertyPageEx

// IPropertyPageEx adds method PageChanging() which notifies a page that it
// is about to lose the activation because another page is being activated.
// The property page should validate the information the user has typed, and
// return S_OK to allow the page change
//
// Return S_FALSE to prevent the page from losing the activation. Display
// a dialog box to explain the problem, and set the focus to the control that
// didn't validate
//
// NOTE: The use of IPropertyPageEx is optional, and you can use IPropertyPage
// if you like
DECLARE_INTERFACE_(IPropertyPageEx, IPropertyPage)
{
	// IUnknown methods
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// IPropertyPage methods
	STDMETHOD(SetPageSite)(THIS_ LPPROPERTYPAGESITE pPageSite) PURE;
	STDMETHOD(Activate)(THIS_ HWND hwndParent, LPCRECT lprc, BOOL bModal) PURE;
	STDMETHOD(Deactivate)(THIS) PURE;
	STDMETHOD(GetPageInfo)(THIS_ LPPROPPAGEINFO pPageInfo) PURE;
	STDMETHOD(SetObjects)(THIS_ ULONG cObjects, LPUNKNOWN FAR* ppunk) PURE;
	STDMETHOD(Show)(THIS_ UINT nCmdShow) PURE;
	STDMETHOD(Move)(THIS_ LPCRECT prect) PURE;
	STDMETHOD(IsPageDirty)(THIS) PURE;
	STDMETHOD(Apply)(THIS) PURE;
	STDMETHOD(Help)(THIS_ LPCOLESTR lpszHelpDir) PURE;
	STDMETHOD(TranslateAccelerator)(THIS_ LPMSG lpMsg) PURE;

	// IPropertyPageEx methods
	STDMETHOD(PageChanging)(THIS_ LPPROPERTYPAGE pNewPage) PURE;
};

#endif /* __PPAGEEX_H_ */

