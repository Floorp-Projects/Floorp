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

#ifndef _APITAB_H
#define _APITAB_H

#ifndef __APIAPI_H
	#include "apiapi.h"
#endif
#ifndef __NSGUIDS_H
	#include "nsguids.h"
#endif

#define	APICLASS_TABCONTROL	"TabControl"

//
// Message send to parent
//

// Notify that a tab is about the change. The message handler
// can return non-zero to prevent the user from changing
// the tab.
#ifndef TCM_TABCHANGING
#define TCM_TABCHANGING		(WM_USER + 200)
#endif

// Notify tab changed. The message handler can use
// GetCurSel to determine the new page.
#ifndef TCM_TABCHANGED
#define TCM_TABCHANGED		(WM_USER + 201)
#endif

#undef INTERFACE
#define INTERFACE ITabControl

DECLARE_INTERFACE_(ITabControl, IUnknown)
{
// Attributes
	// Returns the height of the tab control
	STDMETHOD_(int, GetHeight) (THIS) const PURE;

	// Returns the currently selected tag (0 based index)
	STDMETHOD_(int, GetCurSel) (THIS) const PURE;
	
	// Returns the number of tabs
	STDMETHOD_(int, GetItemCount) (THIS) const PURE;
	
	// Returns the HWND of the tab control
	STDMETHOD_(HWND, GetHWnd) (THIS) const PURE;

	// Returns the tab under a given point (in client coords)
	STDMETHOD_(int, TabFromPoint) (const POINT *pt) PURE;

// Operations
	// Creates a TabControl. Takes normal window styles.
	STDMETHOD(Create) (THIS_ DWORD dwStyle, const RECT *rect, HWND hParent, UINT nID) PURE;
	
	// Adds a tab with caption and image with index iImage (-1 for no image)
	STDMETHOD(AddTab) (THIS_ LPCTSTR lpszCaption, int iImage) PURE;
	
	// Removed tab nTab (0 based index)
	STDMETHOD(RemoveTab) (THIS_ int nTab) PURE;
	
	// Show/Hide tab nTab (0 based index)
	STDMETHOD(ShowTab) (THIS_ int nTab, BOOL bShow = TRUE) PURE;

	// Select tab nTab (0 based index)
	STDMETHOD(SetCurSel) (THIS_ int nTab) PURE;
	
	// Sets the bitmap for the tab images
	STDMETHOD(LoadBitmap) (THIS_ UINT id) PURE;
	
	// Set the size of each tab image
	STDMETHOD(SetSizes) (THIS_ const SIZE *size) PURE;

    // Change the text of a tab
    STDMETHOD(SetText) (THIS_ int idx, LPCSTR lpszCaption) PURE;
};

typedef ITabControl * LPTABCONTROL;

#define ApiTabControl(v,unk) APIPTRDEF(IID_ITabControl,ITabControl,v,unk)

#endif
