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
 *   Ashish Bhatt <ashishbhatt@netscape.com> 
 */

// File Overview....
// 
// Header file for nsiHistory interface test cases


/////////////////////////////////////////////////////////////////////////////
// CNsIHistory window

class CNsIHistory : public CWnd
{
// Construction
public:
	CNsIHistory(CTests *mTests);

// Attributes
public:
	// Mozilla interfaces
	//
	//nsCOMPtr<nsIWebNavigation> qaWebNav;
	//nsCOMPtr<nsIURI> theUri;
	char *uriSpec;
	CTests *qaTests ;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNsIHistory)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CNsIHistory();

public:

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


	// Generated message map functions
protected:
	//{{AFX_MSG(CNsIHistory)
	afx_msg void OnInterfacesNsishistory();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};



/////////////////////////////////////////////////////////////////////////////
