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
#include "stdafx.h"
#include "msgcom.h"
#include "mailfrm.h"
#include "msgview.h"
#include "fegui.h"
#include "shcut.h"
#include "cxsave.h"
#include "thrdfrm.h"
#include "rdfglobal.h"

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CMessageBodyView, CNetscapeView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

#ifdef _WIN32

/////////////////////////////////////////////////////////////////////
//
// CAttachmentDataSource
//
// DataSource for attachment download for drag-and-drop
//

class CAttachmentDataSource: public COleDataSource
{
protected:
	CString m_csURL, m_csName;

public:
	CAttachmentDataSource(LPCTSTR lpszURL, LPCTSTR lpszName);

	virtual BOOL OnRenderGlobalData(LPFORMATETC lpFormatEtc, HGLOBAL* phGlobal);
	virtual BOOL OnRenderFileData(LPFORMATETC lpFormatEtc, CFile* pFile);
	virtual BOOL OnRenderData(LPFORMATETC lpFormatEtc, LPSTGMEDIUM lpStgMedium);
};

CAttachmentDataSource::CAttachmentDataSource(LPCTSTR lpszURL, LPCSTR lpszName)
{
	m_csURL = lpszURL;
	m_csName = lpszName;
}

BOOL CAttachmentDataSource::OnRenderGlobalData(LPFORMATETC lpFormatEtc, HGLOBAL* phGlobal)
{
	BOOL bRes = FALSE;

	if (sysInfo.m_bWinNT && !*phGlobal) 
	{
		bRes = CSaveCX::SaveToGlobal(phGlobal, m_csURL, m_csName);
	}

	return bRes;
}

BOOL CAttachmentDataSource::OnRenderFileData(LPFORMATETC lpFormatEtc, CFile *pFile)
{
	BOOL bRes = FALSE;

	if (pFile) {
		bRes = CSaveCX::SaveToFile(pFile, m_csURL, m_csName);
	}

	return bRes;
}

BOOL CAttachmentDataSource::OnRenderData(LPFORMATETC lpFormatEtc, LPSTGMEDIUM
										 lpStgMedium)
{
	if (lpFormatEtc->tymed & TYMED_ISTREAM)
	{
		COleStreamFile file;
		if (lpStgMedium->tymed == TYMED_ISTREAM)
		{
			ASSERT(lpStgMedium->pstm != NULL);
			file.Attach(lpStgMedium->pstm);
		}
		else
		{
			if (!file.CreateMemoryStream())
				AfxThrowMemoryException();
		}
		// get data into the stream
		if (OnRenderFileData(lpFormatEtc, &file))
		{
			lpStgMedium->tymed = TYMED_ISTREAM;
			lpStgMedium->pstm = file.Detach();
			return TRUE;
		}
		if (lpStgMedium->tymed == TYMED_ISTREAM)
			file.Detach();
	}
	return COleDataSource::OnRenderData(lpFormatEtc, lpStgMedium);
}

#endif

/////////////////////////////////////////////////////////////////////
//
// CAttachmentTray
//

CAttachmentTray::CAttachmentTray()
{
	m_hFont = 0;
	SetCSID(CIntlWin::GetSystemLocaleCsid(), FALSE);

	::SetRectEmpty(&m_rcClient);

	m_pAttachmentData = NULL;
	m_nAttachmentCount = 0;
	m_aAttachAux = NULL;

	m_sizeSpacing.cx = ::GetSystemMetrics(SM_CXICONSPACING);
	m_sizeSpacing.cy = ::GetSystemMetrics(SM_CYICONSPACING);
	m_sizeIcon.cx = ::GetSystemMetrics(SM_CXICON);
	m_sizeIcon.cy = ::GetSystemMetrics(SM_CYICON);

	m_ptOrigin.x = 0;
	m_ptOrigin.y = 0;
	m_sizeMax.cx = 0;
	m_sizeMax.cy = 0;
}

