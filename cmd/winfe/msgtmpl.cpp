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
#include "msgtmpl.h"
#include "mailfrm.h"
#include "thrdfrm.h"
#include "msgfrm.h"
#include "prefapi.h"

void CMsgTemplate::_InitialUpdate(CFrameWnd *pFrame, CFrameWnd *pPrevFrame,
								  BOOL bMakeVisible, 
								  LPSTR lpszPosPref, LPSTR lpszShowPref)
{
	int32 prefInt;
	int16 left, top, width, height;
	PREF_GetIntPref(lpszShowPref, &prefInt);
	PREF_GetRectPref(lpszPosPref, &left, &top, &width, &height);

	POINT ptPos;
	SIZE size;
	int show = SW_NORMAL;

    ptPos.x = left;
    ptPos.y = top;
    size.cx = width;
    size.cy = height;
	show = (prefInt == SW_MAXIMIZE) ? SW_MAXIMIZE : 0;

    if ( pPrevFrame ) {
		WINDOWPLACEMENT wp;
		wp.length = sizeof(WINDOWPLACEMENT);
		pPrevFrame->GetWindowPlacement(&wp);
		
		if ( wp.showCmd == SW_MAXIMIZE ) {
            // Previous frame is maximized, 
            //  so show the new frame in maximized state
            show = SW_MAXIMIZE;                    
        }

		CRect rectLast = wp.rcNormalPosition;

        int titleY = GetSystemMetrics(SM_CYSIZE) + GetSystemMetrics(SM_CYFRAME);

        ptPos.x = rectLast.left + titleY;
        ptPos.y = rectLast.top + titleY;
		size.cx = rectLast.right - rectLast.left;
		size.cy = rectLast.bottom - rectLast.top;
	}	

	if (ptPos.x != -1) {
		int sx = GetSystemMetrics(SM_CXFULLSCREEN);
		int sy = GetSystemMetrics(SM_CYFULLSCREEN);

		// Sanity check size
		if (size.cx > sx)
			size.cx = sx;
		if (size.cx < 300)
			size.cx = 300;
		if (size.cy > sy)
			size.cy = sy;
		if (size.cy < 300)
			size.cy = 300;

		// Sanity check position
		if (ptPos.x + size.cx > sx)
			ptPos.x = 0;
		if (ptPos.x < 0)
			ptPos.x = 0;
		if (ptPos.y + size.cy > sy)
			ptPos.y = 0;
		if (ptPos.y < 0)
			ptPos.y = 0;

		WINDOWPLACEMENT wp;
		memset(&wp, 0, sizeof(WINDOWPLACEMENT));
		wp.length = sizeof(WINDOWPLACEMENT);
		pFrame->GetWindowPlacement(&wp);

		wp.showCmd = show;
		::SetRect(&wp.rcNormalPosition, ptPos.x, ptPos.y,
				  ptPos.x + size.cx, ptPos.y + size.cy);

		pFrame->SetWindowPlacement(&wp);
	}
	pFrame->ActivateFrame(SW_SHOW);
}

void CFolderTemplate::InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
									     BOOL bMakeVisible)
{
	_InitialUpdate(pFrame, NULL, bMakeVisible,
				   "mailnews.folder_window_rect",
				   "mailnews.folder_window_showwindow");

	CMsgTemplate::InitialUpdateFrame(pFrame, pDoc, bMakeVisible);
}

void CThreadTemplate::InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
									   BOOL bMakeVisible)
{
	CFrameWnd *pPrevFrame = CMailNewsFrame::GetLastThreadFrame(pFrame);
	_InitialUpdate(pFrame, pPrevFrame, bMakeVisible,
				   "mailnews.thread_window_rect",
				   "mailnews.thread_window_showwindow");

	CMsgTemplate::InitialUpdateFrame(pFrame, pDoc, bMakeVisible);
}

void CMessageTemplate::InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
									    BOOL bMakeVisible)
{
    CFrameWnd *pPrevFrame = CMailNewsFrame::GetLastMessageFrame(pFrame);
	_InitialUpdate(pFrame, pPrevFrame, bMakeVisible,
				   "mailnews.message_window_rect",
				   "mailnews.message_window_showwindow");

	CMsgTemplate::InitialUpdateFrame(pFrame, pDoc, bMakeVisible);
}

