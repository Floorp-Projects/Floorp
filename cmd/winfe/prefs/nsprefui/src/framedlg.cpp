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
#include "ippageex.h"
#include "prefuiid.h"
#include "pagesite.h"
#include "grayramp.h"
#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// Helper routines

#ifndef _WIN32
#define GET_WM_COMMAND_ID(wp, lp)               ((int)(wp))
#define GET_WM_COMMAND_HWND(wp, lp)             ((HWND)LOWORD(lp))
#define GET_WM_COMMAND_CMD(wp, lp)              ((UINT)HIWORD(lp))
#endif

static void
ResizeChildBy(HWND hdlg, UINT nID, int dx, int dy)
{
	RECT	rect;
	HWND	hwnd;

	// Get the current rectangle
	hwnd = GetDlgItem(hdlg, nID);
	assert(hwnd != NULL);
	GetWindowRect(hwnd, &rect);

	// Convert from screen coords to parent coordinates
	MapWindowPoints(NULL, hdlg, (LPPOINT)&rect, 2);

	// Adjust the right and bottomn edges of the rectangle accordingly
	rect.right += dx;
	rect.bottom += dy;

	// Set the new position
	MoveWindow(hwnd, rect.left, rect.top, rect.right - rect.left,
		rect.bottom - rect.top, FALSE);
}

static void
MoveChildBy(HWND hdlg, UINT nID, int dx, int dy)
{
	RECT	rect;
	HWND	hwnd;

	// Get the current rectangle
	hwnd = GetDlgItem(hdlg, nID);
	assert(hwnd != NULL);
	GetWindowRect(hwnd, &rect);

	// Convert from screen coords to parent coordinates
	MapWindowPoints(NULL, hdlg, (LPPOINT)&rect, 2);

	// Offset by the delta
	OffsetRect(&rect, dx, dy);

	// Set the new position
	MoveWindow(hwnd, rect.left, rect.top, rect.right - rect.left,
		rect.bottom - rect.top, FALSE);
}

// Returns the window handle of the top-level parent/owner of the
// specified window
static HWND
GetTopLevelWindow(HWND hwnd)
{
	HWND hwndTop;

	do {
		hwndTop = hwnd;
		hwnd = GetParent(hwnd);
	} while (hwnd != NULL);

	return hwndTop;
}

// Returns the window handle of the first-level child for the
// specified window. As a precondition, this routine expects hchild
// to be a child of hdlg
static HWND
GetFirstLevelChild(HWND hdlg, HWND hchild)
{
	// Make sure hchild is a child of hdlg
	if (hchild == hdlg || !IsChild(hdlg, hchild))
		return NULL;

	while (GetParent(hchild) != hdlg)
		hchild = GetParent(hchild);

	return hchild;
}

