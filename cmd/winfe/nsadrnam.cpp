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

// NSAdrNam.cpp : implementation file
//

#include "stdafx.h"

#include "nsadrlst.h"
#include "nsadrnam.h"
#include "addrbook.h"

/////////////////////////////////////////////////////////////////////////////
// CNSAddressNameEditField

CNSAddressNameEditField::CNSAddressNameEditField()
{
    m_pIAddressParent = NULL;
	m_bNameCompletion = FALSE;
	m_bAttemptNameCompletion = TRUE;
	m_bSetTimerForCompletion = FALSE;
	m_uTypedownClock = 0;
}

CNSAddressNameEditField::~CNSAddressNameEditField()
{
}


BOOL CNSAddressNameEditField::Create( CWnd *pParent )
{
	return CGenericEdit::Create(ES_MULTILINE|ES_WANTRETURN|ES_LEFT|ES_AUTOHSCROLL, CRect(0,0,0,0), pParent, (UINT)IDC_STATIC );
}


BEGIN_MESSAGE_MAP(CNSAddressNameEditField, CGenericEdit)
	//{{AFX_MSG_MAP(CNSAddressNameEditField)
    ON_WM_KILLFOCUS()
	ON_WM_TIMER ()
	ON_WM_CHAR()
	ON_WM_CREATE()
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDBLCLK()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CNSAddressNameEditField message handlers


BOOL CNSAddressNameEditField::PreTranslateMessage( MSG* pMsg )
{
	if (pMsg->message == WM_KEYDOWN)
	{
        if (pMsg->wParam == 'v' || pMsg->wParam == 'V')
        {
            if (GetKeyState(VK_CONTROL)&0x8000)
            {
                Paste();
                return TRUE;
            }
        } 
        else if (pMsg->wParam == VK_INSERT)
        {
            if (GetKeyState(VK_SHIFT)&0x8000)
            {
                Paste();
                return TRUE;
            }
            else if (GetKeyState(VK_CONTROL)&0x8000)
            {
                Copy();
                return TRUE;
            }
        }
        else if (pMsg->wParam == 'x' || pMsg->wParam == 'X')
        {
            if (GetKeyState(VK_CONTROL)&0x8000)
            {
                Cut();
                return TRUE;
            }
        } 
        else if (pMsg->wParam == 'c' || pMsg->wParam == 'C')
        {
            if (GetKeyState(VK_CONTROL)&0x8000)
            {
                Copy();
                return TRUE;
            }
        } 
	}
	return CGenericEdit::PreTranslateMessage(pMsg);
}

void CNSAddressNameEditField::OnKillFocus(CWnd * pNewWnd) 
{
    if (GetParent()->SendMessage(WM_NOTIFYSELECTIONCHANGE))
		CGenericEdit::OnKillFocus(pNewWnd);
	else
		SetSel(0, -1);
}

void CNSAddressNameEditField::DrawNameCompletion()
{
    if (m_pIAddressParent)
    {
        CString cs;
        GetWindowText(cs);
        char * pName = m_pIAddressParent->NameCompletion((char*)((const char *)cs));
		CDC * pDC = GetDC();
		UINT iPre = cs.GetLength();
		CFont * pOldFont = pDC->SelectObject(CFont::FromHandle(m_cfTextFont));
		int iPixOffset = pDC->GetTextExtent(cs,iPre).cx;
		CRect rect;
		GetClientRect(rect);
		rect.left = iPixOffset;
        if (pName)
		{
			m_bNameCompletion = TRUE;
			COLORREF crOld = pDC->SetTextColor(RGB(150,150,150));
            COLORREF crOldBk = pDC->SetBkColor(GetSysColor(COLOR_WINDOW));
			if (iPre < strlen(pName))
				pDC->DrawText(&pName[iPre],strlen(&pName[iPre]),rect,DT_VCENTER|DT_LEFT);
			pDC->SetTextColor(crOld);
            pDC->SetBkColor(crOldBk);
			free(pName);
		}
		else
		{  
			m_bNameCompletion = FALSE;
			InvalidateRect(rect);
		}
		pDC->SelectObject(pOldFont);
		ReleaseDC(pDC);
    }
}


