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
// Test cases for the nsiWebNavigation Interface



#include "stdafx.h"
#include "testembed.h"
#include "qautils.h"
#include "nsiwebnav.h"
#include "UrlDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNsIWebNav 

CNsIWebNav::CNsIWebNav(nsIWebNavigation *mWebNav)
{
	qaWebNav = mWebNav ;
}

CNsIWebNav::~CNsIWebNav()
{
}


/////////////////////////////////////////////////////////////////////////////
// CNsIWebNav message handlers
// ***********************************************************************
// nsIWebNavigation iface
// ***********************************************************************

// Url table for web navigation
NavElement UrlTable[] = {
   {"http://www.intel.com/", nsIWebNavigation::LOAD_FLAGS_NONE},
   {"http://www.yahoo.com/", nsIWebNavigation::LOAD_FLAGS_MASK},
   {"http://www.oracle.com/", nsIWebNavigation::LOAD_FLAGS_IS_LINK},
   {"http://www.sun.com/", nsIWebNavigation::LOAD_FLAGS_IS_REFRESH},
   {"ftp://ftp.netscape.com", nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY},
   {"ftp://ftp.mozilla.org/", nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY},
   {"https://www.motorola.com/", nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE},
   {"https://www.amazon.com", nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY},
   {"about:plugins", nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE},
   {"javascript: document.write('Test!')", nsIWebNavigation::LOAD_FLAGS_NONE},
   {"file://C|/Program Files", nsIWebNavigation::LOAD_FLAGS_NONE}
};


