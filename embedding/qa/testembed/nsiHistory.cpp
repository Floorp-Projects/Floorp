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
// Test cases for the nsiHistory Interface

#include "stdafx.h"
#include "TestEmbed.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "UrlDialog.h"
#include "ProfileMgr.h"
#include "ProfilesDlg.h"
#include "QaUtils.h"
#include "nsihistory.h"
#include <stdio.h>


/////////////////////////////////////////////////////////////////////////////
// CNsIHistory

CNsIHistory::CNsIHistory(nsIWebNavigation *mWebNav)
{
	//qaTests = mTests;
	qaWebNav = mWebNav ;
}


CNsIHistory::~CNsIHistory()
{
}



/////////////////////////////////////////////////////////////////////////////
// CNsIHistory message handlers
// ***********************************************************************
// ***********************************************************************
//  nsIHistory iface

// ***********************************************************************
// ***********************************************************************
// nsISHistory & nsIHistoryEntry ifaces:

void CNsIHistory::OnStartTests(UINT nMenuID)
{

	// Calls  all or indivdual test cases on the basis of the 
	// option selected from menu.
   PRInt32 numEntries = 5;
   PRInt32 theIndex;
   PRInt32 theMaxLength = 100;

   CString shString;

   nsCOMPtr<nsISHistory> theSessionHistory;
   nsCOMPtr<nsIHistoryEntry> theHistoryEntry;
   nsCOMPtr<nsISimpleEnumerator> theSimpleEnum;


   //nsCOMPtr<nsIURI> theURI;
   // do_QueryInterface
   // NS_HISTORYENTRY_CONTRACTID
   // NS_SHISTORYLISTENER_CONTRACTID

	// get Session History through web nav iface
   if (qaWebNav) {
	  rv = qaWebNav->GetSessionHistory(getter_AddRefs(theSessionHistory));
	  RvTestResult(rv, "GetSessionHistory() object", 1);
	  RvTestResultDlg(rv, "GetSessionHistory() object", true);
   }

   if (!theSessionHistory)
   {
	   QAOutput("theSessionHistory object wasn't created. No session history tests performed.", 2);
	   return;
   }
   else {
	   QAOutput("theSessionHistory object was created.", 1);
	   theSessionHistory->GetCount(&numEntries);
   }

	theSessionHistory->GetEntryAtIndex(0, PR_FALSE, getter_AddRefs(theHistoryEntry));
	if (!theHistoryEntry)
		QAOutput("We didn't get the History Entry object.", 1);
	else 
		QAOutput("We have the History Entry object!", 1);	



	switch(nMenuID)
	{
		case ID_INTERFACES_NSISHISTORY_RUNALLTESTS :
			QAOutput("Begin nsISHistory tests.", 1);
			RunAllTests();
			QAOutput("End nsISHistory tests.", 1);
			break ;
		case ID_INTERFACES_NSISHISTORY_GETCOUNT :
			GetCountTest(theSessionHistory, &numEntries, 2);
			break ;
		case ID_INTERFACES_NSISHISTORY_GETINDEX :
			GetIndexTest(theSessionHistory, &theIndex, 2);
			break ;
		case ID_INTERFACES_NSISHISTORY_GETMAXLENGTH :
			GetMaxLengthTest(theSessionHistory, &theMaxLength, 2);
			break ;
		case ID_INTERFACES_NSISHISTORY_SETMAXLENGTH :
			SetMaxLengthTest(theSessionHistory, theMaxLength, 2);
			break ;
		case ID_INTERFACES_NSISHISTORY_GETENTRYATINDEX :
			//GetEntryAtIndex(0, PR_FALSE, getter_AddRefs(theHistoryEntry));
			for (theIndex = 0; theIndex < numEntries; theIndex++)
			{ 
				FormatAndPrintOutput("the index = ", theIndex, 1); 

				rv = theSessionHistory->GetEntryAtIndex(theIndex, PR_FALSE, getter_AddRefs(theHistoryEntry));
				RvTestResult(rv, "GetEntryAtIndex() test", 1);
				RvTestResultDlg(rv, "GetEntryAtIndex() test");
				if (!theHistoryEntry)
				{
					QAOutput("We didn't get the History Entry object. No more tests performed.", 1);
					return;
				}
				GetURIHistTest(theHistoryEntry, 2);
				GetTitleHistTest(theHistoryEntry, 2);
				GetIsSubFrameTest(theHistoryEntry, 2);
			}	// end for loop

			break ;
		case ID_INTERFACES_NSISHISTORY_PURGEHISTORY :
			PurgeHistoryTest(theSessionHistory, numEntries, 2);
			break ;
		case ID_INTERFACES_NSISHISTORY_GETSHISTORYENUMERATOR :
			//GetSHistoryEnumerator(getter_AddRefs(theSimpleEnum));

			rv = theSessionHistory->GetSHistoryEnumerator(getter_AddRefs(theSimpleEnum));
			RvTestResult(rv, "GetSHistoryEnumerator() (SHistoryEnumerator attribute) test", 1);
			RvTestResultDlg(rv, "GetSHistoryEnumerator() test");

			if (!theSimpleEnum) {
  			   QAOutput("theSimpleEnum for GetSHistoryEnumerator() invalid. Test failed.", 1);
			   return;
			}
			SimpleEnumTest(theSimpleEnum, 2);
			break ;

		case ID_INTERFACES_NSISHISTORY_NSIHISTORYENTRY_RUNALLTESTS :
			RunAllHistoryEntryTests(2);
			break ;
		case ID_INTERFACES_NSISHISTORY_NSIHISTORYENTRY_GETURI :
			GetURIHistTest(theHistoryEntry, 2);
			break ;
		case ID_INTERFACES_NSISHISTORY_NSIHISTORYENTRY_GETTITLE :
			GetTitleHistTest(theHistoryEntry, 2);
			break ;
		case ID_INTERFACES_NSISHISTORY_NSIHISTORYENTRY_GETISSUBFRAME :
			GetIsSubFrameTest(theHistoryEntry, 2);
			break ;
	}

}