// Returns TRUE if the specified window is a dialog and FALSE otherwise
static BOOL
IsDialogWindow(HWND hwnd)
{
	char	szClassName[256];
	
	// See if it's a dialog by checking its class name. The string
	// we're comparing against is the string name for WC_DIALOG
	if (GetClassName(hwnd, szClassName, sizeof(szClassName)) > 0) {
		if (lstrcmp(szClassName, "#32770") == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

// Returns TRUE if the control is a push button and FALSE otherwise
static BOOL inline
IsPushButton(HWND hwnd)
{
	return hwnd && (SendMessage(hwnd, WM_GETDLGCODE, 0, 0) &
		(DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON));
}

// Finds the current default push button and removes the default
// button style
static void
RemoveDefaultButton(HWND hwndDlg)
{
	// We don't know which button is currently the default push button.
	// I suppose we could try and track it, but instead let's do like
	// everyone else does and scan through all the controls and remove
	// the default button style from any button that has it
	for (HWND hwnd = GetFirstChild(hwndDlg); hwnd; hwnd = GetNextSibling(hwnd)) {
		UINT	nCode = (UINT)SendMessage(hwnd, WM_GETDLGCODE, 0, 0);

		if (nCode & DLGC_DEFPUSHBUTTON)
			Button_SetStyle(hwnd, BS_PUSHBUTTON, TRUE);

		else if (IsDialogWindow(hwnd))
			RemoveDefaultButton(hwnd);
	}
}

// Returns the bounding rectangle of the message area in the coordinate
// space of the dialog
static void
GetMessageAreaRect(HWND hwndDlg, RECT &r)
{
	GetWindowRect(GetDlgItem(hwndDlg, IDC_MESSAGE), &r);
	MapWindowPoints(NULL, hwndDlg, (LPPOINT)&r, 2);
}

// Callback function for the property frame dialog
BOOL CALLBACK
#ifndef _WIN32
__export
#endif
DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CPropertyFrameDialog	*pDialog;
	PAINTSTRUCT				 paint;
	LPNMHDR					 lpnmh;
	
	if (uMsg == WM_INITDIALOG) {
		// The lParam is the "this" pointer. Save it away
		SetWindowLong(hwndDlg, DWL_USER, (LONG)lParam);
		return ((CPropertyFrameDialog *)lParam)->InitDialog(hwndDlg);
	}

	// Get the "this" pointer
	pDialog = (CPropertyFrameDialog*)GetWindowLong(hwndDlg, DWL_USER);

	switch (uMsg) {
		case WM_COMMAND:
			return pDialog->OnCommand(GET_WM_COMMAND_ID(wParam, lParam),
				GET_WM_COMMAND_HWND(wParam, lParam),
				GET_WM_COMMAND_CMD(wParam, lParam));

		case WM_PAINT:
			BeginPaint(hwndDlg, &paint);
			pDialog->OnPaint(paint.hdc);
			EndPaint(hwndDlg, &paint);
			return TRUE;

		case WM_NOTIFY:
			lpnmh = (LPNMHDR)lParam;

			if (lpnmh->idFrom == IDC_TREE && lpnmh->code == TVN_SELCHANGED)
				pDialog->TreeViewSelChanged((LPNM_TREEVIEW)lpnmh);
			break;

		default:
			break;
	}

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CPropertyFrameDialog implementation

// Constructor
CPropertyFrameDialog::CPropertyFrameDialog(HWND		   hwndOwner,
										   int		   x,
										   int		   y,
										   LPCSTR	   lpszCaption,
                                           NETHELPFUNC lpfnNetHelp)
{
	m_hdlg = NULL;
	m_hwndOwner = hwndOwner;
	m_x = x;
	m_y = y;
	m_lpszCaption = lpszCaption;
    m_lpfnNetHelp = lpfnNetHelp;
	m_nInitialCategory = 0;
	m_pCurPage = NULL;
	m_hBoldFont = NULL;
	m_lpGradient = NULL;
	m_hBrush = NULL;
}

CPropertyFrameDialog::~CPropertyFrameDialog()
{
	if (m_hBoldFont)
		DeleteObject(m_hBoldFont);

#ifdef _WIN32
	if (m_lpGradient)
		HeapFree(GetProcessHeap(), 0, m_lpGradient);
#else
	assert(!m_lpGradient);
#endif
	
	if (m_hBrush)
		DeleteBrush(m_hBrush);
}

HRESULT
CPropertyFrameDialog::CreatePages(ULONG 				  		nCategories,
								  LPSPECIFYPROPERTYPAGEOBJECTS *lplpProviders,
								  ULONG					  		nInitialCategory)
{
	CPropertyPageSite	*pSite;
	HRESULT				 hres;
	 
	assert(nInitialCategory < nCategories);
	if (nInitialCategory >= nCategories)
		return ResultFromScode(E_INVALIDARG);

	// Allocate a page site. Rather than create one page site per property
	// page, we create one that they all share
	pSite = new CPropertyPageSite(this);
	if (!pSite)
		return ResultFromScode(E_OUTOFMEMORY);

	pSite->AddRef();
	hres = m_categories.Initialize(nCategories, lplpProviders, pSite);
	pSite->Release();

	// Remember this for when we're displayed
	m_nInitialCategory = nInitialCategory;
	return hres;
}

BOOL
CPropertyFrameDialog::InitDialog(HWND hdlg)
{
	HFONT	hFont;
	LOGFONT	font;
	HWND	hwnd;

	// Save away the window handle for the dialog
	m_hdlg = hdlg;

	// Get the bold font we will be using. It's the same font as the
	// dialog box uses, except it's bold
	hFont = GetWindowFont(m_hdlg);
	GetObject(hFont, sizeof(font), &font);
	font.lfWeight = FW_BOLD;
	m_hBoldFont = CreateFontIndirect(&font);
	assert(m_hBoldFont);

	// Add each page to the tree view
	FillTreeView();

	// Size to fit the property frame dialog based on the size of the pages.
	// This routine also calculates the rectangle where the property page
	// will be displayed
	SizeToFit();

	// See how many colors the device has
	HDC	hdc = GetDC(hdlg);

	if (GetDeviceCaps(hdc, BITSPIXEL) == 8) {
		// Use the "medium gray" SVGA color
		m_hBrush = CreateSolidBrush(PALETTERGB(160,160,164));

	} else if (GetDeviceCaps(hdc, BITSPIXEL) > 8) {
#ifdef _WIN32
		RECT	rect;

		// Create a gray scale ramp
		GetMessageAreaRect(m_hdlg, rect);
		m_lpGradient = MakeGrayScaleRamp(rect.right - rect.left, rect.bottom - rect.top);
#else
		m_hBrush = CreateSolidBrush(RGB(160,160,160));
#endif
	}

	ReleaseDC(hdlg, hdc);

	// Make sure the first category is open
	hwnd = GetDlgItem(m_hdlg, IDC_TREE);
	TreeView_Expand(hwnd, m_categories[0][0].m_hitem, TVE_EXPAND);

	// Select the first item of the category that we should initially
	// display. Make sure the category is expanded as well
	TreeView_SelectItem(hwnd, m_categories[m_nInitialCategory][0].m_hitem);
	TreeView_Expand(hwnd, m_categories[m_nInitialCategory][0].m_hitem, TVE_EXPAND);

	// Give the focus to the property page
	SetFocus(GetPropertyPageWindow());
	return FALSE;
}

// Returns the dimensions of the widest and highest page
SIZE
CPropertyFrameDialog::GetMaxPageSize()
{
	SIZE	size = {0, 0};

	for (ULONG i = 0; i < m_categories.m_nCategories; i++) {
		for (ULONG j = 0; j < m_categories[i].m_nPages; j++) {
			CPropertyPage	&page = m_categories[i][j];

			if (page.m_size.cx > size.cx)
				size.cx = page.m_size.cx;
			if (page.m_size.cy > size.cy)
				size.cy = page.m_size.cy;
		}
	}

	return size;
}

// Size to fit the property frame dialog based on the widest/highest
// property page
void
CPropertyFrameDialog::SizeToFit()
{
	SIZE	curSize, maxSize, deltaSize;
	RECT   	clientRect, treeRect, msgRect, rect;

	// Figure out the widest and highest page sizes
	maxSize = GetMaxPageSize();

	// Compute how much room there currently is for the property page.
	//
	// We compute the vertical space as the distance between the bottom of
	// the tree view and the bottom of the message area, and we compute the
	// horizontal space as the distance between the right edge of the frame
	// dialog and the right edge of the text view
	GetClientRect(m_hdlg, &clientRect);
	GetWindowRect(GetDlgItem(m_hdlg, IDC_TREE), &treeRect);
	MapWindowPoints(NULL, m_hdlg, (LPPOINT)&treeRect, 2);
	GetMessageAreaRect(m_hdlg, msgRect);

	curSize.cx = clientRect.right - treeRect.right;
#ifndef _WIN32
	// Ctl3d draws part of its border outside of the window
	curSize.cx--;
#endif
	curSize.cy = treeRect.bottom - msgRect.bottom;

	// Determine how much we need to resize the window
	deltaSize.cx = maxSize.cx - curSize.cx;
	deltaSize.cy = maxSize.cy - curSize.cy;

	// Resize the dialog
	GetWindowRect(m_hdlg, &rect);
	SetWindowPos(m_hdlg, NULL, 0, 0, rect.right - rect.left + deltaSize.cx,
		rect.bottom - rect.top + deltaSize.cy, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);

	// Resize the right edge of the message area
	ResizeChildBy(m_hdlg, IDC_MESSAGE, deltaSize.cx, 0);

	// Resize the height of the tree view
	ResizeChildBy(m_hdlg, IDC_TREE, 0, deltaSize.cy);

	// And reposition the buttons
	MoveChildBy(m_hdlg, IDOK, deltaSize.cx, deltaSize.cy);
	MoveChildBy(m_hdlg, IDCANCEL, deltaSize.cx, deltaSize.cy);
	MoveChildBy(m_hdlg, IDHELP, deltaSize.cx, deltaSize.cy);

	// Update the property page rectangle
	m_pageRect.left = treeRect.right;
#ifndef _WIN32
	// Ctl3d draws part of its border outside of the window
	m_pageRect.left++;
#endif
	m_pageRect.top = msgRect.bottom;
	m_pageRect.right = m_pageRect.left + maxSize.cx;
	m_pageRect.bottom = m_pageRect.top + maxSize.cy;
}

void
CPropertyFrameDialog::FillTreeView()
{
	HWND	hwnd;

	// Get the HWND for the tree view
	hwnd = GetDlgItem(m_hdlg, IDC_TREE);
	assert(hwnd != NULL);

	for (ULONG i = 0; i < m_categories.m_nCategories; i++) {
		for (ULONG j = 0; j < m_categories[i].m_nPages; j++) {
			CPropertyPage	&page = m_categories[i][j];
			TV_INSERTSTRUCT	 tvis;

			// Add it to the outliner
			memset(&tvis.item, 0, sizeof(tvis.item));

			if (j == 0) {
				// Insert this item at the root of the tree view
				tvis.hParent = NULL;
				tvis.item.cChildren = m_categories[i].m_nPages > 1 ? 1 : 0;
			} else {
				// Make this item a child of the category
				tvis.hParent = m_categories[i][0].m_hitem;
			}
			
			tvis.hInsertAfter = TVI_LAST;
			tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
			tvis.item.pszText = (LPSTR)page.m_lpszTitle;
			tvis.item.lParam = (LPARAM)&page;
			page.m_hitem = TreeView_InsertItem(hwnd, &tvis);
		}
	}
}

BOOL
CPropertyFrameDialog::OnCommand(int id, HWND hwndCtl, UINT notifyCode)
{
	ULONG	i, j;

	switch (id) {
		case IDOK:
			m_nModalResult = TRUE;
			m_bKeepGoing = FALSE;

			// Ask each property page that's dirty to apply its changes
			for (i = 0; i < m_categories.m_nCategories; i++) {
				for (j = 0; j < m_categories[i].m_nPages; j++) {
					CPropertyPage	&page = m_categories[i][j];

					if (page.m_pPage->IsPageDirty() == S_OK) {
						// We must call the SetObjects method first. Note that we have
						// aleady done this for the current page so shouldn't do it again
						if (&page != m_pCurPage)
							page.m_pPage->SetObjects(1, (LPUNKNOWN *)&page.m_pCategory->m_pProvider);
						page.m_pPage->Apply();
						if (&page != m_pCurPage)
							page.m_pPage->SetObjects(0, NULL);
					}
				}
			}
			return TRUE;

		case IDCANCEL:
			m_nModalResult = FALSE;
			m_bKeepGoing = FALSE;
			return TRUE;

        case IDHELP:
            if (m_lpfnNetHelp) {
                if (m_pCurPage && m_pCurPage->m_lpszHelpTopic)
                    m_lpfnNetHelp(m_pCurPage->m_lpszHelpTopic);
            }
			return TRUE;
	}

	return FALSE;
}

void
CPropertyFrameDialog::OnPaint(HDC hdc)
{
	RECT	rect;

	// Paint the gray background for the message area
	GetMessageAreaRect(m_hdlg, rect);
	
	if (m_lpGradient)
#ifdef _WIN32
		StretchDIBits(hdc, rect.left, rect.top, m_lpGradient->biWidth,
			rect.bottom - rect.top, 0, 0, m_lpGradient->biWidth, rect.bottom - rect.top,
			(LPSTR)m_lpGradient + m_lpGradient->biSize, (LPBITMAPINFO)m_lpGradient,
			DIB_RGB_COLORS, SRCCOPY);
#else
		assert(FALSE);
#endif
	else if (m_hBrush)
		FillRect(hdc, &rect, m_hBrush);
	else
		FillRect(hdc, &rect, GetStockObject(GRAY_BRUSH));

	// Display the title and description
	if (m_pCurPage) {
		int		nOldBkMode = SetBkMode(hdc, TRANSPARENT);
		HFONT	hOldFont;

		// Leave a margin of 7 pixels on the left and right
		rect.left += 7;
		rect.right -= 7;

		// Use our bold font for the title
		hOldFont = SelectFont(hdc, m_hBoldFont);

		if (m_pCurPage->m_lpszTitle) {
			DrawText(hdc, m_pCurPage->m_lpszTitle, -1, &rect,
				DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
		}

		// Use the dialog font for the description
		SelectFont(hdc, GetWindowFont(m_hdlg));

		if (m_pCurPage->m_lpszDescription) {
			DrawText(hdc, m_pCurPage->m_lpszDescription, -1, &rect,
				DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
		}

		// Restore the HDC
		SetBkMode(hdc, nOldBkMode);
		SelectFont(hdc, hOldFont);
	}
}

// Responds to the tree-view TVN_SELCHANGED notification message informing the
// tree-view control's parent window that the selection has changed from one
// item to another
void
CPropertyFrameDialog::TreeViewSelChanged(LPNM_TREEVIEW pnmtv)
{
	CPropertyPage  *pNewPage;

	if (m_pCurPage)
		assert((CPropertyPage *)pnmtv->itemOld.lParam == m_pCurPage);

	pNewPage = (CPropertyPage *)pnmtv->itemNew.lParam;
	assert(pNewPage);

	if (pNewPage != m_pCurPage) {
		HRESULT	hres;

		// See if the current page wants to be queried about the page change
		if (m_pCurPage) {
			LPPROPERTYPAGEEX	lpPageEx;

			// See if it supports the IPropertyPageEx interface
			hres = m_pCurPage->m_pPage->QueryInterface(IID_IPropertyPageEx, (void **)&lpPageEx);
			if (SUCCEEDED(hres)) {
				BOOL	bCanChange = lpPageEx->PageChanging(pNewPage->m_pPage) == S_OK;
	
				lpPageEx->Release();
				if (!bCanChange)
					return;  // current page is preventing the activation
			}
		}
		
		// Activate the new page. First we need to provide it with the
		// preferences provider object
		assert(pNewPage->m_pCategory);
		hres = pNewPage->m_pPage->SetObjects(1, (LPUNKNOWN *)&pNewPage->m_pCategory->m_pProvider);
		assert(SUCCEEDED(hres));
	
		// Now activate the page
		hres = pNewPage->m_pPage->Activate(m_hdlg, &m_pageRect, TRUE);
		assert(SUCCEEDED(hres));
	
		if (SUCCEEDED(hres)) {
			// Deactivate the old page
			if (m_pCurPage) {
				// Ask the old page to release its pointer
				hres = m_pCurPage->m_pPage->SetObjects(0, NULL);
				assert(SUCCEEDED(hres));
		
				// Deactivate the page
				hres = m_pCurPage->m_pPage->Deactivate();
				assert(SUCCEEDED(hres));
			}
	
			// Remember the new page
			m_pCurPage = pNewPage;
	
			// Update the message are
			HDC	hdc = GetDC(m_hdlg);
	
			OnPaint(hdc);
			ReleaseDC(m_hdlg, hdc);
	
		} else {
			// We failed to activate the new page. Revoke the interface pointer
			// we passed it
			pNewPage->m_pPage->SetObjects(0, NULL);
		}
	}
}

// Called by the property page site to give the property frame a chance
// to translate accelerators
HRESULT
CPropertyFrameDialog::TranslateAccelerator(LPMSG lpMsg)
{
	return IsDialogMessage(m_hdlg, lpMsg) ? ResultFromScode(S_OK) : ResultFromScode(S_FALSE);
}

void
CPropertyFrameDialog::CheckDefPushButton(HWND hwndOldFocus, HWND hwndNewFocus)
{
	// Do nothing if the same control
	if (hwndNewFocus == hwndOldFocus)
		return;
	
	// Make sure the focus still belongs to us
	if (!IsChild(m_hdlg, hwndNewFocus))
		return;

	// If either the old or new focus windows is a pushbutton then remove
	// the default button style from the current default button
	if (IsPushButton(hwndOldFocus) || IsPushButton(hwndNewFocus))
		RemoveDefaultButton(m_hdlg);

	// If moving to a button make it be the default button
	if (IsPushButton(hwndNewFocus))
		Button_SetStyle(hwndNewFocus, BS_DEFPUSHBUTTON, TRUE);

	else {
		// Make the original default button be the default button
		LRESULT	lResult = (int)SendMessage(m_hdlg, DM_GETDEFID, 0, 0);
		int		nID = HIWORD(lResult) == DC_HASDEFID ? LOWORD(lResult) : IDOK;
		HWND	hwnd = GetDlgItem(m_hdlg, nID);

		if (IsWindowEnabled(hwnd))
			Button_SetStyle(hwnd, BS_DEFPUSHBUTTON, TRUE);
	}
}

HWND
CPropertyFrameDialog::GetPropertyPageWindow()
{
	for (HWND hwnd = GetFirstChild(m_hdlg); hwnd; hwnd = GetNextSibling(hwnd)) {
		if (IsDialogWindow(hwnd))
			return hwnd;
	}

	assert(FALSE);
	return NULL;
}

static BOOL
IsHelpKey(LPMSG lpMsg)
{
	// return TRUE only for non-repeat F1 keydowns.
	return lpMsg->message == WM_KEYDOWN &&
		   lpMsg->wParam == VK_F1 &&
		   !(HIWORD(lpMsg->lParam) & KF_REPEAT) &&
		   GetKeyState(VK_SHIFT) >= 0 &&
		   GetKeyState(VK_CONTROL) >= 0 &&
		   GetKeyState(VK_MENU) >= 0;
}

// Returns TRUE if either the property page or the property frame dialog translated
// the message and FALSE otherwise. If this routine returns TRUE then the message 
// must not be translated/dispatched
BOOL
CPropertyFrameDialog::PreTranslateMessage(LPMSG lpMsg)
{
	// This routine does two things:
	// - manage the default push button
	// - where appropriate forward WM_SYSCHAR messages (keyboard mnemonics) to the
	//   property page for translating
	//
	// Here's how it works. Both the property page and the property frame may be
	// involved in translating the message. Who gets first crack at it depends on
	// the message hwnd. There are two scearios:
	//
	// 1. the message hwnd is the property page or one of its children
	// 2. the message hwnd is not the property page or one of its children
	//
	// In scenario #1 the property page gets first crack at translating the message,
	// and if it didn't translate it then the property frame gets to translate it
	//
	// In scenario #2, if the message is a Tab/Shift-Tab and the property page is
	// the control that would receive focus, then it gets first crack at the message.
	// Otherwise, the property frame gets first crack at translating the message. If
	// the message is a WM_SYSCHAR and the property frame didn't translate it then the
	// message is forwarded to the property page for forwarding
	if (lpMsg->hwnd != m_hdlg && !IsChild(m_hdlg, lpMsg->hwnd))
		return FALSE;

	// Translate accelerators. The only accelerator we handle is for bringing up the help
	// window
	if (IsHelpKey(lpMsg)) {
		FORWARD_WM_COMMAND(m_hdlg, IDHELP, 0, 0, SendMessage);
		return TRUE;
	}

	HWND	hwndFocus = GetFocus();  // remember this for later
	BOOL	bHandled = FALSE;

	// See whether the message is targeted for the property page
	HWND	hwnd = GetFirstLevelChild(m_hdlg, lpMsg->hwnd);

	if (hwnd && IsDialogWindow(hwnd)) {
		// Scenario #1. Let the property page have the first chance at
		// translating the accelerator
		//
		// Note that under certain circumstances the property page will
		// call the property page site and ask it to translate the message
		assert(m_pCurPage);
		bHandled = m_pCurPage->m_pPage->TranslateAccelerator(lpMsg) == S_OK;

		if (!bHandled) {
			// Give the property frame a chance to translate the message
			bHandled = TranslateAccelerator(lpMsg) == S_OK;
		}

	} else {
		// If the message is a WM_KEYDOWN of a Tab character and the property page
		// is going to become the focus window, then give it a chance to handle the
		// Tab. This gives it an opportunity to handle Shift-Tab by selecting the
		// last control in the tab order
		if (lpMsg->message == WM_KEYDOWN && lpMsg->wParam == VK_TAB) {
			HWND hNewFocus = GetNextDlgTabItem(m_hdlg, hwndFocus, GetKeyState(VK_SHIFT) < 0);
	
			if (IsDialogWindow(hNewFocus)) {
				// Give the property page a chance to translate the message
				assert(m_pCurPage);
				bHandled = m_pCurPage->m_pPage->TranslateAccelerator(lpMsg) == S_OK;
			}
		}

		// Have the property frame translate the message
		if (!bHandled) {
			bHandled = TranslateAccelerator(lpMsg) == S_OK;

			// If the message is a WM_SYSCHAR and the property frame didn't handle it,
			// we need to give the property page a chance.
			//
			// Unfortunately IsDialogMessage() returns TRUE for all WM_SYSCHAR messages
			// regardless of whether there was a matching mnemonic (it does this because
			// it calls DefWindowProc() so it can check for matching menu bar mnemonics and
			// it has no idea whether DefWindowProc() found a match)
			if (lpMsg->message == WM_SYSCHAR) {
				// See if the focus window has changed. If it has, then IsDialogMessage()
				// found a control that matched the mnemonic. Don't do this if it was for
				// the system menu though
				if (lpMsg->wParam != VK_SPACE && GetFocus() == hwndFocus) {
					// Didn't change the focus so there was not a matching mnemonic. Let
					// the property page translate it
					//
					// Temporarily change the hwnd to be the first child of the property page
					HWND	hwndPage = GetPropertyPageWindow();

					if (hwndPage) {
						HWND	hwndSave = lpMsg->hwnd;

						lpMsg->hwnd = GetFirstChild(hwndPage);
						bHandled = m_pCurPage->m_pPage->TranslateAccelerator(lpMsg) == S_OK;
						lpMsg->hwnd = hwndSave;
					}
				}
			}
		}
	}

	// Check the default push button
	if (hwndFocus)
		CheckDefPushButton(hwndFocus, GetFocus());

	return bHandled;
}

int
CPropertyFrameDialog::RunModalLoop()
{
	MSG	msg;
	
	if (m_hdlg == NULL)
		return -1;

	assert(IsWindowVisible(m_hdlg));

	for (;;) {
		// Check if we should send an idle message
		if (m_hwndOwner && !::PeekMessage(&msg, NULL, NULL, NULL, PM_NOREMOVE)) {
			// Send WM_ENTERIDLE to the owner
			SendMessage(m_hwndOwner, WM_ENTERIDLE, MSGF_DIALOGBOX, (LPARAM)m_hdlg);
		}

		// Pump the messages
		do {
			if (!GetMessage(&msg, NULL, NULL, NULL)) {
				// Repost the WM_QUIT message
				PostQuitMessage(msg.wParam);
				return -1;
			}

			// See if the message should be handled at all
			if (!CallMsgFilter(&msg, MSGF_DIALOGBOX)) {
				// Give the property page and the property frame dialog a chance to
				// translate the message
				if (!PreTranslateMessage(&msg)) {
					// No one wanted it so dispatch the message
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			// Check if we should exit the modal loop
			if (!m_bKeepGoing)
				return m_nModalResult;

			// If there's another message waiting then keep pumping; otherwise go back
			// to the top of the for loop and send another idle message before
			// calling GetMessage again
		} while (PeekMessage(&msg, NULL, NULL, NULL, PM_NOREMOVE));
	}

	return m_nModalResult;
}

int
CPropertyFrameDialog::DoModal()
{
	BOOL	bEnableOwner = FALSE;
	int		nResult;

	// Make sure the parent window isn't a child window. You can't really have
	// a popup window owned by a child window
	if (m_hwndOwner && (GetWindowStyle(m_hwndOwner) & WS_CHILD))
		m_hwndOwner = GetTopLevelWindow(m_hwndOwner);

	// Disable the parent window if necessary
	if (m_hwndOwner != NULL && IsWindowEnabled(m_hwndOwner)) {
		EnableWindow(m_hwndOwner, FALSE);
		bEnableOwner = TRUE;
	}
	
	// Display the dialog
	CreateDialogParam(g_hInstance, MAKEINTRESOURCE(IDD_FRAME), m_hwndOwner,
		(DLGPROC)DialogProc, (LPARAM)this);

	// Run a dispatch loop so we don't return until the dialog is closed
	m_bKeepGoing = TRUE;
	nResult = RunModalLoop();

	// Hide our dialog before enabling the parent window
	SetWindowPos(m_hdlg, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW|SWP_NOSIZE|SWP_NOMOVE|
		SWP_NOACTIVATE|SWP_NOZORDER);

	if (bEnableOwner)
		EnableWindow(m_hwndOwner, TRUE);

	// We don't want to be the active window
	if (m_hwndOwner && GetActiveWindow() == m_hdlg)
		SetActiveWindow(m_hwndOwner);

	// Cleanup
	if (m_pCurPage)
		m_pCurPage->m_pPage->SetObjects(0, NULL);
	DestroyWindow(m_hdlg);

	return nResult;
}

