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
#include "prtypes.h"
#include "tooltip.h"

#ifndef XP_WIN32
#ifdef FEATURE_TOOLTIPS
#include "tooltip.i01"
#endif
#else

CNSToolTip2::CNSToolTip2()
{
	hasCustomColors = FALSE;
}


CNSToolTip2::~CNSToolTip2()
{
}


LRESULT CNSToolTip2::WindowProc( UINT message, WPARAM wParam, LPARAM lParam )
{

	if(sysInfo.m_bOverrideWin95Tooltips)
	{
		switch(message)
		{
			case TTM_ADDTOOL:
			case TTM_DELTOOL:
			case TTM_ENUMTOOLS:
			case TTM_GETCURRENTTOOL:
			case TTM_GETTEXT:
			case TTM_GETTOOLINFO:
			case TTM_NEWTOOLRECT:
			case TTM_SETTOOLINFO:
			case TTM_UPDATETIPTEXT:
				((LPTOOLINFO)lParam)->cbSize = (3 * sizeof(UINT)) + sizeof(HWND) + sizeof(RECT) + sizeof(HINSTANCE)
					+ sizeof(LPSTR);
				break;

			case TTM_HITTEST:
				((LPHITTESTINFO)lParam)->ti.cbSize = (3 * sizeof(UINT)) + sizeof(HWND) + sizeof(RECT) + sizeof(HINSTANCE)
					+ sizeof(LPSTR);
				break;
		}
	}
	return CToolTipCtrl::WindowProc(message, wParam, lParam);


}

BEGIN_MESSAGE_MAP(CNSToolTip2, CToolTipCtrl)
	//{{AFX_MSG_MAP(CNSToolTip2)
	ON_WM_CTLCOLOR_REFLECT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CNSToolTip2::SetCSID(int csid)
{
}

void CNSToolTip2::SetBounding(int *coord, int num, int x, int y)
{
}

HBRUSH CNSToolTip2::CtlColor ( CDC* pDC, UINT nCtlColor )
{
	if (hasCustomColors)
	{
		pDC->SetTextColor( m_ForegroundColor );    // text
		pDC->SetBkColor( m_BackgroundColor );    // text bkgnd
		return m_BackgroundBrush;            // ctl bkgnd
	}
	return NULL;
}
#endif
 
