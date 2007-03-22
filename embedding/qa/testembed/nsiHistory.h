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

// File Overview....
// 
// Header file for nsiHistory interface test cases

#include "nsIHistoryEntry.h"

/////////////////////////////////////////////////////////////////////////////
// CNsIHistory window

class CNsIHistory
{
// Construction
public:
	CNsIHistory(nsIWebNavigation *mWebNav);

// Attributes
public:
	// Mozilla interfaces
	//
	nsCOMPtr<nsIWebNavigation> qaWebNav;
	nsCOMPtr<nsIURI> theURI;
	//CTests *qaTests ;

public:

// Implementation
public:
	virtual ~CNsIHistory();

public:

	// individual nsISHistory tests
	void GetCountTest(nsISHistory *, PRInt32 *, PRInt16);
	void GetIndexTest(nsISHistory *, PRInt32 *, PRInt16);
	void GetMaxLengthTest(nsISHistory *, PRInt32 *, PRInt16);
	void SetMaxLengthTest(nsISHistory *, PRInt32, PRInt16);
	void GetEntryAtIndexTest(nsISHistory *, nsIHistoryEntry *, PRInt32 theIndex, PRInt16);
	void GetURIHistTest(nsIHistoryEntry *, PRInt16);
	void GetTitleHistTest(nsIHistoryEntry *, PRInt16);
	void GetIsSubFrameTest(nsIHistoryEntry *, PRInt16);
	void GetSHEnumTest(nsISHistory*, nsISimpleEnumerator *, PRInt16);
	void SimpleEnumTest(nsISimpleEnumerator *, PRInt16);
	void PurgeHistoryTest(nsISHistory *, PRInt32, PRInt16);
	void RunAllTests();
	void OnStartTests(UINT nMenuID);
	void RunAllHistoryEntryTests(PRInt16);
	// Generated message map functions
protected:

};



/////////////////////////////////////////////////////////////////////////////
