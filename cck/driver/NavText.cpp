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

// NavText.cpp : implementation file
//

#include "stdafx.h"
#include "WizardMachine.h"
#include "NavText.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNavText

CNavText::CNavText()
{
	/**
	m_pNavFont = new CFont; 
	m_pNavFont->CreateFont(8, 0, 0, 0, FW_BOLD,
					0, 0, 0, ANSI_CHARSET,
					OUT_DEFAULT_PRECIS,
					CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY,
					DEFAULT_PITCH|FF_DONTCARE,
					"MS Sans Serif");

	this->SetFont(m_pNavFont);
	**/
	//m_textColor = RGB(0,0,255);
}

CNavText::~CNavText()
{
}


BEGIN_MESSAGE_MAP(CNavText, CStatic)
	//{{AFX_MSG_MAP(CNavText)
	ON_WM_CTLCOLOR_REFLECT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNavText message handlers

HBRUSH CNavText::CtlColor(CDC* pDC, UINT nCtlColor)
{
	// TODO: Change any attributes of the DC here
	pDC->SetTextColor(RGB(0,0,255));

	return (HBRUSH) (RGB(0,0,255));
}
