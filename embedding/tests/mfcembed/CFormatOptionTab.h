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

#if !defined(AFX_CFORMATOPTIONTAB_H__F7BDB355_9202_440A_8478_165AD3FC2F41__INCLUDED_)
#define AFX_CFORMATOPTIONTAB_H__F7BDB355_9202_440A_8478_165AD3FC2F41__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CFormatOptionTab.h : header file
//

#include "nsIPrintSettings.h"

/////////////////////////////////////////////////////////////////////////////
// CFormatOptionTab dialog

class CFormatOptionTab : public CPropertyPage
{
	DECLARE_DYNCREATE(CFormatOptionTab)

// Construction
public:
	CFormatOptionTab();
	~CFormatOptionTab();
  void GetPaperSizeInfo(short& aType, double& aWidth, double& aHeight);

// Dialog Data
	//{{AFX_DATA(CFormatOptionTab)
	enum { IDD = IDD_FORMAT_OPTIONS_TAB };
	int		m_PaperSize;
	BOOL	m_BGColors;
	int		m_Scaling;
	CString	m_ScalingText;
	int		m_DoInches;
	BOOL	m_BGImages;
	double	m_PaperHeight;
	double	m_PaperWidth;
	BOOL	m_IsLandScape;
	//}}AFX_DATA

  nsCOMPtr<nsIPrintSettings> m_PrintSettings;
  int                        m_PaperSizeInx;

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CFormatOptionTab)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  void EnableUserDefineControls(BOOL aEnable);
  int  GetPaperSizeIndexFromData(short aType, double aW, double aH);
  int  GetPaperSizeIndex(const CString& aStr);

	// Generated message map functions
	//{{AFX_MSG(CFormatOptionTab)
	virtual BOOL OnInitDialog();
	afx_msg void OnCustomdrawScale(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKillfocusScaleTxt();
	afx_msg void OnSelchangePaperSizeCbx();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CFORMATOPTIONTAB_H__F7BDB355_9202_440A_8478_165AD3FC2F41__INCLUDED_)
