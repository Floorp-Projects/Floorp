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
#ifndef _EDITFLOAT_H
#define _EDITFLOAT_H

class CEnderBar : public CToolBar
{
// Construction
public:
    DECLARE_DYNAMIC(CEnderBar)
	CEnderBar();
	BOOL Init(CWnd* pParentWnd, BOOL bToolTips);
	BOOL SetColor(BOOL bColor);
	BOOL SetHorizontal();
	BOOL SetVertical();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEnderBar)
	//}}AFX_VIRTUAL

private:
	BOOL m_bVertical;
	CPoint m_location;
// Implementation
public:

	virtual ~CEnderBar();
//	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
	virtual CSize CalcDynamicLayout(int nLength, DWORD dwMode);

	// Generated message map functions
protected:
	//{{AFX_MSG(CEnderBar)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif //_EDITFLOAT_H
