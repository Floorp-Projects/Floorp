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
 * Portions created by the Initial Developer are Copyright (C) 1998, 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Epstein <depstein@netscape.com>
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

//
// test cases for nsIWebBrowserFind
//

#include "Stdafx.h"
#include "TestEmbed.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "Tests.h"
#include "domwindow.h"
#include "QaUtils.h"
#include <stdio.h>
#include "nsIWebBrowFind.h"
#include "UrlDialog.h"
#include "QaFindDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// constructor for CNsIWebBrowFind
CNsIWebBrowFind::CNsIWebBrowFind(nsIWebBrowser *mWebBrowser, CBrowserImpl *mpBrowserImpl)
{
	qaWebBrowser = mWebBrowser;
	qaBrowserImpl = mpBrowserImpl;
}

// destructor for CNsIWebBrowFind
CNsIWebBrowFind::~CNsIWebBrowFind()
{

}

	// get webBrowserFind object
nsIWebBrowserFind * CNsIWebBrowFind::GetWebBrowFindObject()
{
	nsCOMPtr<nsIWebBrowserFind> qaWBFind(do_GetInterface(qaWebBrowser, &rv));
	if (!qaWBFind) {
		QAOutput("Didn't get WebBrowserFind object.", 2);
		return NULL;
	}
	else {
		RvTestResult(rv, "nsIWebBrowserFind object test", 1);
		return(qaWBFind);
	}
}

void CNsIWebBrowFind::SetSearchStringTest()
{
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();

	nsString searchString;
	if (myDialog.DoModal() == IDOK) {

		// SetSearchString()
		searchString.AssignWithConversion(myDialog.m_textfield);
		rv = qaWBFind->SetSearchString(searchString.get());
		RvTestResult(rv, "nsIWebBrowserFind::SetSearchString() test", 2);
	}
}

void CNsIWebBrowFind::GetSearchStringTest()
{
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();

	// GetSearchString()
	nsXPIDLString stringBuf;
	CString csSearchStr;
	rv = qaWBFind->GetSearchString(getter_Copies(stringBuf));
	RvTestResult(rv, "nsIWebBrowserFind::GetSearchString() test", 2);
	csSearchStr = stringBuf.get();
	FormatAndPrintOutput("The searched string = ", csSearchStr, 2);
}

void CNsIWebBrowFind::FindNextTest(PRBool didFind)
{
	// FindNext()

	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();

	rv = qaWBFind->FindNext(&didFind);
	RvTestResult(rv, "nsIWebBrowserFind::FindNext(PR_TRUE) object test", 2);
	FormatAndPrintOutput("returned didFind = ", didFind, 2);
}

void CNsIWebBrowFind::SetFindBackwardsTest(PRBool didFindBackwards)
{
	// SetFindBackwards()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetFindBackwards(didFindBackwards);
	RvTestResult(rv, "nsIWebBrowserFind::SetFindBackwards(PR_TRUE) object test", 2);
}

void CNsIWebBrowFind::GetFindBackwardsTest(PRBool didFindBackwards)
{
	// GetFindBackwards()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetFindBackwards(&didFindBackwards);
	RvTestResult(rv, "nsIWebBrowserFind::GetFindBackwards(PR_TRUE) object test", 2);
	FormatAndPrintOutput("returned didFindBackwards = ", didFindBackwards, 2);
}

void CNsIWebBrowFind::SetWrapFindTest(PRBool didWrapFind)
{
	// SetWrapFind()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetWrapFind(didWrapFind);
	RvTestResult(rv, "nsIWebBrowserFind::SetWrapFind(PR_TRUE) object test", 2);
}

void CNsIWebBrowFind::GetWrapFindTest(PRBool didWrapFind)
{
	// GetWrapFind()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetWrapFind(&didWrapFind);
	RvTestResult(rv, "nsIWebBrowserFind::GetWrapFind(PR_TRUE) object test", 2);
	FormatAndPrintOutput("returned didWrapFind = ", didWrapFind, 2);
}

void CNsIWebBrowFind::SetEntireWordTest(PRBool didEntireWord)
{
	// SetEntireWord()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetEntireWord(didEntireWord);
	RvTestResult(rv, "nsIWebBrowserFind::SetEntireWord(PR_TRUE) object test", 2);
}