void CNsIHistory::RunAllTests() 
{
   //nsresult rv;  

   PRInt32 numEntries = 5;
   PRInt32 theIndex;
   PRInt32 theMaxLength = 100;

   CString shString;

   nsCOMPtr<nsISHistory> theSessionHistory;
   nsCOMPtr<nsIHistoryEntry> theHistoryEntry;

	// get Session History through web nav iface
   if (qaWebNav)
		qaWebNav->GetSessionHistory( getter_AddRefs(theSessionHistory));

   if (!theSessionHistory)
   {
	   QAOutput("theSessionHistory object wasn't created. No session history tests performed.", 2);
	   return;
   }
   else {
	   QAOutput("theSessionHistory object was created.", 1);
	   theSessionHistory->GetCount(&numEntries);
   }

		// test count attribute in nsISHistory.idl
   GetCountTest(theSessionHistory, &numEntries, 1);

		// test index attribute in nsISHistory.idl
   GetIndexTest(theSessionHistory, &theIndex, 1);

		// test maxLength attribute in nsISHistory.idl
   SetMaxLengthTest(theSessionHistory, theMaxLength, 1);
   GetMaxLengthTest(theSessionHistory, &theMaxLength, 1);

	QAOutput("Start nsiHistoryEntry tests.", 1); 

		// get theHistoryEntry object
	rv = theSessionHistory->GetEntryAtIndex(0, PR_FALSE, getter_AddRefs(theHistoryEntry));
	RvTestResult(rv, "GetEntryAtIndex() test", 1);
	RvTestResultDlg(rv, "GetEntryAtIndex() test");

	if (!theHistoryEntry)
		QAOutput("We didn't get the History Entry object.", 1);
	else 
	{
		QAOutput("We have the History Entry object!", 1);	

			    // getEntryAtIndex() tests
		if (theMaxLength < numEntries)
		{
			QAOutput("Setting number of entries to maximum length!", 1);
			numEntries = theMaxLength;
		}

		for (theIndex = 0; theIndex < numEntries; theIndex++)
		{ 
			FormatAndPrintOutput("the index = ", theIndex, 1); 

			//GetEntryAtIndexTest(theSessionHistory,theHistoryEntry, theIndex);

			theSessionHistory->GetEntryAtIndex(theIndex, PR_FALSE, getter_AddRefs(theHistoryEntry));
			if (!theHistoryEntry)
			{
				QAOutput("We didn't get the History Entry object. No more tests performed.", 1);
				return;
			}
			// nsiHistoryEntry.idl tests	

			// test URI attribute in nsIHistoryEntry.idl
			GetURIHistTest(theHistoryEntry, 1);

			// test title attribute in nsIHistoryEntry.idl
			GetTitleHistTest(theHistoryEntry, 1);

			// test isSubFrame attribute in nsIHistoryEntry.idl
			GetIsSubFrameTest(theHistoryEntry, 1);

		}	// end for loop
	}		// end outer else


	// test SHistoryEnumerator attribute in nsISHistory.idl
	nsCOMPtr<nsISimpleEnumerator> theSimpleEnum;

//	GetSHEnumTest(theSessionHistory, theSimpleEnum);

	rv = theSessionHistory->GetSHistoryEnumerator(getter_AddRefs(theSimpleEnum));
	RvTestResult(rv, "GetSHistoryEnumerator() (SHistoryEnumerator attribute) test", 1);
	RvTestResultDlg(rv, "GetSHistoryEnumerator() (SHistoryEnumerator attribute) test");

	if (!theSimpleEnum)
  	   QAOutput("theSimpleEnum for GetSHistoryEnumerator() invalid. Test failed.", 1);
	else
		SimpleEnumTest(theSimpleEnum, 1);

	// PurgeHistory() test

	PurgeHistoryTest(theSessionHistory, numEntries, 1);
}

