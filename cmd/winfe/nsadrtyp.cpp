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

// NSAdrTyp.cpp : implementation file
// See NSAdrTyp.h for details on how to use this class
//

#include "stdafx.h"
#include "resource.h"
#include "nsadrlst.h"
#include "nsadrtyp.h"
#include "compfrm.h"

/////////////////////////////////////////////////////////////////////////////
// CNSAddressTypeComboBox

CNSAddressTypeControl::CNSAddressTypeControl()
{
    m_iTypeBitmapWidth = 0;

    penFace.CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNFACE));
    brushFace.CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
#ifdef _WIN32
    pen3dLight.CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DLIGHT));
    pen3dShadow.CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DSHADOW));
    pen3dHilight.CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DHILIGHT));
    pen3dDkShadow.CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DDKSHADOW));
#else
    pen3dLight.CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNHIGHLIGHT));
    pen3dShadow.CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNSHADOW));
    pen3dHilight.CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNHIGHLIGHT));
    pen3dDkShadow.CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNSHADOW));
#endif
}

CNSAddressTypeControl::~CNSAddressTypeControl()
{
	if (m_cfTextFont) {
		theApp.ReleaseAppFont(m_cfTextFont);
	}
}

BOOL CNSAddressTypeControl::Create( CWnd *pParent )
{
	BOOL rv = CListBox::Create( 
		LBS_NOINTEGRALHEIGHT | LBS_NOTIFY | LBS_WANTKEYBOARDINPUT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
		CRect(0,0,0,0), pParent, (UINT)5000 );
	CBitmap cbitmap;
	cbitmap.LoadBitmap(IDB_ARROW3D);
	BITMAP bitmap;
	cbitmap.GetObject(sizeof(BITMAP), &bitmap);
    m_iTypeBitmapWidth = bitmap.bmWidth;
    cbitmap.DeleteObject();
	return rv;
}

BEGIN_MESSAGE_MAP(CNSAddressTypeControl, CListBox)
	//{{AFX_MSG_MAP(CNSAddressTypeControl)
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_WM_KEYDOWN()
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_ENTERIDLE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CNSAddressTypeControl::OnKillFocus(CWnd *pNewWnd) 
{
    GetParent()->SendMessage(WM_CHILDLOSTFOCUS);
	CListBox::OnKillFocus(pNewWnd);
}

BOOL CNSAddressTypeControl::PreTranslateMessage( MSG* pMsg ) 
{
    if ( (pMsg->message == WM_KEYDOWN) && ( (pMsg->wParam == VK_TAB) || (pMsg->wParam == VK_RETURN) || (pMsg->wParam == VK_BACK) ) )
        return ((CNSAddressList *)GetParent())->OnKeyPress( this, pMsg->wParam, LOWORD( pMsg->lParam ), HIWORD( pMsg->lParam ) );

	return CListBox::PreTranslateMessage(pMsg);
}

void CNSAddressTypeControl::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
	CDC * pDC = GetDC();
	TEXTMETRIC tm;
	pDC->GetTextMetrics(&tm);
	lpMeasureItemStruct->itemHeight = tm.tmHeight;
	ReleaseDC(pDC);
}


void CNSAddressTypeControl::OnSetFocus(CWnd* pOldWnd) 
{
	CListBox::OnSetFocus(pOldWnd);
}


void CNSAddressTypeControl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch (nChar) 
	{
		case VK_SPACE:
		    GetParent()->PostMessage(WM_DISPLAYTYPELIST);       
			return;
		case VK_UP:
		case VK_DOWN:
		case VK_HOME: 
		case VK_END: 
	case VK_DELETE:
			((CNSAddressList *)GetParent())->OnKeyPress(this,nChar,nRepCnt,nFlags); 
			return;
		break;
	}
	CListBox::OnKeyDown(nChar, nRepCnt, nFlags);
}


void DrawTransparentBitmap(HDC hdc, HBITMAP hBitmap, short xStart, short yStart, COLORREF cTransparentColor )
{
#ifdef FEATURE_DRAWTRANSBITMAP
#include "nsadrtyp.i00"
#endif
}

int CNSAddressTypeControl::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CListBox::OnCreate(lpCreateStruct) == -1)
		return -1;

	CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
	CDC * pdc = GetDC();
    int16 resource_csid = INTL_CharSetNameToID(INTL_ResourceCharSet());
    LOGFONT lf;                 
    memset(&lf,0,sizeof(LOGFONT));
    lf.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
	lf.lfHeight = -MulDiv(NS_ADDRESSFONTSIZE,pdc->GetDeviceCaps(LOGPIXELSY), 72);
    ReleaseDC ( pdc );
    lf.lfQuality = PROOF_QUALITY;    
    lf.lfCharSet = IntlGetLfCharset(resource_csid); 
    strcpy(lf.lfFaceName, IntlGetUIFixFaceName(resource_csid));
	m_cfTextFont = theApp.CreateAppFont( lf );
	::SendMessage(GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfTextFont, FALSE);
	// TODO: Add your specialized creation code here
	
	return 0;
}

