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

//
//	This file contains the implementation of 
//		CODNetscapeButton
//		CODMochaListBox
//		CODMochaComboBox
//
//	Why we need the ownerdrawn version ?
//	1. So we could display UTF8 widget
//	2. So we coudl use Unicode Font on US Win95 to display non-Latin 
//		characters.



//	Requried include file
#include "stdafx.h"

#include "odctrl.h"
#include "intlwin.h"
#include "intl_csi.h"


//////////////////////////////////////////////////////////////////////////
//
//	CODNetscapeButton
//
//////////////////////////////////////////////////////////////////////////

CODNetscapeButton::CODNetscapeButton(MWContext * context, LO_FormElementStruct * form, CWnd* pParent) :
	CNetscapeButton(context, form, pParent)
{
}

void CODNetscapeButton::Draw3DBox(CDC *pDC, CRect& r, BOOL selected)
{

	// fill out the rectangle wiht background brush
	CBrush* pBrush = new CBrush ( ::GetSysColor (COLOR_BTNFACE));

	pDC->FillRect (r, pBrush);
	delete pBrush;

	// decide which color we should use to draw the four edges
	COLORREF color1, color2, color3, color4;

#ifdef XP_WIN32
	if(selected)
	{
		color1 = ::GetSysColor(COLOR_3DHILIGHT);
		color2 = ::GetSysColor(COLOR_3DLIGHT);
		color3 = ::GetSysColor(COLOR_3DDKSHADOW);
		color4 = ::GetSysColor(COLOR_3DSHADOW);
	}
	else
	{
		color1 = ::GetSysColor(COLOR_3DDKSHADOW);
		color2 = ::GetSysColor(COLOR_3DSHADOW);
		color3 = ::GetSysColor(COLOR_3DHILIGHT);
		color4 = ::GetSysColor(COLOR_3DLIGHT);
	}
#else
	if(selected)
	{
		color1 = ::GetSysColor(COLOR_BTNHIGHLIGHT);
		color2 = ::GetSysColor(COLOR_BTNHIGHLIGHT);
		color3 = ::GetSysColor(COLOR_BTNSHADOW);
		color4 = ::GetSysColor(COLOR_BTNSHADOW);
	}
	else
	{
		color1 = ::GetSysColor(COLOR_BTNSHADOW);
		color2 = ::GetSysColor(COLOR_BTNSHADOW);
		color3 = ::GetSysColor(COLOR_BTNHIGHLIGHT);
		color4 = ::GetSysColor(COLOR_BTNHIGHLIGHT);
	}
#endif

	// 3. create pen and draw them
	CPen* pPen1 = new CPen( PS_SOLID, 0, color1);
	CPen* pOldpen = pDC->SelectObject(pPen1);

	POINT seg1[3];
	seg1[0].x = r.left; 
	seg1[0].y = r.bottom-1;
	seg1[1].x = r.right-1;
	seg1[1].y = r.bottom-1;
	seg1[2].x = r.right-1;
	seg1[2].y = r.top;
	pDC->Polyline(seg1, 3);

	CPen* pPen2 = new CPen( PS_SOLID, 0, color2);
	pDC->SelectObject(pPen2);
	
	delete pPen1;

	POINT seg2[3];
	seg2[0].x =	r.left+1;
	seg2[0].y =	r.bottom-2;
	seg2[1].x =	r.right-2;
	seg2[1].y =	r.bottom-2; 
	seg2[2].x =	r.right-2;
	seg2[2].y =	r.top;
	pDC->Polyline(seg2, 3);

	CPen* pPen3 = new CPen( PS_SOLID, 0, color3);
	pDC->SelectObject(pPen3);
	delete pPen2;

	POINT seg3[3];
	seg3[0].x =	r.left;	
	seg3[0].y =	r.bottom-2; 
	seg3[1].x =	r.left;
	seg3[1].y =	r.top; 
	seg3[2].x =	r.right-1;
	seg3[2].y =	r.top;
	pDC->Polyline(seg3, 3);

	CPen* pPen4 = new CPen( PS_SOLID, 0, color4);
	pDC->SelectObject(pPen4);
	delete pPen3;

	POINT seg4[3]; 
	seg4[0].x =	r.left+1;
	seg4[0].y =	r.bottom-3; 
	seg4[1].x =	r.left+1;
	seg4[1].y =	r.top+1; 
	seg4[2].x =	r.right-2;
	seg4[2].y =	r.top+1;
	pDC->Polyline(seg4, 3);

	pDC->SelectObject(pOldpen);
	delete pPen4;
}
void CODNetscapeButton::DrawCenterText(CDC *pDC, LPCSTR text, CRect& r)
{
	CSize size;
	int16 wincsid = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(m_Context));

	LO_TextAttr *text_attr = m_Form->text_attr;

	//	Measure what the size of the button should be.
	CyaFont	*pMyFont;
	CXDC(m_Context)->SelectNetscapeFont( pDC->GetSafeHdc(), text_attr, pMyFont );
	// pMyFont->PrepareDrawText(pDC->GetSafeHdc());

	CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, wincsid, 
		pDC->GetSafeHdc(), text, XP_STRLEN(text), &size);

	CIntlWin::TextOutWithCyaFont(pMyFont, wincsid, pDC->GetSafeHdc(), 
		(r.right + r.left - size.cx) / 2 , 
		(r.top + r.bottom - size.cy) / 2 , 
		text, XP_STRLEN(text));

	// pMyFont->EndDrawText( pDC->GetSafeHdc( ));
	CXDC(m_Context)->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );

}
void CODNetscapeButton::DrawContent(CDC *pDC, LPDRAWITEMSTRUCT lpdis)
{

	CString text;
	int16 win_csid = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(m_Context));

	if (CS_UTF8 == win_csid)
	{
		// UTF8 text is broken inside MFC or WINAPI.
		// We need to use original data in form.
		// We should maybe only use this part (not call MFC get a window text).
		// This should be reviewed again for Gromit.
		LO_FormElementStruct* elm = GetElement();
		lo_FormElementMinimalData ele_minimal = elm->element_data->ele_minimal;
		char *cp = (char *) (ele_minimal.value);
		text = CString(cp);
	}
	else
	{
		GetWindowText(text);
	}

	// set the mode to TRANSPARENT
	pDC->SetBkMode(TRANSPARENT);

	// If it is disabled, draw a ghost one first.
	if(lpdis->itemState & ODS_DISABLED)
	{
#ifdef XP_WIN32
		pDC->SetTextColor(::GetSysColor(COLOR_3DLIGHT));
#else
		// *** Fix me ftang
		pDC->SetTextColor(::GetSysColor(COLOR_HIGHLIGHTTEXT));
#endif

		CRect ghostRect = lpdis->rcItem;
		ghostRect.OffsetRect(1,1);

		this->DrawCenterText(pDC, (LPCSTR)text, ghostRect);

		pDC->SetTextColor(::GetSysColor(COLOR_GRAYTEXT));
	}
	else
	{
		pDC->SetTextColor(::GetSysColor(COLOR_BTNTEXT));
	}

	// Find teh rect that we should draw the text
	CRect textrect = lpdis->rcItem;
	if(lpdis->itemState & ODS_SELECTED)
		textrect.OffsetRect(1,1);

	// Draw the text
	this->DrawCenterText(pDC, (LPCSTR)text, textrect);

	// Draw focus box
	if( (lpdis->itemState & ODS_FOCUS) && 
		!(lpdis->itemState & ODS_DISABLED))
	{
		CRect rect = lpdis->rcItem;
		rect.InflateRect(-3, -3);
		pDC->DrawFocusRect(rect);
	}
}