// ***********************************************************************
// Individual nsISHistory tests

void CNsIHistory::GetCountTest(nsISHistory *theSessionHistory, PRInt32 *numEntries,
							   PRInt16 displayMode)
{
    rv = theSessionHistory->GetCount(numEntries);
	if (*numEntries < 0) 
		QAOutput("numEntries for GetCount() < 0. Test failed.", 1);
	else {
		FormatAndPrintOutput("GetCount():number of entries = ", *numEntries, displayMode);
		RvTestResult(rv, "GetCount() (count attribute) test", 1);
		RvTestResultDlg(rv, "GetCount() (count attribute) test");
	}
}

void CNsIHistory::GetIndexTest(nsISHistory *theSessionHistory, PRInt32 *theIndex,
							   PRInt16 displayMode)
{
	rv = theSessionHistory->GetIndex(theIndex);
	if (*theIndex <0) 
		QAOutput("theIndex for GetIndex() < 0. Test failed.", 1);
	else {
		FormatAndPrintOutput("GetIndex():the index = ", *theIndex, displayMode);
		RvTestResult(rv, "GetIndex() (index attribute) test", 1);
		RvTestResultDlg(rv, "GetIndex() (index attribute) test");
	}
}

void CNsIHistory::SetMaxLengthTest(nsISHistory *theSessionHistory, PRInt32 theMaxLength,
								   PRInt16 displayMode)
{
	rv = theSessionHistory->SetMaxLength(theMaxLength);
	if (theMaxLength < 0)
		QAOutput("theMaxLength for SetMaxLength() < 0. Test failed.", 1);
	else {
		RvTestResult(rv, "SetMaxLength() (MaxLength attribute) test", displayMode);
		RvTestResultDlg(rv, "SetMaxLength() (MaxLength attribute) test");
	}
}

