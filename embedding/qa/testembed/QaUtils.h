/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Epstein <depstein@netscape.com> 
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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


