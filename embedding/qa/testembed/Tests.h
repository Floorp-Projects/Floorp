/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   David Epstein <depstein@netscape.com> 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

// Tests.h : header file for QA test cases
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _TESTS_H
#define _TESTS_H

#if _MSC_VER > 1000
	#pragma once
#endif

#include "BrowserView.h"
#include "BrowserImpl.h"
#include "StdAfx.h"


/////////////////////////////////////////////////////////////////////////////
// CTESTS class

class CBrowserImpl;
class CBrowserView;

class CTests : public CWnd
{
public:
	CTests(nsIWebBrowser* mWebBrowser,
			   nsIBaseWindow* mBaseWindow,
			   nsIWebNavigation* mWebNav,
			   CBrowserImpl *mpBrowserImpl);
	virtual ~CTests();

	// Some helper methods

	// Mozilla interfaces
	//
	nsCOMPtr<nsIWebBrowser> qaWebBrowser;
	nsCOMPtr<nsIBaseWindow> qaBaseWindow;
	nsCOMPtr<nsIWebNavigation> qaWebNav;	

	CBrowserImpl	*qaBrowserImpl;

	// local test methods

	// local test variables
	nsresult rv;
	CString strMsg;
	char theUrl[200];
	char *uriSpec;
	PRBool exists;
	PRInt32 numEntries;
	PRInt32 theIndex;

	nsCOMPtr<nsIURI> theUri;

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTests)
	protected:
	//}}AFX_VIRTUAL


	// Generated message map functions
protected:
	//{{AFX_MSG(CTests)
	afx_msg void OnUpdateNavBack(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNavForward(CCmdUI* pCmdUI);
	afx_msg void OnUpdateNavStop(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCut(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCopy(CCmdUI* pCmdUI);
	afx_msg void OnUpdatePaste(CCmdUI* pCmdUI);
	afx_msg void OnTestsChangeUrl();
	afx_msg void OnTestsGlobalHistory();
	afx_msg void OnTestsCreateFile();
	afx_msg void OnTestsCreateprofile();
	afx_msg void OnTestsAddWebProgListener();
	afx_msg void OnTestsAddHistoryListener();
	afx_msg void OnInterfacesNsifile();
	afx_msg void OnInterfacesNsishistory();
	afx_msg void OnInterfacesNsiwebnav();
	afx_msg void OnInterfacesNsirequest();
	afx_msg void OnToolsRemoveGHPage();
	afx_msg void OnToolsRemoveAllGH();
	afx_msg void OnToolsTestYourMethod();
	afx_msg void OnToolsTestYourMethod2();
	afx_msg void OnVerifybugs70228();
    afx_msg void OnPasteTest();
    afx_msg void OnCopyTest();
    afx_msg void OnSelectAllTest();
    afx_msg void OnSelectNoneTest();
    afx_msg void OnCutSelectionTest();
    afx_msg void copyLinkLocationTest();
    afx_msg void canCopySelectionTest();
    afx_msg void canCutSelectionTest();
    afx_msg void canPasteTest();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	// individual nsIFile tests
	void InitWithPathTest(nsILocalFile *);
	void AppendRelativePathTest(nsILocalFile *);
	void FileCreateTest(nsILocalFile *);
	void FileExistsTest(nsILocalFile *);
	void FileCopyTest(nsILocalFile *, nsILocalFile *);
	void FileMoveTest(nsILocalFile *, nsILocalFile *);

	// individual nsISHistory tests
	void GetCountTest(nsISHistory *, PRInt32 *);
	void GetIndexTest(nsISHistory *, PRInt32 *);
	void GetMaxLengthTest(nsISHistory *, PRInt32 *);
	void SetMaxLengthTest(nsISHistory *, PRInt32);
	void GetEntryAtIndexTest(nsISHistory *, nsIHistoryEntry *, PRInt32 theIndex);
	void GetURIHistTest(nsIHistoryEntry *);
	void GetTitleHistTest(nsIHistoryEntry *);
	void GetIsSubFrameTest(nsIHistoryEntry *);
	void GetSHEnumTest(nsISHistory*, nsISimpleEnumerator *);
	void SimpleEnumTest(nsISimpleEnumerator *);
	void PurgeHistoryTest(nsISHistory *, PRInt32);

	// individual nsIWebNavigation tests
	void CanGoBackTest();
	void GoBackTest();
	void CanGoForwardTest();
	void GoForwardTest();
	void GoToIndexTest();
	void LoadUriTest(char *, const unsigned long);
	void ReloadTest(const unsigned long);
	void StopUriTest(char *);
	void GetDocumentTest(void);
	void GetCurrentURITest(void);
	void GetSHTest(void);

public:
	// individual nsIRequest tests
	void static IsPendingReqTest(nsIRequest *);
	void static GetStatusReqTest(nsIRequest *);
	void static SuspendReqTest(nsIRequest *);
	void static ResumeReqTest(nsIRequest *);
	void static CancelReqTest(nsIRequest *);
	void static SetLoadGroupTest(nsIRequest *, nsILoadGroup *);
	void static GetLoadGroupTest(nsIRequest *);
};

//	structure for uri table
typedef struct
{
	char		theUri[1024];
	bool		reqPend;
	bool		reqStatus;
	bool		reqSuspend;
	bool		reqResume;
	bool		reqCancel;
	bool		reqSetLoadGroup;
	bool		reqGetLoadGroup;	
} ReqElement;

typedef struct
{
  char			theUri[1024];
  unsigned long theFlag;
} NavElement;

#endif //_TESTS_H