CAttachmentTray::~CAttachmentTray()
{
	for (int i = 0; i < m_nAttachmentCount; i++)
#ifdef _WIN32
		ImageList_Destroy(m_aAttachAux[i].hImage);
#else
		DestroyIcon(m_aAttachAux[i].hIcon);
#endif
	delete [] m_aAttachAux;
	theApp.ReleaseAppFont(m_hFont);
}

HICON CAttachmentTray::DredgeUpIcon(char *name)
{
	// Get the default icon
	HICON res = ::LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(IDI_FILE));
#ifdef _WIN32
	if (sysInfo.m_bWin4) {
		// Do we have a name?
		if (name) {
			// Dig out the extension
			char *pszExt = strrchr(name, '.');

			// Did we get an extension?
			if (pszExt) {
				// Look up the file type
				_TCHAR szBuf[32];
				LONG cbBuf = 32;
				LONG lres = ::RegQueryValue( HKEY_CLASSES_ROOT, pszExt, szBuf, &cbBuf );

				// Did we find a file type?
				if (lres == ERROR_SUCCESS) {
					// Figure out the default icon path
					_TCHAR szIconPath[_MAX_PATH];
					_TCHAR szRegKey[_MAX_PATH];
					LONG cbIconPath = _MAX_PATH;

					_tcscpy(szRegKey, szBuf);
					_tcscat(szRegKey, _T("\\DefaultIcon"));

					lres = ::RegQueryValue( HKEY_CLASSES_ROOT, szRegKey, szIconPath, &cbIconPath );

					// Did we get an icon path
					if (lres == ERROR_SUCCESS) {
						// Split up the path given to us
						_TCHAR *pszPath = _tcstok(szIconPath, ",");
						_TCHAR *pszIndex = _tcstok(NULL, ",");
						UINT nIndex = pszIndex ? _ttoi(pszIndex) : 0;

						// Go get the icon
						HICON resIcon = ::ExtractIcon(AfxGetInstanceHandle(), pszPath, nIndex);
						if (resIcon && (int) resIcon != 1) {
							res = resIcon;
						}
					}
				}
			}
		}
	}
#endif
	return res;
}

void CAttachmentTray::SetAttachments(int nAttachmentCount, MSG_AttachmentData *pAttachmentData)
{
	// Clean up old images
	if (nAttachmentCount < m_nAttachmentCount) {
		for (int i = 0; i < m_nAttachmentCount; i++)
#ifdef _WIN32
			ImageList_Destroy(m_aAttachAux[i].hImage);
#else
			DestroyIcon(m_aAttachAux[i].hIcon);
#endif
		delete [] m_aAttachAux;
		m_aAttachAux = NULL;
		m_nAttachmentCount = 0;
	}

	AttachAux *aTemp = nAttachmentCount > 0 ? new AttachAux[nAttachmentCount] : NULL;

	if (nAttachmentCount > 0) {
		int i = 0;
		while (i < nAttachmentCount) {
			if (i < m_nAttachmentCount) {
				aTemp[i] = m_aAttachAux[i];
			} else {
				aTemp[i].bSelected = FALSE;
#ifdef _WIN32
				aTemp[i].hImage = ImageList_Create(m_sizeIcon.cx, m_sizeIcon.cy, ILC_MASK, 1, 1);
				ImageList_AddIcon(aTemp[i].hImage, DredgeUpIcon(pAttachmentData[i].real_name));
#else
				aTemp[i].hIcon = DredgeUpIcon(pAttachmentData[i].real_name);
#endif
				aTemp[i].ptPos.x = 0;
				aTemp[i].ptPos.y = 0;
			}
			i++;
		}
	}

	delete [] m_aAttachAux;
	m_aAttachAux = aTemp;
	m_nAttachmentCount = nAttachmentCount;
	m_pAttachmentData = pAttachmentData;

	Layout();
	UpdateWindow();
}

