/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 *   David Epstein <depstein@netscape.com> 
 */

// QAUtils.h : Global function declarations
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _QAUTILS_H
#define _QAUTILS_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "BrowserView.h"
#include "resource.h"
#include "stdafx.h"

extern void RvTestResultDlg(nsresult rv, CString pLine,BOOL bClearList = false);
extern void RvTestResult(nsresult, const char *, int displayMethod=1);
extern void WriteToOutputFile(const char *);
extern void QAOutput(const char *pLine, int displayMethod=1);
extern void FormatAndPrintOutput(const char *, const char *, int);
extern void FormatAndPrintOutput(const char *, int, int);
extern void RequestName(nsIRequest *, nsCString &, int displayMethod=1);
extern void WebProgDOMWindowTest(nsIWebProgress *, const char *,int displayMethod=1);
extern void GetTheUri(nsIURI *theUri, int displayMethod);
extern nsresult rv;

#endif //_QAUTILS_H/////////////////////////////////////////////////////////////////////////////



// CShowTestResults dialog
class CShowTestResults : public CDialog
{
// Construction
public:
	CShowTestResults(CWnd* pParent = NULL);   // standard constructor
	void AddItemToList(LPCTSTR szTestCaseName, BOOL bResult);

// Dialog Data
	//{{AFX_DATA(CShowTestResults)
	enum { IDD = IDD_RUNTESTSDLG };
	CListCtrl	m_ListResults;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CShowTestResults)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
private:
	LPCTSTR m_TitleString ;
protected:

	// Generated message map functions
	//{{AFX_MSG(CShowTestResults)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


