/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Rod Spears <rods@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined(AFX_CMARGINHEADERFOOTER_H__A95A9A17_692E_425B_8B70_419C24DDE6BC__INCLUDED_)
#define AFX_CMARGINHEADERFOOTER_H__A95A9A17_692E_425B_8B70_419C24DDE6BC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CMarginHeaderFooter.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMarginHeaderFooter dialog

class CMarginHeaderFooter : public CPropertyPage
{
	DECLARE_DYNCREATE(CMarginHeaderFooter)

// Construction
public:
	CMarginHeaderFooter();
	~CMarginHeaderFooter();

// Dialog Data
	//{{AFX_DATA(CMarginHeaderFooter)
	enum { IDD = IDD_HEADERFOOTER_TAB };
	CString	m_BottomMarginText;
	CString	m_LeftMarginText;
	CString	m_RightMarginText;
	CString	m_TopMarginText;
	//}}AFX_DATA

	CString	m_FooterLeftText;
	CString	m_FooterCenterText;
	CString	m_FooterRightText;
	CString	m_HeaderLeftText;
	CString	m_HeaderCenterText;
	CString	m_HeaderRightText;

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CMarginHeaderFooter)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  void SetCombobox(int aId, CString& aText);
  void SetComboboxValue(int aId, const TCHAR * aValue);
  void AddCBXItem(int aId, const TCHAR * aItem);

	// Generated message map functions
	//{{AFX_MSG(CMarginHeaderFooter)
	virtual BOOL OnInitDialog();
	afx_msg void OnEditchangeFTRLeft();
	afx_msg void OnEditchangeFTRCenter();
	afx_msg void OnEditchangeFTRRight();
	afx_msg void OnEditchangeHDRLeft();
	afx_msg void OnEditchangeHDRCenter();
	afx_msg void OnEditchangeHDRRight();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MARGINHEADERFOOTER_H__A95A9A17_692E_425B_8B70_419C24DDE6BC__INCLUDED_)