void CAttachmentTray::Layout()
{
	int x = 0;
	int y = 0;
	for (int i = 0; i < m_nAttachmentCount; i++) {
		m_aAttachAux[i].ptPos.x = x;
		m_aAttachAux[i].ptPos.y = y;

		m_sizeMax.cx = MAX(x + m_sizeSpacing.cx, m_sizeMax.cy);
		m_sizeMax.cy = MAX(y + m_sizeSpacing.cy, m_sizeMax.cy);

		x += m_sizeSpacing.cx;
		if ( (x + m_sizeSpacing.cx) > m_rcClient.right ) {
			x = 0;
			y += m_sizeSpacing.cy;
		}
	}
	
	if (m_sizeMax.cy > m_rcClient.bottom) {
        ShowScrollBar(SB_VERT, TRUE);
#ifdef WIN32
        SCROLLINFO ScrollInfo;
        ScrollInfo.cbSize = sizeof(SCROLLINFO);
        ScrollInfo.fMask = SIF_PAGE|SIF_RANGE|SIF_POS;
		ScrollInfo.nMin = 0;
		ScrollInfo.nMax = m_sizeMax.cy;
        ScrollInfo.nPage = m_sizeSpacing.cy;
		ScrollInfo.nPos = m_ptOrigin.y;
        SetScrollInfo(SB_VERT, &ScrollInfo, TRUE);
#else
		SetScrollPos(SB_VERT, m_ptOrigin.y);
        SetScrollRange(SB_VERT, 0, m_sizeMax.cy - m_sizeSpacing.cy + 1, TRUE);
#endif
    } else {
        ShowScrollBar ( SB_VERT, FALSE );
    }

}

BOOL CAttachmentTray::WaitForDrag()
{
	BOOL bDrag = FALSE;

	POINT pt;
	RECT rcDrag;
#ifdef _WIN32
	int dx = ::GetSystemMetrics(SM_CXDRAG);
	int dy = ::GetSystemMetrics(SM_CYDRAG);
#else
	int dx = ::GetSystemMetrics(SM_CXDOUBLECLK);
	int dy = ::GetSystemMetrics(SM_CYDOUBLECLK);
#endif

	GetCursorPos(&pt);
	
	::SetRect(&rcDrag, pt.x - dx, pt.y - dy, pt.x + dx, pt.y + dy);

	SetCapture();
	while (!bDrag)
	{
		// some applications steal capture away at random times
		if (CWnd::GetCapture() != this)
			break;

		// peek for next input message
		MSG msg;
		if (::PeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) ||
			::PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
		{
			// check for button cancellation (any button down will cancel)
			if (msg.message == WM_LBUTTONUP || msg.message == WM_RBUTTONUP ||
				msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN)
				break;

			// check for keyboard cancellation
			if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
				break;

			// check for drag start transition
			bDrag = !::PtInRect(&rcDrag, msg.pt);
		}
	}
	ReleaseCapture();
	return bDrag;
}


int CAttachmentTray::Point2Index(POINT pt)
{
	int i = 0;
	int x = pt.x + m_ptOrigin.x;
	int y = pt.y + m_ptOrigin.y;
	while (i < m_nAttachmentCount) {
		if (x >= m_aAttachAux[i].ptPos.x && 
			y >= m_aAttachAux[i].ptPos.y &&
			x < (m_aAttachAux[i].ptPos.x + m_sizeSpacing.cx) &&
			y < (m_aAttachAux[i].ptPos.y + m_sizeSpacing.cy)) {
			return i;
		}
		i++;
	}
	return -1;
}

void CAttachmentTray::Index2Rect(int idx, RECT &rect)
{
	::SetRectEmpty(&rect);

	if (idx < 0 || idx >= m_nAttachmentCount)
		return;

	rect.left = m_aAttachAux[idx].ptPos.x - m_ptOrigin.x;
	rect.top = m_aAttachAux[idx].ptPos.y - m_ptOrigin.y;
	rect.right = rect.left + m_sizeSpacing.cx;
	rect.bottom = rect.top + m_sizeSpacing.cy;
}

void CAttachmentTray::ClearSelection()
{
	for (int i = 0; i < m_nAttachmentCount; i++) {
		if (m_aAttachAux[i].bSelected) {
			m_aAttachAux[i].bSelected = FALSE;
			RECT rcInval;
			Index2Rect(i, rcInval);
			InvalidateRect(&rcInval);
		}
	}
}