void CNsIHistory::GetMaxLengthTest(nsISHistory *theSessionHistory, PRInt32 *theMaxLength,
								   PRInt16 displayMode)
{
	rv = theSessionHistory->GetMaxLength(theMaxLength);
	if (*theMaxLength<0)
		QAOutput("theMaxLength for GetMaxLength() < 0. Test failed.", 1);
	else {
		FormatAndPrintOutput("GetMaxLength():theMaxLength = ", *theMaxLength, displayMode); 
		RvTestResult(rv, "GetMaxLength() (MaxLength attribute) test", 1);
		RvTestResultDlg(rv, "GetMaxLength() (MaxLength attribute) test");
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


void CNsIHistory::GetURIHistTest(nsIHistoryEntry* theHistoryEntry,
								 PRInt16 displayMode)
{
	rv = theHistoryEntry->GetURI(getter_AddRefs(theURI));
	RvTestResult(rv, "GetURI() (URI attribute) test", 1);
	RvTestResultDlg(rv, "GetURI() (URI attribute) test");
	if (!theURI)
		QAOutput("theURI for GetURI() invalid. Test failed.", 1);
	else
	{
		nsCAutoString uriString;
		rv = theURI->GetSpec(uriString);
		if (NS_FAILED(rv))
			QAOutput("We didn't get the uriString.", 1);
		else
			FormatAndPrintOutput("The SH Url = ", uriString, displayMode);
	}
}

void CNsIHistory::GetTitleHistTest(nsIHistoryEntry* theHistoryEntry,
								   PRInt16 displayMode)
{
   nsXPIDLString theTitle;

	rv = theHistoryEntry->GetTitle(getter_Copies(theTitle));
	RvTestResult(rv, "GetTitle() (title attribute) test", 1);
	RvTestResultDlg(rv, "GetTitle() (title attribute) test");
	if (!theTitle) {
		QAOutput("theTitle for GetTitle() is blank. Test failed.", 1);
		return;
	}

	FormatAndPrintOutput("The title = ",
                        NS_ConvertUCS2toUTF8(theTitle).get(), displayMode);
}

void CNsIHistory::GetIsSubFrameTest(nsIHistoryEntry* theHistoryEntry,
									PRInt16 displayMode)
{
	PRBool isSubFrame;

	rv = theHistoryEntry->GetIsSubFrame(&isSubFrame);
	
	RvTestResult(rv, "GetIsSubFrame() (isSubFrame attribute) test", 1);
	RvTestResultDlg(rv, "GetIsSubFrame() (isSubFrame attribute) test");
	FormatAndPrintOutput("The subFrame boolean value = ", isSubFrame, displayMode);
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

void CNsIHistory::SimpleEnumTest(nsISimpleEnumerator *theSimpleEnum,
								 PRInt16 displayMode)
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
	 rv = nextHistoryEntry->GetURI(getter_AddRefs(theURI));
	 RvTestResult(rv, "theSimpleEnum nsIHistoryEntry->GetURI() test", 1);
	 RvTestResultDlg(rv, "theSimpleEnum nsIHistoryEntry->GetURI() test");
	 nsCAutoString uriString;
	 rv = theURI->GetSpec(uriString);
	 if (NS_FAILED(rv))
		QAOutput("uriString for GetSpec() invalid. Test failed.", 1);
	 else
		FormatAndPrintOutput("The SimpleEnum URL = ", uriString, displayMode);		 
  } 
}

void CNsIHistory::PurgeHistoryTest(nsISHistory* theSessionHistory, PRInt32 numEntries,
								   PRInt16 displayMode)
{
   rv = theSessionHistory->PurgeHistory(numEntries);
   RvTestResult(rv, "PurgeHistory() test", 1);
   RvTestResultDlg(rv, "PurgeHistory() test");
   FormatAndPrintOutput("PurgeHistory(): num requested entries for removal = ", numEntries, 1);		 
}

void CNsIHistory::RunAllHistoryEntryTests(PRInt16 displayMode) 
{
   PRInt32 numEntries = 5;
   PRInt32 theIndex;
   PRInt32 theMaxLength = 100;

   CString shString;

   nsCOMPtr<nsISHistory> theSessionHistory;
   nsCOMPtr<nsIHistoryEntry> theHistoryEntry;

	// get Session History through web nav iface
   if (qaWebNav)
		qaWebNav->GetSessionHistory( getter_AddRefs(theSessionHistory));

   if (!theSessionHistory)
   {
	   QAOutput("theSessionHistory object wasn't created. No session history tests performed.", displayMode);
	   return;
   }
   else
	   QAOutput("theSessionHistory object was created.", 1);

	theSessionHistory->GetEntryAtIndex(0, PR_FALSE, getter_AddRefs(theHistoryEntry));
	if (!theHistoryEntry)
		QAOutput("We didn't get the History Entry object.", 1);
	else 
	{
		QAOutput("We have the History Entry object!", 1);	

			    // getEntryAtIndex() tests
		if (theMaxLength < numEntries)
		{
			QAOutput("Setting number of entries to maximum length!", 1);
			numEntries = theMaxLength;
		}

		for (theIndex = 0; theIndex < numEntries; theIndex++)
		{ 
			FormatAndPrintOutput("the index = ", theIndex, displayMode); 

			//GetEntryAtIndexTest(theSessionHistory,theHistoryEntry, theIndex);

			theSessionHistory->GetEntryAtIndex(theIndex, PR_FALSE, getter_AddRefs(theHistoryEntry));
			if (!theHistoryEntry)
			{
				QAOutput("We didn't get the History Entry object. No more tests performed.", 1);
				return;
			}
			// nsiHistoryEntry.idl tests	

			// test URI attribute in nsIHistoryEntry.idl
			GetURIHistTest(theHistoryEntry, displayMode);

			// test title attribute in nsIHistoryEntry.idl
			GetTitleHistTest(theHistoryEntry, displayMode);

			// test isSubFrame attribute in nsIHistoryEntry.idl
			GetIsSubFrameTest(theHistoryEntry, displayMode);

		}	// end for loop
	}		// end outer else
}