void CNsIWebBrowFind::GetEntireWordTest(PRBool didEntireWord)
{
	// GetEntireWord()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetEntireWord(&didEntireWord);
	RvTestResult(rv, "nsIWebBrowserFind::GetEntireWord(PR_TRUE) object test", 2);
	FormatAndPrintOutput("returned didEntireWord = ", didEntireWord, 2);
}

void CNsIWebBrowFind::SetMatchCase(PRBool didMatchCase)
{
	// SetMatchCase()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetMatchCase(didMatchCase);
	RvTestResult(rv, "nsIWebBrowserFind::SetMatchCase(PR_TRUE) object test", 2);
}

void CNsIWebBrowFind::GetMatchCase(PRBool didMatchCase)
{
	// GetMatchCase()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetMatchCase(&didMatchCase);
	RvTestResult(rv, "nsIWebBrowserFind::GetMatchCase(PR_TRUE) object test", 2);
	FormatAndPrintOutput("returned didMatchCase = ", didMatchCase, 2);
}

void CNsIWebBrowFind::SetSearchFrames(PRBool didSearchFrames)
{
	// SetSearchFrames()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetSearchFrames(didSearchFrames);
	RvTestResult(rv, "nsIWebBrowserFind::SetSearchFrames(PR_TRUE) object test", 2);
}

void CNsIWebBrowFind::GetSearchFrames(PRBool didSearchFrames)
{
	// GetSearchFrames()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetSearchFrames(&didSearchFrames);
	RvTestResult(rv, "nsIWebBrowserFind::GetSearchFrames(PR_TRUE) object test", 2);
	FormatAndPrintOutput("returned didSearchFrames = ", didSearchFrames, 2);
}

void CNsIWebBrowFind::OnStartTests(UINT nMenuID)
{
	switch(nMenuID)
	{
		case ID_INTERFACES_NSIWEBBROWSERFIND_RUNALLTESTS :
			RunAllTests();
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETSEARCHSTRINGTEST :
			SetSearchStringTest();
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETSEARCHSTRINGTEST :
			GetSearchStringTest();
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_FINDNEXTTEST  :
			FindNextTest(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETFINDBACKWARDSTEST :
			SetFindBackwardsTest(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETFINDBACKWARDSTEST :
			GetFindBackwardsTest(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETWRAPFINDTEST :
			SetWrapFindTest(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETWRAPFINDTEST  :
			GetWrapFindTest(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETENTIREWORDTEST  :
			SetEntireWordTest(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETENTIREWORDTEST :
			GetEntireWordTest(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETMATCHCASE :
			SetMatchCase(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETMATCHCASE :
			GetMatchCase(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETSEARCHFRAMES  :
			SetSearchFrames(PR_TRUE);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETSEARCHFRAMES  :
			GetSearchFrames(PR_TRUE);
			break ;
	}
}

void CNsIWebBrowFind::RunAllTests()
{
	GetWebBrowFindObject();
	SetSearchStringTest();
	GetSearchStringTest();
	FindNextTest(PR_TRUE);
	SetFindBackwardsTest(PR_TRUE);
	GetFindBackwardsTest(PR_TRUE);
	SetWrapFindTest(PR_TRUE);
	GetWrapFindTest(PR_TRUE);
	SetEntireWordTest(PR_TRUE);
	GetEntireWordTest(PR_TRUE);
	SetMatchCase(PR_TRUE);
	GetMatchCase(PR_TRUE);
	SetSearchFrames(PR_TRUE);
	GetSearchFrames(PR_TRUE);

	FindNextTest(PR_FALSE);
	SetFindBackwardsTest(PR_FALSE);
	GetFindBackwardsTest(PR_FALSE);
	SetWrapFindTest(PR_FALSE);
	GetWrapFindTest(PR_FALSE);
	SetEntireWordTest(PR_FALSE);
	GetEntireWordTest(PR_FALSE);
	SetMatchCase(PR_FALSE);
	GetMatchCase(PR_FALSE);
	SetSearchFrames(PR_FALSE);
	GetSearchFrames(PR_FALSE);
}