BEGIN_MESSAGE_MAP(CAttachmentTray, CWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_VSCROLL()
END_MESSAGE_MAP()

void CAttachmentTray::OnPaint()
{
	CPaintDC dc(this);

	HBRUSH hbrush = CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
	::FillRect( dc.m_hDC, &dc.m_ps.rcPaint, hbrush );
	VERIFY(::DeleteObject(hbrush));

	int i = 0;

	HFONT hOldFont = (HFONT) dc.SelectObject(m_hFont);
	COLORREF oldTextColor = dc.SetTextColor(GetSysColor(COLOR_WINDOWTEXT));
	COLORREF oldTextBkColor = dc.SetBkColor(GetSysColor(COLOR_WINDOW));
	while (i < m_nAttachmentCount) {
		int x = m_aAttachAux[i].ptPos.x - m_ptOrigin.x;
		int y = m_aAttachAux[i].ptPos.y - m_ptOrigin.y;

		RECT rcInter, rcIcon;
		::SetRect(&rcIcon, x, y, x + m_sizeSpacing.cx, y + m_sizeSpacing.cy);
		if (::IntersectRect(&rcInter, &rcIcon, &dc.m_ps.rcPaint)) {
			RECT rcText;
			::SetRect(&rcText, x, y + m_sizeIcon.cy + 8, x+m_sizeSpacing.cx, y + m_sizeSpacing.cy - 2);
			int dx = x + (m_sizeSpacing.cx - m_sizeIcon.cx) / 2;
			int dy = y + 6;
			
#ifdef _WIN32
			if (m_aAttachAux[i].hImage)
				ImageList_Draw(m_aAttachAux[i].hImage, 0, dc.m_hDC, dx, dy, 
							   m_aAttachAux[i].bSelected ? ILD_SELECTED : ILD_NORMAL);
#else
			if (m_aAttachAux[i].hIcon) {
				dc.DrawIcon(dx, dy, m_aAttachAux[i].hIcon);
			}
#endif
			if (m_aAttachAux[i].bSelected) {
				dc.SetTextColor(GetSysColor(COLOR_HIGHLIGHTTEXT));
				dc.SetBkColor(GetSysColor(COLOR_HIGHLIGHT));
			} else {
				dc.SetTextColor(GetSysColor(COLOR_WINDOWTEXT));
				dc.SetBkColor(GetSysColor(COLOR_WINDOW));
			}
			if (m_pAttachmentData[i].real_name)
				WFE_DrawTextEx(0, dc.m_hDC, m_pAttachmentData[i].real_name, -1, &rcText,
							   DT_CENTER|DT_NOPREFIX, WFE_DT_CROPRIGHT);
		}
		i++;
	}

	if (hOldFont)
		dc.SelectObject(hOldFont);
	dc.SetTextColor(oldTextColor);
	dc.SetBkColor(oldTextBkColor);
}

void CAttachmentTray::OnSize( UINT nType, int cx, int cy )
{
	if (nType != SIZE_MINIMIZED && 
		(m_rcClient.right != cx || m_rcClient.bottom != cy)) {

		m_rcClient.right = cx;
		m_rcClient.bottom = cy;

		Layout();
	}
}

#define ABS(x) ((x) < 0 ? (-x) : (x))

void CAttachmentTray::OnLButtonDown( UINT nFlags, CPoint point )
{	
	int idx = Point2Index(point);

	// Ignore if nothing hit, for now
	if (idx >= 0) {
		RECT rect;
		Index2Rect(idx, rect);

		BOOL bSel = m_aAttachAux[idx].bSelected;
		BOOL bCtrl = ::GetKeyState(VK_CONTROL) & 0x8000;

		// If not selected, select immediately
		if (!bSel) {
			ClearSelection();
			m_aAttachAux[idx].bSelected = TRUE;
			InvalidateRect(&rect);
			UpdateWindow();
		}

		if (WaitForDrag()) {
			CString csUntitled;
			csUntitled.LoadString(IDS_UNTITLED);

			// Drag Drop Section
			const char *name = "";
			if (m_pAttachmentData[idx].real_name)
				name = m_pAttachmentData[idx].real_name;
			else
				name = csUntitled;

#ifdef _WIN32
			COleDataSource *pDataSource = new CAttachmentDataSource(m_pAttachmentData[idx].url, name);

			HGLOBAL hfgd = GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE,sizeof(FILEGROUPDESCRIPTOR));
			LPFILEGROUPDESCRIPTOR lpfgd = (LPFILEGROUPDESCRIPTOR) GlobalLock(hfgd);

			// one file in the file descriptor block
			lpfgd->cItems = 1;
			lpfgd->fgd[0].dwFlags = 0;
			strcpy(lpfgd->fgd[0].cFileName, name);
			GlobalUnlock (hfgd);

			// register the file descriptor clipboard format
			UINT cfFileDescriptor = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);

			// register the clipboard format for file contents
			UINT cfFileContents = RegisterClipboardFormat(CFSTR_FILECONTENTS);

			pDataSource->CacheGlobalData(cfFileDescriptor, hfgd);

			FORMATETC formatEtc;
			formatEtc.cfFormat = cfFileContents;
			formatEtc.ptd = NULL;
			formatEtc.dwAspect = DVASPECT_CONTENT;
			formatEtc.lindex = 0;
			if (sysInfo.m_bWinNT)
				formatEtc.tymed = TYMED_HGLOBAL;
			else
				formatEtc.tymed = TYMED_ISTREAM;
			pDataSource->DelayRenderFileData(cfFileContents, &formatEtc);

#else
			COleDataSource *pDataSource = new COleDataSource;
#endif
			RDFGLOBAL_DragTitleAndURL(pDataSource, name, m_pAttachmentData[idx].url);

			DROPEFFECT res = pDataSource->DoDragDrop( DROPEFFECT_COPY|DROPEFFECT_SCROLL );

			pDataSource->Empty();
			delete pDataSource;
		} else {
			if (bSel && bCtrl) {
				m_aAttachAux[idx].bSelected = FALSE;
				InvalidateRect(&rect);
				UpdateWindow();
			}
		}
	}
}

