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
 *   Ashish Bhatt <ashishbhatt@netscape.com> 
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

// File Overview....
// 
// Header file for nsiWebNavigation interface test cases


#if !defined(AFX_NSIWEBNAV_H__4E002235_7569_11D5_89E7_00010316305A__INCLUDED_)
#define AFX_NSIWEBNAV_H__4E002235_7569_11D5_89E7_00010316305A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


/////////////////////////////////////////////////////////////////////////////
// CNsIWebNav window

class CNsIWebNav : public CWnd
{
// Construction
public:
	CNsIWebNav(CTests *mTests);

// Attributes
public:
	CTests *qaTests ;

// Operations
public:

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

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNsIWebNav)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CNsIWebNav();

	// Generated message map functions
protected:
	//{{AFX_MSG(CNsIWebNav)
	afx_msg void OnInterfacesNsiwebnav();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
typedef struct
{
  char			theUri[1024];
  unsigned long theFlag;
} NavElement;

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NSIWEBNAV_H__4E002235_7569_11D5_89E7_00010316305A__INCLUDED_)
