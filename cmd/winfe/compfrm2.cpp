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

/* COMPFRM.CPP - Compose Window Frame.  This module contains code for the
 * compose window frame class.  Handles all menu and tool commands.
 *
 */
 
#include "stdafx.h"
#include "edt.h"
#include "compbar.h"
#include "compfrm.h"
#include "prefapi.h"
#include "addrdlg.h"
#include "intl_csi.h"

extern "C" {
#include "xpgetstr.h"
extern int MK_MSG_SAVE_AS;
extern int MK_MSG_CANT_OPEN;
extern int MK_MSG_MISSING_SUBJECT;
};

#ifdef XP_WIN32
#include "postal.h"
#endif
#include "netsvw.h"

void CComposeFrame::SetEditorParent(CWnd * pWnd)
{
   CWinCX * pWinCX = (CWinCX*)GetMainContext();

   // after the window is created, tell the browser view to resize the 
   // parent which resizes the editor.
   if (pWinCX) 
   {
      CNetscapeView * pView = (CNetscapeView*)pWinCX->GetView();
      if (pView)
         pView->SetChild (pWnd);
   }
}

void CComposeFrame::SetHtmlMode(BOOL bMode)
{
	m_bUseHtml = bMode;
}

void CComposeFrame::SetQuoteSelection(void)
{
	int32 eReplyOnTop = 0;

	if (PREF_NOERROR == PREF_GetIntPref("mailnews.reply_on_top", &eReplyOnTop) &&
			eReplyOnTop != 0) {
    // call backend with insertion call-back
		if (UseHtml()) 
		{
			int32 offset = EDT_GetInsertPointOffset( GetMainContext()->GetContext() );
			SetQuoteSel(offset);
		}
		else 
		{
			int tmpStartSel, tmpEndSel;
			GetEditor()->GetSel(tmpStartSel, tmpEndSel);
			// we only care about start position
			SetQuoteSel((int32) tmpStartSel);
		}
	}
}

CWnd * CComposeFrame::GetEditorWnd(void)
{
    if (m_bUseHtml) 
    {
    	CWinCX * pWinCX = (CWinCX*)GetMainContext();
    	HWND hwnd = pWinCX->GetPane();
    	return CWnd::FromHandle(hwnd);
    }
	return (CWnd *)&m_Editor;
}

void CComposeFrame::UpdateAttachmentInfo(void)
{
    ASSERT(m_pComposeBar);
    m_pComposeBar->UpdateAttachmentInfo(MSG_GetAttachmentList(GetMsgPane()) ? 1 : 0);
}

BOOL CComposeFrame::PreTranslateMessage( MSG* pMsg )
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_TAB)
		{
            BOOL bControl = GetKeyState(VK_CONTROL) & 0x8000;
            BOOL bShift = GetKeyState(VK_SHIFT) & 0x8000;
            CWnd * pWnd = CWnd::FromHandle(pMsg->hwnd);
            if (GetComposeBar()->TabControl(bShift, bControl, pWnd))
        		return TRUE;                    
		}
	}
    else if (pMsg->message == WM_COMMAND)
    {
        int ID = (int)LOWORD(pMsg->wParam);
        if (ID == ID_CHECK_SPELLING)
        {
            OnCheckSpelling();
            return TRUE;
        }
    }
	return CGenericFrame::PreTranslateMessage(pMsg);
}

void CComposeFrame::OnSelectAddresses()
{
    CAddrDialog AddressDialog(this);
    GetComposeBar()->OnAddressTab();
    GetComposeBar()->UpdateWindow();
    CWnd * pWnd = GetFocus();
    AddressDialog.DoModal();
    pWnd->SetFocus();
}