void CAttachmentTray::OnLButtonDblClk( UINT nFlags, CPoint point )
{
	int idx = Point2Index(point);
	if (idx >= 0) {
		GetParentFrame()->PostMessage(WM_COMMAND, (WPARAM) (FIRST_ATTACH_MENU_ID + idx), (LPARAM) 0);
	}
}

void CAttachmentTray::OnRButtonDown( UINT nFlags, CPoint point )
{	
	int idx = Point2Index(point);

	// Ignore if nothing hit, for now
	if (idx >= 0) {
		RECT rect;
		Index2Rect(idx, rect);

		BOOL bSel = m_aAttachAux[idx].bSelected;
		BOOL bCtrl = ::GetKeyState(VK_CONTROL) & 0x8000;

		// If not selected, select immediately
		if (!bSel) {
			ClearSelection();
			m_aAttachAux[idx].bSelected = TRUE;
			InvalidateRect(&rect);
			UpdateWindow();
		}
		HMENU hMenu = ::CreatePopupMenu();
		::AppendMenu(hMenu, MF_STRING, (FIRST_ATTACH_MENU_ID + idx), szLoadString(IDS_POPUP_OPENATTACHMENT));
		::AppendMenu(hMenu, MF_STRING, (FIRST_SAVEATTACH_MENU_ID + idx), szLoadString(IDS_POPUP_SAVEATTACHMENTAS));
		::AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		::AppendMenu(hMenu, MF_STRING, (FIRST_ATTACHPROP_MENU_ID + idx), szLoadString(IDS_POPUP_ATTACHMENTPROP));

		::ClientToScreen(m_hWnd, &point);
		::TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, point.x, point.y, 0, 
						 GetParentFrame()->GetSafeHwnd(), NULL);
		::DestroyMenu(hMenu);
	}
}

