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