void CNsIWebNav::OnStartTests(UINT nMenuID)
{
	switch(nMenuID)
	{
		case ID_INTERFACES_NSIWEBNAV_RUNALLTESTS :
			RunAllTests();
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETCANGOBACK :
		    CanGoBackTest(2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETCANGOFORWARD :
			CanGoForwardTest(2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_GOBACK  :
			GoBackTest(2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_GOFORWARD :
			GoForwardTest(2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_GOTOINDEX :
			GoToIndexTest(2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_LOADURI :
			LoadURITest(nsnull, nsnull, 2, PR_FALSE);
			break ;
		case ID_INTERFACES_NSIWEBNAV_RELOAD  :
			ReloadTest(nsIWebNavigation::LOAD_FLAGS_NONE, 2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_STOP    :
			StopURITest("file://C|/Program Files",
						 nsIWebNavigation::STOP_CONTENT, 2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETDOCUMENT :
			GetDocumentTest(2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETCURRENTURI :
			GetCurrentURITest(2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETREFERRINGURI:
			GetReferringURITest(2);
			break;
		case ID_INTERFACES_NSIWEBNAV_GETSESSIONHISTORY :
			GetSHTest(2);
			break ;
		case ID_INTERFACES_NSIWEBNAV_SETSESSIONHISTORY :
			SetSHTest(2);
			break ;
	}
}

void CNsIWebNav::RunAllTests()
{
   int i=0;

   if (qaWebNav)
	   QAOutput("We have the web nav object.", 1);
   else {
	   QAOutput("We don't have the web nav object. No tests performed.", 2);
	   return;
   }

   // load a couple of URLs to get things going
	LoadURITest("http://www.cisco.com", nsIWebNavigation::LOAD_FLAGS_NONE, 2, PR_TRUE);
	LoadURITest("www.google.com", nsIWebNavigation::LOAD_FLAGS_NONE, 2, PR_TRUE);
	
   // canGoBack attribute test
   CanGoBackTest(1);

   // GoBack test
   GoBackTest(2);

   // canGoForward attribute test
   CanGoForwardTest(1);

   // GoForward test
   GoForwardTest(2);

   // GotoIndex test
   GoToIndexTest(2);

   // LoadURI() & reload tests

   QAOutput("Run a few LoadURI() tests.", 1);

 	
   LoadUriandReload(11);

 
	// Stop() tests
   StopURITest("http://www.microsoft.com", nsIWebNavigation::STOP_ALL, 1);
   StopURITest("https://www.microsoft.com/", nsIWebNavigation::STOP_NETWORK, 1);
   StopURITest("ftp://ftp.microsoft.com/", nsIWebNavigation::STOP_CONTENT, 1);

   // document test
   GetDocumentTest(1);
   
   // uri test
   GetCurrentURITest(1);
   GetReferringURITest(1);

   // session history test
   SetSHTest(1);
   GetSHTest(1);
}

void CNsIWebNav::LoadUriandReload(int URItotal)
{
   int i=0, j=0;
   // LoadURI() & reload tests

   QAOutput("Run a few LoadURI() and Reload() tests.", 1);

   for (j=0; j < 9; j++) 
   {
	   for (i=0; i < URItotal; i++)
	   {
		   LoadURITest(UrlTable[i].theURI, UrlTable[j].theFlag, 2, PR_TRUE);
		   switch (i)
		   {
		   case 0:
			   ReloadTest(nsIWebNavigation::LOAD_FLAGS_NONE, 1);
			   break;
		   case 1:
			   ReloadTest(nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE, 1);
			   break;
		   case 2:
			   ReloadTest(nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY, 1);
			   break;
		   // simulate shift-reload
		   case 3:
			   ReloadTest(nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
						  nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY, 1);
			   break;
		   case 4:
			   ReloadTest(nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE, 1);
			   break;
		   case 5:
			   ReloadTest(nsIWebNavigation::LOAD_FLAGS_NONE, 1);
			   break;
		   } 
	   }
   }
}

// ***********************************************************************
// Individual nsIWebNavigation tests


void CNsIWebNav::CanGoBackTest(PRInt16 displayMode)
{
   PRBool canGoBack = PR_FALSE;
   rv =  qaWebNav->GetCanGoBack(&canGoBack);
   RvTestResult(rv, "GetCanGoBack() attribute test", displayMode);
   FormatAndPrintOutput("canGoBack value = ", canGoBack, displayMode);
}

void CNsIWebNav::GoBackTest(PRInt16 displayMode)
{
   rv =  qaWebNav->GoBack();
   RvTestResult(rv, "GoBack() test", displayMode);
}

void CNsIWebNav::CanGoForwardTest(PRInt16 displayMode)
{
   PRBool canGoForward = PR_FALSE;
   rv =  qaWebNav->GetCanGoForward(&canGoForward);
   RvTestResult(rv, "GetCanGoForward() attribute test", displayMode);
   FormatAndPrintOutput("canGoForward value = ", canGoForward, displayMode); 
}

void CNsIWebNav::GoForwardTest(PRInt16 displayMode)
{
   rv =  qaWebNav->GoForward();
   RvTestResult(rv, "GoForward() test", displayMode);
}

void CNsIWebNav::GoToIndexTest(PRInt16 displayMode)
{
   PRInt32 theIndex = 0;

   rv =  qaWebNav->GotoIndex(theIndex);
   RvTestResult(rv, "GotoIndex() test", displayMode);
}

void CNsIWebNav::LoadURITest(char *theUrl, PRUint32 theFlag,
							 PRInt16 displayMode, PRBool runAllTests)
{
   char theTotalString[500];
   char theFlagName[200];

   if (runAllTests == PR_FALSE)	// load just one url from Url dialog
   {
	  CUrlDialog myDialog;
      if (myDialog.DoModal() == IDOK)
	  {
		QAOutput("Begin Change URL test.", 1);
		rv = qaWebNav->LoadURI(NS_ConvertASCIItoUCS2(myDialog.m_urlfield).get(),
								myDialog.m_flagvalue, nsnull,nsnull, nsnull);

	    RvTestResult(rv, "rv LoadURI() test", 1);
		FormatAndPrintOutput("The url = ", myDialog.m_urlfield, displayMode);
		FormatAndPrintOutput("The flag = ", myDialog.m_flagvalue, displayMode);
		QAOutput("End Change URL test.", 1);
	  }
	  return;
   }

   switch(theFlag)
   {
   case nsIWebNavigation::LOAD_FLAGS_NONE:
	   strcpy(theFlagName, "LOAD_FLAGS_NONE");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_MASK:
	   strcpy(theFlagName, "LOAD_FLAGS_MASK");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_IS_REFRESH:
	   strcpy(theFlagName, "LOAD_FLAGS_IS_REFRESH");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_IS_LINK:
	   strcpy(theFlagName, "LOAD_FLAGS_IS_LINK");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY:
	   strcpy(theFlagName, "LOAD_FLAGS_BYPASS_HISTORY");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY:
	   strcpy(theFlagName, "LOAD_FLAGS_REPLACE_HISTORY");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE:
	   strcpy(theFlagName, "LOAD_FLAGS_BYPASS_CACHE");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY:
	   strcpy(theFlagName, "LOAD_FLAGS_BYPASS_PROXY");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE:
	   strcpy(theFlagName, "LOAD_FLAGS_CHARSET_CHANGE");
	   break;
   }

   rv = qaWebNav->LoadURI(NS_ConvertASCIItoUCS2(theUrl).get(), 
                          theFlag,
                          nsnull,
                          nsnull,
                          nsnull);
   sprintf(theTotalString, "%s%s%s%s%s", "LoadURI(): ", theUrl, " w/ ", theFlagName, " test");
   RvTestResult(rv, theTotalString, displayMode);
}

void CNsIWebNav::ReloadTest(PRUint32 theFlag, PRInt16 displayMode)
{
   char theTotalString[500];
   char theFlagName[200];

  switch(theFlag)
  {
  case nsIWebNavigation::LOAD_FLAGS_NONE:
      strcpy(theFlagName, "LOAD_FLAGS_NONE");
      break;
  case nsIWebNavigation::LOAD_FLAGS_MASK:
      strcpy(theFlagName, "LOAD_FLAGS_MASK");
      break;
  case nsIWebNavigation::LOAD_FLAGS_IS_REFRESH:
      strcpy(theFlagName, "LOAD_FLAGS_IS_REFRESH");
      break;
  case nsIWebNavigation::LOAD_FLAGS_IS_LINK:
      strcpy(theFlagName, "LOAD_FLAGS_IS_LINK");
      break;
  case nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY:
      strcpy(theFlagName, "LOAD_FLAGS_BYPASS_HISTORY");
      break;
  case nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY:
      strcpy(theFlagName, "LOAD_FLAGS_REPLACE_HISTORY");
      break;
  case nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE:
      strcpy(theFlagName, "LOAD_FLAGS_BYPASS_CACHE");
      break;
  case nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY:
      strcpy(theFlagName, "LOAD_FLAGS_BYPASS_PROXY");
      break;
  case nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE:
      strcpy(theFlagName, "LOAD_FLAGS_CHARSET_CHANGE");
      break;
  case nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY:
      strcpy(theFlagName, "cache & proxy");
      break;
  } 

   rv =  qaWebNav->Reload(theFlag);
   sprintf(theTotalString, "%s%s%s%s", "Reload(): ", " w/ ", theFlagName, " test");
   RvTestResult(rv, theTotalString, displayMode);
}

void CNsIWebNav::StopURITest(char *theUrl, PRUint32 theFlag,
							 PRInt16 displayMode)
{
   char theTotalString[200];
   char flagString[100];

   if (theFlag == nsIWebNavigation::STOP_ALL)
	   strcpy(flagString, "STOP_ALL");
   else if (theFlag == nsIWebNavigation::STOP_NETWORK)
	   strcpy(flagString, "STOP_NETWORK");
   else
	   strcpy(flagString, "STOP_CONTENT");

   qaWebNav->LoadURI(NS_ConvertASCIItoUCS2(theUrl).get(), 
                     nsIWebNavigation::LOAD_FLAGS_NONE,
                     nsnull,
                     nsnull,
                     nsnull);

   rv = qaWebNav->Stop(theFlag);
   sprintf(theTotalString, "%s%s%s%s", "Stop(): ", theUrl, " test: ", flagString);
   RvTestResult(rv, theTotalString, displayMode);
}

void CNsIWebNav::GetDocumentTest(PRInt16 displayMode)
{
   nsCOMPtr<nsIDOMDocument> theDocument;
   nsCOMPtr<nsIDOMDocumentType> theDocType;
   
   rv =  qaWebNav->GetDocument(getter_AddRefs(theDocument));
   if (!theDocument) {
	  QAOutput("We didn't get the document. Test failed.", 2);
	  return;
   }
   else
	  RvTestResult(rv, "GetDocument() test", displayMode);

   rv = theDocument->GetDoctype(getter_AddRefs(theDocType));
   RvTestResult(rv, "nsIDOMDocument::GetDoctype() for nsIWebNav test", displayMode);
}

void CNsIWebNav::GetCurrentURITest(PRInt16 displayMode)
{
   nsCOMPtr<nsIURI> theURI;

   rv =  qaWebNav->GetCurrentURI(getter_AddRefs(theURI));
   if (!theURI) {
      QAOutput("We didn't get the URI. Test failed.", 2);
	  return;
   }
   else
	  RvTestResult(rv, "GetCurrentURI() test", displayMode);

   nsCAutoString uriString;
   rv = theURI->GetSpec(uriString);
   RvTestResult(rv, "nsIURI::GetSpec() for nsIWebNav test", 1);

   FormatAndPrintOutput("the nsIWebNav uri = ", uriString, displayMode);
}

void CNsIWebNav::GetReferringURITest(PRInt16 displayMode)
{
   nsCOMPtr<nsIURI> theURI;
   nsCAutoString uriString;
   CUrlDialog myDialog;
   if (myDialog.DoModal() == IDOK)
   {
	  uriString = myDialog.m_urlfield;
	  rv = NS_NewURI(getter_AddRefs(theURI), uriString);
	  if (theURI)
		 QAOutput("We GOT the URI.", 1);
	  else
		 QAOutput("We DIDN'T GET the URI.", 1);
	  rv = qaWebNav->GetReferringURI(getter_AddRefs(theURI));
	  RvTestResult(rv, "GetReferringURI() test", displayMode);
//	  rv = qaWebNav->LoadURI(NS_ConvertASCIItoUCS2(myDialog.m_urlfield).get(),
//								myDialog.m_flagvalue, theURI, nsnull, nsnull);
   }
   if (!theURI) {
      QAOutput("We didn't get the URI. Test failed.", 2);
	  return;
   }

   rv = theURI->GetSpec(uriString);
   RvTestResult(rv, "nsIURI::GetSpec() for nsIWebNav test", 1);

   FormatAndPrintOutput("the nsIWebNav uri = ", uriString, displayMode);
}

void CNsIWebNav::GetSHTest(PRInt16 displayMode)
{
   PRInt32 numOfElements;

   nsCOMPtr<nsISHistory> theSessionHistory;
   rv =  qaWebNav->GetSessionHistory(getter_AddRefs(theSessionHistory));
   if (!theSessionHistory) {
      QAOutput("We didn't get the session history. Test failed.", 2);
	  return;
   }
   else
	  RvTestResult(rv, "GetSessionHistory() test", displayMode);

   rv = theSessionHistory->GetCount(&numOfElements);
   RvTestResult(rv, "nsISHistory::GetCount() for nsIWebNav test", 1);
 
   FormatAndPrintOutput("the sHist entry count = ", numOfElements, displayMode);
}

void CNsIWebNav::SetSHTest(PRInt16 displayMode)
{
   nsCOMPtr<nsISHistory> theSessionHistory, tempSHObject;
   // we want to save the existing session history
   rv =  qaWebNav->GetSessionHistory(getter_AddRefs(theSessionHistory));
   // this will create the test session history object
   tempSHObject = do_CreateInstance(NS_SHISTORY_CONTRACTID);
   rv = qaWebNav->SetSessionHistory(tempSHObject);
   RvTestResult(rv, "SetSessionHistory() test", displayMode);
   if (!tempSHObject)
      QAOutput("We didn't get the session history test object. Test failed.", 2);
	// we now reset the previous session history
   rv =  qaWebNav->SetSessionHistory(theSessionHistory);
}
