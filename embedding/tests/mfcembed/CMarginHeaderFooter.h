/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rod Spears <rods@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