void CODNetscapeButton::DrawItem(LPDRAWITEMSTRUCT lpdis)
{
	CDC dc;
	dc.Attach (lpdis->hDC);
	CRect rect = lpdis->rcItem;

	this->Draw3DBox(&dc, rect, (lpdis->itemState & ODS_SELECTED));
	this->DrawContent(&dc, lpdis);

	dc.Detach();	
}
//////////////////////////////////////////////////////////////////////////
//
//	CODMochaListBox
//
//////////////////////////////////////////////////////////////////////////
// Helper routine used shared by list box and combo box code
static void
DrawTheItem(LPDRAWITEMSTRUCT lpdis, int16 wincsid, LO_TextAttr* text_attr, MWContext* m_Context)
{
#ifdef _WIN32
	::FillRect(lpdis->hDC, &lpdis->rcItem, lpdis->itemState & ODS_SELECTED ?
		(HBRUSH)(COLOR_HIGHLIGHT + 1) : (HBRUSH)(COLOR_WINDOW + 1));
#else
	HBRUSH	hBrush;
	 
	// Paint the item background
	hBrush = ::CreateSolidBrush(::GetSysColor(lpdis->itemState & ODS_SELECTED ?
		COLOR_HIGHLIGHT : COLOR_WINDOW));
	::FillRect(lpdis->hDC, &lpdis->rcItem, hBrush);
	::DeleteObject(hBrush);
#endif

	//	Measure what the size of the button should be.
	if (lpdis->itemID != (UINT)-1) {
		CyaFont	*pMyFont;
		CXDC(m_Context)->SelectNetscapeFont( lpdis->hDC, text_attr, pMyFont );
		//pMyFont->PrepareDrawText(lpdis->hDC);
	
		::SetTextColor(lpdis->hDC, ::GetSysColor(lpdis->itemState & ODS_SELECTED ?
			COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
		::SetBkColor(lpdis->hDC, ::GetSysColor(lpdis->itemState & ODS_SELECTED ? 
			COLOR_HIGHLIGHT : COLOR_WINDOW ));

		CIntlWin::TextOutWithCyaFont(pMyFont, wincsid, lpdis->hDC, 
			2 + lpdis->rcItem.left, lpdis->rcItem.top,
			(LPCSTR)lpdis->itemData, XP_STRLEN((char*)lpdis->itemData));

		//pMyFont->EndDrawText(lpdis->hDC);
		CXDC(m_Context)->ReleaseNetscapeFont( lpdis->hDC, pMyFont );
	}
	
	if (lpdis->itemState & ODS_FOCUS)
		::DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
}

void CODMochaListBox::MeasureItem(LPMEASUREITEMSTRUCT lpmis)
{
	CyaFont	*pMyFont;
	CDC *pDC = GetDC();
	HDC hDC = pDC->GetSafeHdc();
	CXDC(m_Context)->SelectNetscapeFont( hDC, m_Form->text_attr, pMyFont );
    lpmis->itemHeight = pMyFont->GetHeight();

	CXDC(m_Context)->ReleaseNetscapeFont( hDC, pMyFont );
	ReleaseDC(pDC);
}

void CODMochaListBox::DrawItem(LPDRAWITEMSTRUCT lpdis)
{
	LO_LockLayout();
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(m_Context);
	DrawTheItem(lpdis, INTL_GetCSIWinCSID(csi), m_Form->text_attr, m_Context);
	LO_UnlockLayout();
}

//////////////////////////////////////////////////////////////////////////
//
//	CODMochaComboBox
//
//////////////////////////////////////////////////////////////////////////

void CODMochaComboBox::MeasureItem(LPMEASUREITEMSTRUCT lpmis)
{
	CyaFont	*pMyFont;
	CDC *pDC = GetDC();
	CXDC(m_Context)->SelectNetscapeFont( pDC->GetSafeHdc(), m_Form->text_attr, pMyFont );
    lpmis->itemHeight = pMyFont->GetHeight();


	CXDC(m_Context)->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );
	ReleaseDC(pDC);

}

void CODMochaComboBox::DrawItem(LPDRAWITEMSTRUCT lpdis)
{
	LO_LockLayout();
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(m_Context);
	DrawTheItem(lpdis, INTL_GetCSIWinCSID(csi), m_Form->text_attr, m_Context);
	LO_UnlockLayout();
}


//////////////////////////////////////////////////////////////////////////
//
//	CUTF8NetscapeEdit
//
//////////////////////////////////////////////////////////////////////////
#define ResizeBuffer(b,l,nl)	\
	if((nl) > (l))	{			\
		(b) = ((b) == NULL) ? ((unsigned char*) malloc(      ((l)=(((nl) < 512) ? 512 : (nl)))))	\
							: ((unsigned char*) realloc((b), ((l)=(((nl) < 512) ? 512 : (nl)))));	\
	}




CODNetscapeEdit::CODNetscapeEdit()
{
	m_set_text_wincsid = 0;
	m_convertedBufSize = m_ucs2BufSize = m_gettextBufSize = 0;
	m_ucs2Buf = m_convertedBuf = m_gettextBuf = NULL;
}
CODNetscapeEdit::~CODNetscapeEdit()
{
	if(m_convertedBuf != NULL)
	{
		free(m_convertedBuf);
		m_convertedBuf = NULL;
		m_convertedBufSize = 0;
	}
	if(m_ucs2Buf != NULL)
	{
		free(m_ucs2Buf);
		m_ucs2Buf=NULL;
		m_ucs2BufSize = 0;
	}
	if(m_gettextBuf != NULL)
	{
		free(m_gettextBuf);
		m_gettextBuf=NULL;
		m_gettextBufSize = 0;
	}
}


void	
CODNetscapeEdit::GetWindowText(CString& rString)
{

	if( (CIntlWin::UseUnicodeFontAPI(GetSetWindowTextCSID())) || 
	    (sysInfo.m_bWinNT))
	{
		int nLen = GetWindowTextLength();
		GetWindowText (rString.GetBufferSetLength(nLen), nLen+1);
		rString.ReleaseBuffer();
	}
	else
	{
		CNetscapeEdit::GetWindowText(rString);
	}
}
int	
CODNetscapeEdit::GetWindowTextLength()
{

	if( (CIntlWin::UseUnicodeFontAPI(GetSetWindowTextCSID())) || 
	    (sysInfo.m_bWinNT))
		return GetWindowText(NULL, 0);
	else	
		return CNetscapeEdit::GetWindowTextLength();
}

#ifdef XP_WIN32

void		
CODNetscapeEdit::SetWindowTextW(HWND hWnd, LPCWSTR lpString)
{
	::SendMessageW(hWnd, WM_SETTEXT, (WPARAM) 0, (LPARAM) lpString);
}

int			
CODNetscapeEdit::GetWindowTextW(HWND hWnd, LPWSTR lpString, int nMaxCount)
{
	if(nMaxCount > 0)
		return ::SendMessageW(hWnd, WM_GETTEXT, (WPARAM) nMaxCount, (LPARAM) lpString);
	else
		return ::SendMessageW(hWnd, WM_GETTEXTLENGTH, 0, 0);
}
#endif

void	
CODNetscapeEdit::SetWindowText(LPCTSTR lpszString)
{
	int16 wincsid = GetWinCSID();
	SetSetWindowTextCSID(wincsid);
#ifdef XP_WIN32
	if(sysInfo.m_bWinNT)
	{
		// In case it is NT, we should convert it to Unicode and use 
		// if appropriate SendMessageW
		ASSERT((strlen(lpszString) < 4) || 
				(!((lpszString[0]=='?') && (lpszString[1]=='?') && (lpszString[2]=='?') && (lpszString[3]=='?')))
				);
		Convert(wincsid, CS_UCS2, (unsigned char*)lpszString);

		SetWindowTextW(m_hWnd, (unsigned short*)m_convertedBuf);

		return;
	}
#endif

	if(CS_UTF8 == wincsid)
	{
		int16 csid = CIntlWin::GetSystemLocaleCsid();
		Convert(wincsid, csid, (unsigned char*)lpszString);
		CNetscapeEdit::SetWindowText((char*)m_convertedBuf);
	}
	else
	{
		CNetscapeEdit::SetWindowText(lpszString);
	}
}

int	
CODNetscapeEdit::GetWindowText(LPTSTR lpszStringBuf, int nMaxCount)
{
#ifdef XP_WIN32
	if(sysInfo.m_bWinNT)
	{
		// In case it is NT, we should convert it to Unicode and use 
		// if appropriate SendMessageW
		int rawLen = (::GetWindowTextLengthW(m_hWnd) + 1) * 2;
		ResizeBuffer(m_gettextBuf, m_gettextBufSize, rawLen);

		GetWindowTextW(m_hWnd, (unsigned short*)m_gettextBuf, m_gettextBufSize / 2);

		Convert(CS_UCS2, GetSetWindowTextCSID(), (unsigned char*)m_gettextBuf);
		int convertedLen = strlen((char*)m_convertedBuf);
		ASSERT((convertedLen < 4) || 
				(! ((m_convertedBuf[0]=='?') && (m_convertedBuf[1]=='?') && 
					 (m_convertedBuf[2]=='?') && (m_convertedBuf[3]=='?')))
			);
		if((nMaxCount > 0) && (lpszStringBuf != NULL))
		{
			if(nMaxCount > convertedLen)
			{
				strcpy(lpszStringBuf, (char*)m_convertedBuf);
			}
			else
			{
				/*	We should really make sure it return the whole characters */
				strncpy(lpszStringBuf, (char*)m_convertedBuf, nMaxCount-1);
				lpszStringBuf[nMaxCount-1] = '\0';
			}
		}
		return convertedLen;
	}
#endif

	if(CS_UTF8 == GetWinCSID())
	{
		int rawLen = CNetscapeEdit::GetWindowTextLength();
		if(rawLen == 0)
			return 0;
		ResizeBuffer(m_gettextBuf, m_gettextBufSize, rawLen);

		// Currently, I have problem to use GetWindowTextW to get the
		// Unicode text. Try to use DDE CF_UNICODETEXT instead
		CNetscapeEdit::GetWindowText((char*)m_gettextBuf, m_gettextBufSize);
		Convert(CIntlWin::GetSystemLocaleCsid(),GetWinCSID(), m_gettextBuf);
		int convertedLen = strlen((char*)m_convertedBuf);

		if((nMaxCount > 0) && (lpszStringBuf != NULL))
		{
			if(nMaxCount > convertedLen)
			{
				strcpy(lpszStringBuf, (char*)m_convertedBuf);
			}
			else
			{
				/*	We should really make sure it return the whole characters */
				strncpy(lpszStringBuf, (char*)m_convertedBuf, nMaxCount-1);
				lpszStringBuf[nMaxCount-1] = '\0';
			}
		}
		return convertedLen;
	}
	else
	{
		if(0 == nMaxCount)
			return CNetscapeEdit::GetWindowTextLength();
		else
			return CNetscapeEdit::GetWindowText(lpszStringBuf,nMaxCount);
	}
}

void CODNetscapeEdit::Convert(int16 from, int16 to, unsigned char* lpszString)
{
	int ucs2bufneeded;
	int ucs2len;
	int mbbufneeded;
	if((lpszString == NULL) || ((lpszString[0] == NULL) && (CS_UCS2 != from)))
	{
		ResizeBuffer(m_convertedBuf, m_convertedBufSize, 2);
		m_convertedBuf[0] = '\0';
		m_convertedBuf[1] = '\0';	// in case the to is UCS2
		return;
	}
	if(CS_UCS2 == from)
	{
		// it the from is UCS2, then we don't need to convert it
		// Just make sure the buffer is big enough and copy to it.
		ucs2len = CASTINT(INTL_UnicodeLen((INTL_Unicode*)lpszString));
		ucs2bufneeded = CASTINT((ucs2len + 1)* 2);
		ResizeBuffer(m_ucs2Buf, m_ucs2BufSize, ucs2bufneeded);
		memcpy((char*)m_ucs2Buf, (char*)lpszString, ucs2bufneeded);
		m_ucs2Buf[ucs2len*2] = '\0';	// NULL terminated UCS2 string
		m_ucs2Buf[ucs2len*2+1] = '\0';	// NULL terminated UCS2 string
	} else { 
		ucs2bufneeded = CASTINT(INTL_StrToUnicodeLen(from, lpszString) * 2);
		ResizeBuffer(m_ucs2Buf, m_ucs2BufSize, ucs2bufneeded);
		ucs2len = CASTINT(INTL_StrToUnicode(from, lpszString, (INTL_Unicode*)m_ucs2Buf, m_ucs2BufSize));
	}

	if(CS_UCS2 == to)
	{
		// it the to is UCS2, then we don't need to convert it
		// Just make sure the buffer is big enough and copy to it.
		mbbufneeded = CASTINT((ucs2len+1)*2);
		ResizeBuffer(m_convertedBuf, m_convertedBufSize, mbbufneeded);
		memcpy((char*)m_convertedBuf, (char*)m_ucs2Buf, mbbufneeded);
		m_convertedBuf[ucs2len*2] = '\0';	// NULL terminated UCS2 string
		m_convertedBuf[ucs2len*2+1] = '\0';	// NULL terminated UCS2 string

	} else {
		mbbufneeded = CASTINT(INTL_UnicodeToStrLen(to, (INTL_Unicode*)m_ucs2Buf, ucs2len));
		ResizeBuffer(m_convertedBuf, m_convertedBufSize, mbbufneeded);
		INTL_UnicodeToStr(to, (INTL_Unicode*)m_ucs2Buf, ucs2len, m_convertedBuf, m_convertedBufSize);
	}
}


int16 
CODNetscapeEdit::GetSetWindowTextCSID()
{
//	XP_ASSERT(0 != m_set_text_wincsid);	// Make sure SetWindowText is called after SetWindowText
	if(m_set_text_wincsid != CS_UNKNOWN && m_set_text_wincsid != CS_DEFAULT)
		return m_set_text_wincsid;
	else
		return INTL_DefaultWinCharSetID(0);	// temp use CS_LATIN1
}
int16 CODNetscapeEdit::GetWinCSID()
{
	XP_ASSERT(m_Context);

	if(m_Context) 
	{
		return INTL_GetCSIWinCSID(
			LO_GetDocumentCharacterSetInfo(m_Context));

	}
	else
	{
		return CS_DEFAULT;	// CS_DEFAULT == 0
	}
}

#ifdef XP_WIN32
BOOL CODNetscapeEdit::CreateEx(DWORD dwExStyle, LPCTSTR lpszClassName,
	LPCTSTR lpszWindowName, DWORD dwStyle,
	int x, int y, int nWidth, int nHeight,
	HWND hWndParent, HMENU nIDorHMenu, LPVOID lpParam)
{
	if(sysInfo.m_bWinNT)
	{	
           // Place holder for code deal with Unicode
	}
	
	return CEdit::CreateEx(dwExStyle, lpszClassName, lpszWindowName,
		dwStyle, x, y, nWidth, nHeight, hWndParent, nIDorHMenu, lpParam);
}
#endif