void CNSAddressTypeControl::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CListBox::OnLButtonDown(nFlags, point);
    GetParent()->PostMessage(WM_DISPLAYTYPELIST);       
}

void DrawTypeBitmap(CRect &rect, CDC * pDC, BOOL bPressed, BOOL bHighlight)
{
	CBitmap cbitmap;
	cbitmap.LoadBitmap(bPressed?IDB_ARROW3D_PRESS:IDB_ARROW3D);
	BITMAP bitmap;
	cbitmap.GetObject(sizeof(BITMAP), &bitmap );
	int center_y = rect.top + (rect.Height() - bitmap.bmHeight)/2;
	DrawTransparentBitmap( 
		pDC->GetSafeHdc(), 
	(HBITMAP)cbitmap.GetSafeHandle(), 
		2, 
		center_y, 
		RGB( 255, 0, 255 ) );
    cbitmap.DeleteObject();
}

void CNSAddressTypeControl::DrawHighlight(
    HDC hDC, 
    CRect &rect, 
    HPEN hpenTopLeft, 
    HPEN hpenBottomRight)
{
	HPEN hpenOld = (HPEN) ::SelectObject( hDC, hpenTopLeft );

	::MoveToEx( hDC, rect.left, rect.bottom, NULL );
	::LineTo( hDC, rect.left, rect.top );
	::LineTo( hDC, rect.right - 1, rect.top );

	::SelectObject( hDC, hpenBottomRight );
	::LineTo( hDC, rect.right - 1, rect.bottom - 1);
	::LineTo( hDC, rect.left, rect.bottom - 1);

	::SelectObject( hDC, hpenOld );
}

void CNSAddressTypeControl::DrawRaisedRect(HDC hDC, CRect &rect)
{
    rect.InflateRect(-1,-1);
    DrawHighlight(hDC,rect,(HPEN)pen3dLight.GetSafeHandle(),(HPEN)pen3dShadow.GetSafeHandle());
    rect.InflateRect(1,1);
    DrawHighlight(hDC,rect,(HPEN)pen3dHilight.GetSafeHandle(),(HPEN)pen3dDkShadow.GetSafeHandle());
}

void CNSAddressTypeControl::DrawItemSoItLooksLikeAButton(
    CDC * pDC, CRect &rect, CString & cs )
{
    if (rect.Width() > 0 && rect.Height() > 0)
    {
        COLORREF cOldText = pDC->SetTextColor(GetSysColor(COLOR_BTNTEXT));
        COLORREF cOldBk = pDC->SetBkColor(GetSysColor(COLOR_BTNFACE));

        NS_FillSolidRect(pDC->GetSafeHdc(),rect,GetSysColor(COLOR_BTNFACE));

		CFont *oldFont = pDC->SelectObject(CFont::FromHandle(m_cfTextFont));

        TEXTMETRIC tm;
	    pDC->GetTextMetrics(&tm);
	    int y = ((rect.bottom - rect.top) - tm.tmHeight)/2;

        CBitmap cbitmap;
        cbitmap.LoadBitmap(IDB_ARROW3D);
        BITMAP bitmap;
        cbitmap.GetObject(sizeof(BITMAP), &bitmap );

        rect.left += bitmap.bmWidth + TEXT_PAD;
        rect.top += y;
        CString csRight = cs.Right(cs.GetLength()-1);
        pDC->DrawText(csRight,csRight.GetLength(),rect,DT_LEFT);

	pDC->SelectObject(oldFont);

        rect.top -= y;
        rect.left -= bitmap.bmWidth + TEXT_PAD;
		DrawRaisedRect(pDC->GetSafeHdc(), rect);

        DrawTransparentBitmap( 
            pDC->GetSafeHdc(), 
            (HBITMAP)cbitmap.GetSafeHandle(), 
            2, 
            rect.top + y, 
            RGB( 255, 0, 255 ) );
            cbitmap.DeleteObject();
    }
}

void CNSAddressTypeControl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
	if (lpDrawItemStruct->itemID != -1) 
	{
        CDC dc;
        dc.Attach(lpDrawItemStruct->hDC);
        CRect rect;
        GetClientRect(rect);
        CString cs;
        GetText(lpDrawItemStruct->itemID,cs);
        DrawItemSoItLooksLikeAButton(&dc,rect,cs);
        if (lpDrawItemStruct->itemState & ODS_FOCUS)
        {
            rect.InflateRect(-4,-3);
            dc.DrawFocusRect(rect);
        }
        dc.Detach();
        ValidateRect(rect);
    }
}

void CNSAddressTypeControl::DeleteItem(LPDELETEITEMSTRUCT lpDeleteItemStruct) 
{
	delete (CNSAddressTypeInfo *)GetItemDataPtr( lpDeleteItemStruct->itemID );
	CListBox::DeleteItem(lpDeleteItemStruct);
}