void CAttachmentTray::OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar )
{
	POINT  ptOld = m_ptOrigin;

	switch (nSBCode) {
	case SB_BOTTOM:
		m_ptOrigin.y = m_sizeMax.cy - m_sizeSpacing.cy;
		break;
	case SB_ENDSCROLL:
		break;
	case SB_LINEDOWN:
		m_ptOrigin.y++;
		break;
	case SB_LINEUP:
		m_ptOrigin.y--;
		break;
	case SB_PAGEDOWN:
		m_ptOrigin.y += m_sizeSpacing.cy;
		break;
	case SB_PAGEUP:
		m_ptOrigin.y -= m_sizeSpacing.cy;
		break;
	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		m_ptOrigin.y = nPos;
		break;
	case SB_TOP:
		m_ptOrigin.y = 0;
		break;
	}
    if (m_ptOrigin.y < 0)
        m_ptOrigin.y = 0;
	if (m_ptOrigin.y > (m_sizeMax.cy - m_rcClient.bottom))
		m_ptOrigin.y = m_sizeMax.cy - m_rcClient.bottom;

    int iDiff = ptOld.y - m_ptOrigin.y;
    if ( iDiff != 0 )
    {
        ScrollWindow(0, iDiff );
        UpdateWindow();
    }
    SetScrollPos ( SB_VERT, m_ptOrigin.y );
}

void CAttachmentTray::SetCSID(int16 doc_csid, XP_Bool updateWindow)
{
	if (m_hFont)
		theApp.ReleaseAppFont(m_hFont);

	LOGFONT lf;  

	memset(&lf,0,sizeof(LOGFONT));

	lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
	lf.lfWeight = FW_NORMAL;
	lf.lfCharSet = IntlGetLfCharset(doc_csid);
	if (doc_csid == CS_LATIN1)
 		strcpy(lf.lfFaceName, "MS Sans Serif");
	else
 		strcpy(lf.lfFaceName, IntlGetUIPropFaceName(doc_csid));
	lf.lfQuality = PROOF_QUALITY;    
	lf.lfHeight =  -10; // 8 points
	m_hFont = theApp.CreateAppFont( lf );

	if (updateWindow)
	{
		Layout();
		Invalidate();
		UpdateWindow();
	}

}


/////////////////////////////////////////////////////////////////////
//
// CMessageBodyView
//

BEGIN_MESSAGE_MAP(CMessageBodyView, CNetscapeView)
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

