/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
