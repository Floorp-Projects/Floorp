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

// editfloat.cpp : implementation file
//
#include "stdafx.h"
#include "editfloat.h"
#include "pa_tags.h"   // Needed for P_MAX
#include "edres2.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CEnderBar,CToolBar)
static UINT BASED_CODE nIDCharButtonBarArray[] =
{
	// same order as in the bitmap for toolbar
    ID_FORMAT_CHAR_BOLD,
    ID_FORMAT_CHAR_ITALIC,
    ID_FORMAT_CHAR_UNDERLINE,
    ID_CHECK_SPELLING,
    ID_SEPARATOR,
    ID_UNUM_LIST,
    ID_NUM_LIST,
    ID_FORMAT_OUTDENT,
    ID_FORMAT_INDENT,
    ID_SEPARATOR,
    ID_ALIGN_POPUP,
    ID_INSERT_POPUP
};
#define CHARBUTTONBAR_ID_COUNT ((sizeof(nIDCharButtonBarArray))/sizeof(UINT))

/////////////////////////////////////////////////////////////////////////////
// CEnderBar

CEnderBar::CEnderBar()
{
}

CEnderBar::~CEnderBar()
{
}

BOOL CEnderBar::Init(CWnd* pParentWnd, BOOL bToolTips)
{
	if (!pParentWnd)
	{
		TRACE("bad parentwnd");
		return FALSE;
	}
	m_bVertical = FALSE;

	// start out with no borders
	DWORD dwStyle = WS_CHILD|WS_VISIBLE|CBRS_TOOLTIPS|CBRS_RIGHT|CBRS_FLYBY|CBRS_SIZE_DYNAMIC;
	//if (bToolTips)
	//	dwStyle |= (CBRS_TOOLTIPS | CBRS_FLYBY);
	if (!Create(pParentWnd, dwStyle, IDS_EDIT_FLOAT_TOOLBAR)) 
	{
		return FALSE;
	}
    if (!LoadBitmap(IDB_EDIT_FLOAT_TOOLBAR))
        return FALSE;
	if(!SetButtons(nIDCharButtonBarArray, CHARBUTTONBAR_ID_COUNT))
		return FALSE;
	SetSizes(CSize(27,22),CSize(20,16));
    EnableDocking(CBRS_ALIGN_ANY);
    if(!SetVertical())
		return FALSE;
	return TRUE;
}

BOOL CEnderBar::SetHorizontal()
{
	m_bVertical = FALSE;

//	SetBarStyle(GetBarStyle() | CBRS_ALIGN_TOP);

	return TRUE;
}

BOOL CEnderBar::SetVertical()
{
	m_bVertical = TRUE;
	return TRUE;
}


CSize CEnderBar::CalcDynamicLayout(int nLength, DWORD dwMode)
{
	// if we're committing set the buttons appropriately
	if (dwMode & LM_COMMIT)
	{
		return CToolBar::CalcDynamicLayout(nLength, dwMode);
	}
	else
	{

		CSize sizeResult = CToolBar::CalcDynamicLayout(nLength, dwMode);
		return sizeResult;
	}
}


BEGIN_MESSAGE_MAP(CEnderBar, CToolBar)
	//{{AFX_MSG_MAP(CEnderBar)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CEnderBar message handlers