void CMessageBodyView::OnSetFocus(CWnd *pOldWnd)
{
	CNetscapeView::OnSetFocus(pOldWnd);

	CMessageView* pParentView = (CMessageView*)GetParent();
	if (pParentView->GetParentFrame()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
	{
		pParentView->UpdateFocusFrame();
		((C3PaneMailFrame*)pParentView->GetParentFrame())->SetFocusWindow(this);
	}
}

void CMessageBodyView::OnKillFocus(CWnd *pOldWnd)
{
	CNetscapeView::OnKillFocus(pOldWnd);

	CMessageView* pParentView = (CMessageView*)GetParent();
	if (pParentView->GetParentFrame()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
	{
		((CMessageView*)GetParent())->UpdateFocusFrame();
	}
}

/////////////////////////////////////////////////////////////////////
//
// CMessageView
//

CMessageView::CMessageView()
{
	m_pViewMessage = NULL;
	m_pWndAttachments = NULL;
#ifdef WIN32
	m_iAttachmentsHeight = ::GetSystemMetrics(SM_CYICONSPACING) +
						   ::GetSystemMetrics(SM_CYEDGE) * 2;
	m_iSliderHeight = ::GetSystemMetrics(SM_CYSIZEFRAME);
#else
	m_iAttachmentsHeight = ::GetSystemMetrics(SM_CYICONSPACING) +
						   ::GetSystemMetrics(SM_CYBORDER) * 2;
	m_iSliderHeight = ::GetSystemMetrics(SM_CYFRAME);
#endif
	m_bAttachmentsVisible = FALSE;
	::SetRectEmpty(&m_rcClient);
}

CMessageView::~CMessageView()
{
	delete m_pWndAttachments;
}

void CMessageView::SetAttachments(int nAttachmentCount, MSG_AttachmentData *pAttachmentData)
{
	m_nAttachmentCount = nAttachmentCount;
	ASSERT(m_pWndAttachments);
	m_pWndAttachments->SetAttachments(nAttachmentCount, pAttachmentData);

	OnSize(0, m_rcClient.right, m_rcClient.bottom);
}

void CMessageView::ShowAttachments(BOOL bShow)
{
	if (m_bAttachmentsVisible != bShow) {
		m_bAttachmentsVisible = bShow;

		OnSize(0, m_rcClient.right, m_rcClient.bottom);
	}
}

BOOL CMessageView::AttachmentsVisible() const
{
	return m_bAttachmentsVisible;
}

BOOL CMessageView::OnCommand( WPARAM wParam, LPARAM lParam )
{
	BOOL res = (BOOL) m_pViewMessage->SendMessage(WM_COMMAND, wParam, lParam);
	if (!res)
		res = CView::OnCommand(wParam, lParam);
	return res;
}

BOOL CMessageView::OnCmdMsg( UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo )
{
	BOOL res = m_pViewMessage->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	if (!res)
		res = CView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
	return res;
}

void CMessageView::OnDraw(CDC* pDC)
{
	HBRUSH hbrushButton = CreateSolidBrush( GetSysColor(COLOR_BTNFACE));
	HPEN hpenLight = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
	HPEN hpenShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));

	::FillRect(pDC->m_hDC, &m_rcSlider, hbrushButton);

	HPEN hpenOld = (HPEN) SelectObject(pDC->m_hDC, hpenLight);
	::MoveToEx(pDC->m_hDC, m_rcSlider.left, m_rcSlider.top, NULL);
	::LineTo(pDC->m_hDC, m_rcSlider.right, m_rcSlider.top);
	SelectObject(pDC->m_hDC, hpenShadow);
	::MoveToEx(pDC->m_hDC, m_rcSlider.left, m_rcSlider.bottom - 1, NULL);
	::LineTo(pDC->m_hDC, m_rcSlider.right, m_rcSlider.bottom - 1);
	SelectObject(pDC->m_hDC, hpenOld);

	VERIFY(::DeleteObject(hbrushButton));
	VERIFY(::DeleteObject(hpenLight));
	VERIFY(::DeleteObject(hpenShadow));	


	if (GetParentFrame()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
	{
		HBRUSH hBrush = NULL;
		CWnd* pFocusWnd = GetFocus();
		if (pFocusWnd && (pFocusWnd == this || pFocusWnd == m_pViewMessage ||
			pFocusWnd == m_pWndAttachments))
			hBrush = ::CreateSolidBrush( RGB(0, 0, 0) );
		else
			hBrush = ::CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );

		RECT clientRect;
		GetClientRect(&clientRect);
		::FrameRect( pDC->m_hDC, &clientRect, hBrush );	 
		VERIFY(DeleteObject( hBrush ));
	}
}

BEGIN_MESSAGE_MAP(CMessageView, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_SETCURSOR()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

int CMessageView::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	int res = CView::OnCreate(lpCreateStruct);
	if (res != -1) {
		m_pViewMessage = (CMessageBodyView *) RUNTIME_CLASS(CMessageBodyView)->CreateObject();
#ifdef _WIN32
		m_pViewMessage->CreateEx(0, NULL, NULL, 
							   WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
							   0,0,0,0, m_hWnd, (HMENU) IDW_MESSAGE_PANE, lpCreateStruct->lpCreateParams);
#else
		m_pViewMessage->Create(NULL, NULL, 
							   WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
							   CRect(0,0,0,0), this, IDW_MESSAGE_PANE, (CCreateContext *) lpCreateStruct->lpCreateParams);
#endif
		m_pViewMessage->SendMessage(WM_INITIALUPDATE);

		m_pWndAttachments = new CAttachmentTray;
#ifdef _WIN32
		m_pWndAttachments->CreateEx(0, NULL, NULL, 
									WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
									0,0,0,0, m_hWnd, (HMENU) IDW_ATTACHMENTS_PANE, lpCreateStruct->lpCreateParams );
#else
		m_pWndAttachments->Create(NULL, NULL, 
								  WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
								  CRect(0,0,0,0), this, IDW_ATTACHMENTS_PANE, (CCreateContext *) lpCreateStruct->lpCreateParams);
#endif
	}
	return res;
}