#define NAME_COMPLETION_TIMER        12
#define COMPLETION_SPEED			 750

void CNSAddressNameEditField::OnTimer(UINT nID)
{
	if (nID == NAME_COMPLETION_TIMER) {
		KillTimer(m_uTypedownClock);
		m_uTypedownClock = 0;
		DrawNameCompletion();
	}
}


void CNSAddressNameEditField::OnChar( UINT nChar, UINT nRepCnt, UINT nFlags ) 
{
    switch (nChar)
    {
        case VK_ESCAPE:
            return;
        case VK_DELETE:
            break;
        default:
        	if ( ((CNSAddressList *)GetParent())->OnKeyPress( this, nChar, nRepCnt, nFlags ) )
                return;
    }
    CGenericEdit::OnChar( nChar, nRepCnt, nFlags );
	if (m_bAttemptNameCompletion)
	{
		if (m_bSetTimerForCompletion)
		{
			if (m_uTypedownClock) {
				KillTimer(m_uTypedownClock);
			}
			m_uTypedownClock = SetTimer(NAME_COMPLETION_TIMER, COMPLETION_SPEED, NULL);
		}
		else
			DrawNameCompletion();
	}
}

int CNSAddressNameEditField::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CGenericEdit::OnCreate(lpCreateStruct) == -1)
		return -1;

   uint32 count;
   DIR_Server* pab = NULL;

   DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
   if (pab) {
		AB_GetEntryCount(pab, theApp.m_pABook, &count, ABTypeAll, NULL);
		if (count > 1500)
			m_bSetTimerForCompletion = TRUE;
   }
	return 0;
}

void CNSAddressNameEditField::SetCSID(int16 iCSID) 
{
	if(m_hWnd)
	{
		CDC * pdc = GetDC();
		if (m_cfTextFont) {
			theApp.ReleaseAppFont(m_cfTextFont);
		}
		LOGFONT lf;			
		memset(&lf,0,sizeof(LOGFONT));

		lf.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
		strcpy(lf.lfFaceName, IntlGetUIFixFaceName(iCSID));
		lf.lfHeight = -MulDiv(NS_ADDRESSFONTSIZE,pdc->GetDeviceCaps(LOGPIXELSY),72);
		lf.lfQuality = PROOF_QUALITY;
		lf.lfCharSet = IntlGetLfCharset(iCSID); 
		m_cfTextFont = theApp.CreateAppFont( lf );
		if (!(lf.lfPitchAndFamily & FIXED_PITCH))
				m_bAttemptNameCompletion = FALSE;
		::SendMessage(GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfTextFont, FALSE);
		TEXTMETRIC tm;
		pdc->GetTextMetrics(&tm);
		m_iTextHeight = tm.tmHeight;

		ReleaseDC(pdc);
	}
}
void CNSAddressNameEditField::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch (nChar) 
	{
		case VK_HOME: 
			{
				int nStart, nEnd;
				GetSel(nStart,nEnd);
				if ((!nStart)&&(!nEnd)) 
					((CNSAddressList *)GetParent())->OnKeyPress(this,nChar,nRepCnt,nFlags); 
			}
			break;
		case VK_END: 
			{
				int nStart, nEnd;
				GetSel(nStart,nEnd);
				if (nStart == nEnd)
					if (nStart == LineLength())
						((CNSAddressList *)GetParent())->OnKeyPress(this,nChar,nRepCnt,nFlags); 
			}
			break;
		case VK_UP:
		case VK_DOWN:
			((CNSAddressList *)GetParent())->OnKeyPress(this,nChar,nRepCnt,nFlags); 
			break;
        case VK_DELETE:
            {
                CString cs;
                GetWindowText(cs);
                if (!cs.GetLength())
                {
        			((CNSAddressList *)GetParent())->OnKeyPress(this,nChar,nRepCnt,nFlags); 
                    return;
                }
            }
            break;
	}
	CGenericEdit::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CNSAddressNameEditField::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	SetSel(0,-1);
}

