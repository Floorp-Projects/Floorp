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
// Test cases for the nsiHistory Interface

#include "stdafx.h"
#include "TestEmbed.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "UrlDialog.h"
#include "ProfileMgr.h"
#include "ProfilesDlg.h"
#include "QaUtils.h"
#include "tests.h"
#include <stdio.h>


/////////////////////////////////////////////////////////////////////////////
// CNsIHistory

CNsIHistory::CNsIHistory(CTests *mTests)
{
	qaTests = mTests;
	//qaWebNav = mWebNav ;
}


CNsIHistory::~CNsIHistory()
{
}


BEGIN_MESSAGE_MAP(CNsIHistory, CWnd)
	//{{AFX_MSG_MAP(CNsIHistory)
	ON_COMMAND(ID_INTERFACES_NSISHISTORY, OnInterfacesNsishistory)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CNsIHistory message handlers
// ***********************************************************************
// ***********************************************************************
//  nsIHistory iface

// ***********************************************************************
// ***********************************************************************
// nsISHistory & nsIHistoryEntry ifaces:

void CNsIHistory::OnInterfacesNsishistory() 
{
   //nsresult rv;  

   PRInt32 numEntries = 5;
   PRInt32 theIndex;
   PRInt32 theMaxLength = 100;

   CString shString;

   nsCOMPtr<nsISHistory> theSessionHistory;
   nsCOMPtr<nsIHistoryEntry> theHistoryEntry;

   //nsCOMPtr<nsIURI> theUri;
   // do_QueryInterface
   // NS_HISTORYENTRY_CONTRACTID
   // NS_SHISTORYLISTENER_CONTRACTID

	// get Session History through web nav iface
   if (qaTests->qaWebNav)
		qaTests->qaWebNav->GetSessionHistory( getter_AddRefs(theSessionHistory));

   if (!theSessionHistory)
   {
	   QAOutput("theSessionHistory object wasn't created. No session history tests performed.", 2);
	   return;
   }
   else
	   QAOutput("theSessionHistory object was created.", 2);


		// test count attribute in nsISHistory.idl
   GetCountTest(theSessionHistory, &numEntries);

		// test index attribute in nsISHistory.idl
   GetIndexTest(theSessionHistory, &theIndex);

		// test maxLength attribute in nsISHistory.idl
   SetMaxLengthTest(theSessionHistory, theMaxLength);
   GetMaxLengthTest(theSessionHistory, &theMaxLength);

	QAOutput("Start nsiHistoryEntry tests.", 2); 

		// get theHistoryEntry object
	theSessionHistory->GetEntryAtIndex(0, PR_FALSE, getter_AddRefs(theHistoryEntry));
	if (!theHistoryEntry)
		QAOutput("We didn't get the History Entry object.", 1);
	else 
	{
		QAOutput("We have the History Entry object!", 1);	

			    // getEntryAtIndex() tests
		for (theIndex = 0; theIndex < numEntries; theIndex++)
		{ 
			FormatAndPrintOutput("the index = ", theIndex, 2); 

			//GetEntryAtIndexTest(theSessionHistory,theHistoryEntry, theIndex);

			theSessionHistory->GetEntryAtIndex(theIndex, PR_FALSE, getter_AddRefs(theHistoryEntry));
			// nsiHistoryEntry.idl tests	

			// test URI attribute in nsIHistoryEntry.idl
			GetURIHistTest(theHistoryEntry);

			// test title attribute in nsIHistoryEntry.idl
			GetTitleHistTest(theHistoryEntry);

			// test isSubFrame attribute in nsIHistoryEntry.idl
			GetIsSubFrameTest(theHistoryEntry);

		}	// end for loop
	}		// end outer else


	// test SHistoryEnumerator attribute in nsISHistory.idl
	nsCOMPtr<nsISimpleEnumerator> theSimpleEnum;

//	GetSHEnumTest(theSessionHistory, theSimpleEnum);

	rv = theSessionHistory->GetSHistoryEnumerator(getter_AddRefs(theSimpleEnum));
	if (!theSimpleEnum)
  	   QAOutput("theSimpleEnum for GetSHistoryEnumerator() invalid. Test failed.", 1);
	else
	   RvTestResult(rv, "GetSHistoryEnumerator() (SHistoryEnumerator attribute) test", 2);

	SimpleEnumTest(theSimpleEnum);

	// PurgeHistory() test

	PurgeHistoryTest(theSessionHistory, numEntries);
}

// ***********************************************************************
// Individual nsISHistory tests

void CNsIHistory::GetCountTest(nsISHistory *theSessionHistory, PRInt32 *numEntries)
{
    rv = theSessionHistory->GetCount(numEntries);
	if (*numEntries < 0)
		QAOutput("numEntries for GetCount() < 0. Test failed.", 1);
	else {
		FormatAndPrintOutput("number of entries = ", *numEntries, 2);
		RvTestResult(rv, "GetCount() (count attribute) test", 2);
	}
}

void CNsIHistory::GetIndexTest(nsISHistory *theSessionHistory, PRInt32 *theIndex)
{
	rv = theSessionHistory->GetIndex(theIndex);
	if (*theIndex <0)
		QAOutput("theIndex for GetIndex() < 0. Test failed.", 1);
	else
		RvTestResult(rv, "GetIndex() (index attribute) test", 2);
}