void CMessageView::OnSize( UINT nType, int cx, int cy )
{
	if (nType == SIZE_MINIMIZED || !cx || !cy)
		return;

	m_rcClient.right = cx;
	m_rcClient.bottom = cy;

	if (m_bAttachmentsVisible && m_nAttachmentCount > 0) {
		::SetRect(&m_rcSlider, 0, cy - m_iAttachmentsHeight - m_iSliderHeight - 1, cx, cy - m_iAttachmentsHeight);
	} else {
		::SetRect(&m_rcSlider, 0, cy - 1, cx, cy);
	}
	if (m_pViewMessage) {
		m_pViewMessage->MoveWindow(1, 1, cx - 2, m_rcSlider.top - 1);
	}

	if (m_pWndAttachments) {
		m_pWndAttachments->MoveWindow(1, m_rcSlider.bottom, cx - 2, cy - m_rcSlider.bottom - 1);
	}
	Invalidate();
	UpdateWindow();
}

BOOL CMessageView::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message )
{
	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(&pt);

	// Handle in OnMouseMove
	if (::PtInRect(&m_rcSlider, pt)) {
		SetCursor( theApp.LoadCursor ( AFX_IDC_VSPLITBAR ) );
		return TRUE;
	} 
	
	return CView::OnSetCursor(pWnd, nHitTest, message);
}

void CMessageView::UpdateFocusFrame()
{
	if (GetParentFrame()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
	{
		RECT clientRect;
		GetClientRect(&clientRect);
		InvalidateRect(&clientRect, FALSE);
		::InflateRect(&clientRect, -1, -1);
		ValidateRect(&clientRect);
		UpdateWindow();
	}
}

void CMessageView::OnSetFocus(CWnd *pOldWnd)
{
	CView::OnSetFocus(pOldWnd);

	if (GetParentFrame()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
	{
		UpdateFocusFrame();
		((C3PaneMailFrame*)GetParentFrame())->SetFocusWindow(m_pViewMessage);
	}
	else
		m_pViewMessage->SetFocus();
}

void CMessageView::OnKillFocus(CWnd *pOldWnd)
{
	CView::OnKillFocus(pOldWnd);

	if (GetParentFrame()->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame)))
	{
		UpdateFocusFrame();
	}
}

void CMessageView::OnLButtonDown( UINT nFlags, CPoint point )
{
	if (::PtInRect(&m_rcSlider, point)) {
		SetCapture();
		m_ptHit = point;
		m_iOldHeight = m_iAttachmentsHeight;
	}
}

void CMessageView::OnMouseMove( UINT nFlags, CPoint point )
{
	if (GetCapture() == this) {
		RECT rect;
		GetClientRect(&rect);
		m_iAttachmentsHeight = m_iOldHeight + (m_ptHit.y - point.y);

		// Oh, the sanity...
		if (m_iAttachmentsHeight < 0)
			m_iAttachmentsHeight = 0;
		if (m_iAttachmentsHeight > (m_rcClient.bottom - m_iSliderHeight))
			m_iAttachmentsHeight = m_rcClient.bottom - m_iSliderHeight;

		OnSize(0, rect.right, rect.bottom);
	}
}

void CMessageView::OnLButtonUp( UINT nFlags, CPoint point )
{
	if (GetCapture() == this) {
		ReleaseCapture();
	}
}

void CMessageView::OnDestroy()
{
	CView::OnDestroy();
}

void CMessageView::SetCSID(int16 doc_csid)
{
	if (m_pWndAttachments)
		m_pWndAttachments->SetCSID(doc_csid);
}

BOOL CMessageView::HasFocus()
{
	CWnd* pFocusWnd = CWnd::GetFocus();

	return(pFocusWnd == this || pFocusWnd == m_pWndAttachments ||
		   pFocusWnd == m_pViewMessage);

}


#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CMessageView, CView)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif
