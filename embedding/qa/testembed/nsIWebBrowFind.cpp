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
		RvTestResultDlg(rv, "nsIWebBrowserFind object test");
		return(qaWBFind);
	}
}

void CNsIWebBrowFind::SetSearchStringTest(PRInt16 displayMode)
{
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();

	nsString searchString;
	if (myDialog.DoModal() == IDOK) {

		// SetSearchString()
		searchString.AssignWithConversion(myDialog.m_textfield);
		rv = qaWBFind->SetSearchString(searchString.get());
	}
	RvTestResult(rv, "nsIWebBrowserFind::SetSearchString() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::SetSearchString test");
}

void CNsIWebBrowFind::GetSearchStringTest(PRInt16 displayMode)
{
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();

	// GetSearchString()
	nsXPIDLString stringBuf;
	CString csSearchStr;
	rv = qaWBFind->GetSearchString(getter_Copies(stringBuf));
	RvTestResult(rv, "nsIWebBrowserFind::GetSearchString() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::GetSearchString() test");
	csSearchStr = stringBuf.get();
	FormatAndPrintOutput("The searched string = ", csSearchStr, displayMode);
}

void CNsIWebBrowFind::FindNextTest(PRBool didFind, PRInt16 displayMode)
{
	// FindNext()

	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();

	rv = qaWBFind->FindNext(&didFind);
	RvTestResult(rv, "nsIWebBrowserFind::FindNext() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::FindNext() test");
	FormatAndPrintOutput("returned didFind = ", didFind, displayMode);
}

void CNsIWebBrowFind::SetFindBackwardsTest(PRBool didFindBackwards, PRInt16 displayMode)
{
	// SetFindBackwards()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetFindBackwards(didFindBackwards);
	RvTestResult(rv, "nsIWebBrowserFind::SetFindBackwards() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::SetFindBackwards() test", displayMode);
}

void CNsIWebBrowFind::GetFindBackwardsTest(PRBool didFindBackwards, PRInt16 displayMode)
{
	// GetFindBackwards()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetFindBackwards(&didFindBackwards);
	RvTestResult(rv, "nsIWebBrowserFind::GetFindBackwards() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::GetFindBackwards() test");
	FormatAndPrintOutput("returned didFindBackwards = ", didFindBackwards, displayMode);
}

void CNsIWebBrowFind::SetWrapFindTest(PRBool didWrapFind, PRInt16 displayMode)
{
	// SetWrapFind()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetWrapFind(didWrapFind);
	RvTestResult(rv, "nsIWebBrowserFind::SetWrapFind() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::SetWrapFind() test");
}

void CNsIWebBrowFind::GetWrapFindTest(PRBool didWrapFind, PRInt16 displayMode)
{
	// GetWrapFind()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetWrapFind(&didWrapFind);
	RvTestResult(rv, "nsIWebBrowserFind::GetWrapFind() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::GetWrapFind() test");
	FormatAndPrintOutput("returned didWrapFind = ", didWrapFind, displayMode);
}

void CNsIWebBrowFind::SetEntireWordTest(PRBool didEntireWord, PRInt16 displayMode)
{
	// SetEntireWord()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetEntireWord(didEntireWord);
	RvTestResult(rv, "nsIWebBrowserFind::SetEntireWord() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::SetEntireWord() test");
}

void CNsIWebBrowFind::GetEntireWordTest(PRBool didEntireWord, PRInt16 displayMode)
{
	// GetEntireWord()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetEntireWord(&didEntireWord);
	RvTestResult(rv, "nsIWebBrowserFind::GetEntireWord() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::GetEntireWord() test");
	FormatAndPrintOutput("returned didEntireWord = ", didEntireWord, displayMode);
}

void CNsIWebBrowFind::SetMatchCase(PRBool didMatchCase, PRInt16 displayMode)
{
	// SetMatchCase()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetMatchCase(didMatchCase);
	RvTestResult(rv, "nsIWebBrowserFind::SetMatchCase() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::SetMatchCase() test");
}

void CNsIWebBrowFind::GetMatchCase(PRBool didMatchCase, PRInt16 displayMode)
{
	// GetMatchCase()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetMatchCase(&didMatchCase);
	RvTestResult(rv, "nsIWebBrowserFind::GetMatchCase() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::GetMatchCase() test");
	FormatAndPrintOutput("returned didMatchCase = ", didMatchCase, displayMode);
}

void CNsIWebBrowFind::SetSearchFrames(PRBool didSearchFrames, PRInt16 displayMode)
{
	// SetSearchFrames()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->SetSearchFrames(didSearchFrames);
	RvTestResult(rv, "nsIWebBrowserFind::SetSearchFrames() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::SetSearchFrames() test");
}

void CNsIWebBrowFind::GetSearchFrames(PRBool didSearchFrames, PRInt16 displayMode)
{
	// GetSearchFrames()
	nsCOMPtr<nsIWebBrowserFind> qaWBFind;
	qaWBFind = GetWebBrowFindObject();
	
	rv = qaWBFind->GetSearchFrames(&didSearchFrames);
	RvTestResult(rv, "nsIWebBrowserFind::GetSearchFrames() test", displayMode);
	RvTestResultDlg(rv, "nsIWebBrowserFind::GetSearchFrames() test");
	FormatAndPrintOutput("returned didSearchFrames = ", didSearchFrames, displayMode);
}

void CNsIWebBrowFind::OnStartTests(UINT nMenuID)
{
	switch(nMenuID)
	{
		case ID_INTERFACES_NSIWEBBROWSERFIND_RUNALLTESTS :
			RunAllTests();
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETSEARCHSTRINGTEST :
			SetSearchStringTest(2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETSEARCHSTRINGTEST :
			GetSearchStringTest(2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_FINDNEXTTEST  :
			FindNextTest(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETFINDBACKWARDSTEST :
			SetFindBackwardsTest(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETFINDBACKWARDSTEST :
			GetFindBackwardsTest(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETWRAPFINDTEST :
			SetWrapFindTest(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETWRAPFINDTEST  :
			GetWrapFindTest(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETENTIREWORDTEST  :
			SetEntireWordTest(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETENTIREWORDTEST :
			GetEntireWordTest(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETMATCHCASE :
			SetMatchCase(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETMATCHCASE :
			GetMatchCase(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_SETSEARCHFRAMES  :
			SetSearchFrames(PR_TRUE, 2);
			break ;
		case ID_INTERFACES_NSIWEBBROWSERFIND_GETSEARCHFRAMES  :
			GetSearchFrames(PR_TRUE, 2);
			break ;
	}
}

void CNsIWebBrowFind::RunAllTests()
{
	QAOutput("Begin WebBrowserFind tests.", 1);
	GetWebBrowFindObject();
	SetSearchStringTest(1);
	GetSearchStringTest(1);
	FindNextTest(PR_TRUE, 1);
	FindNextTest(PR_TRUE, 1);  // run a 2nd time to advance it
	SetFindBackwardsTest(PR_TRUE, 1);
	GetFindBackwardsTest(PR_TRUE, 1);
	FindNextTest(PR_TRUE, 1);	// to find backwards
	SetWrapFindTest(PR_TRUE, 1);
	GetWrapFindTest(PR_TRUE, 1);
	FindNextTest(PR_TRUE, 1);  // to wrap around
	SetEntireWordTest(PR_TRUE, 1);
	GetEntireWordTest(PR_TRUE, 1);
	FindNextTest(PR_TRUE, 1);  // entire word
	SetMatchCase(PR_TRUE, 1);
	GetMatchCase(PR_TRUE, 1);
	FindNextTest(PR_TRUE, 1);  // match case
	SetSearchFrames(PR_TRUE, 1);
	GetSearchFrames(PR_TRUE, 1);
	FindNextTest(PR_TRUE, 1);  // frames

	QAOutput("PR_FALSE tests", 1);
	SetFindBackwardsTest(PR_FALSE, 1);
	GetFindBackwardsTest(PR_FALSE, 1);
	FindNextTest(PR_FALSE, 1);		// shouldn't find backwards
	SetWrapFindTest(PR_FALSE, 1);
	GetWrapFindTest(PR_FALSE, 1);
	FindNextTest(PR_FALSE, 1);		// shouldn't find wrap around
	SetEntireWordTest(PR_FALSE, 1);
	GetEntireWordTest(PR_FALSE, 1);
	FindNextTest(PR_FALSE, 1);		// shouldn't find entire word
	SetMatchCase(PR_FALSE, 1);
	GetMatchCase(PR_FALSE, 1);
	FindNextTest(PR_FALSE, 1);		// shouldn't find case
	SetSearchFrames(PR_FALSE, 1);
	GetSearchFrames(PR_FALSE, 1);
	FindNextTest(PR_FALSE, 1);		// shouldn't find frame
	QAOutput("End WebBrowserFind tests.", 1);
}