void CNsIHistory::SetMaxLengthTest(nsISHistory *theSessionHistory, PRInt32 theMaxLength)
{
	rv = theSessionHistory->SetMaxLength(theMaxLength);
	RvTestResult(rv, "SetMaxLength() (MaxLength attribute) test", 2);
}

void CNsIHistory::GetMaxLengthTest(nsISHistory *theSessionHistory, PRInt32 *theMaxLength)
{
	rv = theSessionHistory->GetMaxLength(theMaxLength);
	if (*theMaxLength<0)
		QAOutput("theMaxLength for GetMaxLength() < 0. Test failed.", 1);
	else {
		FormatAndPrintOutput("theMaxLength = ", *theMaxLength, 2); 
		RvTestResult(rv, "GetMaxLength() (MaxLength attribute) test", 2);
	}
}


/*void CNsIHistory::GetEntryAtIndexTest(nsISHistory *theSessionHistory, 
								 nsIHistoryEntry *theHistoryEntry, 
								 PRInt32 theIndex)
{
	theSessionHistory->GetEntryAtIndex(theIndex, PR_FALSE,&theHistoryEntry);

	// test URI attribute in nsIHistoryEntry.idl
	//GetURIHistTest(theHistoryEntry);

	// test title attribute in nsIHistoryEntry.idl
	//GetTitleHistTest(theHistoryEntry);

	// test isSubFrame attribute in nsIHistoryEntry.idl
	//GetIsSubFrameTest(theHistoryEntry);

}*/


void CNsIHistory::GetURIHistTest(nsIHistoryEntry* theHistoryEntry)
{
	rv = theHistoryEntry->GetURI(getter_AddRefs(qaTests->theUri));
	if (!qaTests->theUri)
		QAOutput("theUri for GetURI() invalid. Test failed.", 1);
	else
	{
		RvTestResult(rv, "GetURI() (URI attribute) test", 1);
		rv = qaTests->theUri->GetSpec(&uriSpec);
		if (NS_FAILED(rv))
			QAOutput("We didn't get the uriSpec.", 1);
		else
			FormatAndPrintOutput("The SH Url = ", uriSpec, 2);
	}
}

void CNsIHistory::GetTitleHistTest(nsIHistoryEntry* theHistoryEntry)
{
   nsXPIDLString theTitle;
   const char *  titleCString;

	rv = theHistoryEntry->GetTitle(getter_Copies(theTitle));
	if (!theTitle) {
		QAOutput("theTitle for GetTitle() is blank. Test failed.", 1);
		return;
	}
	else
		RvTestResult(rv, "GetTitle() (title attribute) test", 1);
	titleCString = NS_ConvertUCS2toUTF8(theTitle).get();
	FormatAndPrintOutput("The title = ", (char *)titleCString, 2);

}

void CNsIHistory::GetIsSubFrameTest(nsIHistoryEntry* theHistoryEntry)
{
	PRBool isSubFrame;

	rv = theHistoryEntry->GetIsSubFrame(&isSubFrame);
	
	RvTestResult(rv, "GetIsSubFrame() (isSubFrame attribute) test", 1);
	FormatAndPrintOutput("The subFrame boolean value = ", isSubFrame, 2);
}

/*
void CNsIHistory::GetSHEnumTest(nsISHistory *theSessionHistory,
						   nsISimpleEnumerator *theSimpleEnum)
{
	rv = theSessionHistory->GetSHistoryEnumerator(getter_AddRefs(theSimpleEnum));
	if (!theSimpleEnum)
  	   QAOutput("theSimpleEnum for GetSHistoryEnumerator() invalid. Test failed.", 1);
	else
	   RvTestResult(rv, "GetSHistoryEnumerator() (SHistoryEnumerator attribute) test", 2);
}
*/

void CNsIHistory::SimpleEnumTest(nsISimpleEnumerator *theSimpleEnum)
{
  PRBool bMore = PR_FALSE;
  nsCOMPtr<nsISupports> nextObj;
  nsCOMPtr<nsIHistoryEntry> nextHistoryEntry;

  while (NS_SUCCEEDED(theSimpleEnum->HasMoreElements(&bMore)) && bMore)
  {
	 theSimpleEnum->GetNext(getter_AddRefs(nextObj));
	 if (!nextObj)
		continue;
	 nextHistoryEntry = do_QueryInterface(nextObj);
	 if (!nextHistoryEntry)
		continue;
	 rv = nextHistoryEntry->GetURI(getter_AddRefs(qaTests->theUri));
	 rv = qaTests->theUri->GetSpec(&uriSpec);
	 if (!uriSpec)
		QAOutput("uriSpec for GetSpec() invalid. Test failed.", 1);
	 else
		FormatAndPrintOutput("The SimpleEnum URL = ", uriSpec, 2);		 
  } 
}

void CNsIHistory::PurgeHistoryTest(nsISHistory* theSessionHistory, PRInt32 numEntries)
{
   rv = theSessionHistory->PurgeHistory(numEntries);
   RvTestResult(rv, "PurgeHistory() test", 2);
   FormatAndPrintOutput("Number of removed entries = ", numEntries, 2);		 
}
