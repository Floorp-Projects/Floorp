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

#if !defined(AFX_CPAGESETUPPROPSHEET_H__E8A6D703_7EAD_4729_8FBA_9E0515AB9822__INCLUDED_)
#define AFX_CPAGESETUPPROPSHEET_H__E8A6D703_7EAD_4729_8FBA_9E0515AB9822__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CPageSetupPropSheet.h : header file
//
#include "CFormatOptionTab.h"
#include "CMarginHeaderFooter.h"

/////////////////////////////////////////////////////////////////////////////
// CPageSetupPropSheet

class CPageSetupPropSheet : public CPropertySheet
{
	DECLARE_DYNAMIC(CPageSetupPropSheet)

// Construction
public:
	CPageSetupPropSheet(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CPageSetupPropSheet(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

  void SetPrintSettingsValues(nsIPrintSettings* aPrintSettings);
  void GetPrintSettingsValues(nsIPrintSettings* aPrintSettings);

protected:
	void AddControlPages(void);

// Attributes
public:
	CFormatOptionTab     m_FormatOptionTab;
	CMarginHeaderFooter  m_MarginHeaderFooterTab;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPageSetupPropSheet)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CPageSetupPropSheet();

	// Generated message map functions
protected:
	//{{AFX_MSG(CPageSetupPropSheet)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CPAGESETUPPROPSHEET_H__E8A6D703_7EAD_4729_8FBA_9E0515AB9822__INCLUDED_)
