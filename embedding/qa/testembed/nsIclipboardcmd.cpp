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
 *   Dharma Sirnapalli <dsirnapalli@netscape.com>
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
// Test cases for the nsIClipBoardCommand Interface

#include "stdafx.h"
#include "QaUtils.h"
#include <stdio.h>
#include "nsIClipboardCmd.h"

CNsIClipBoardCmd::CNsIClipBoardCmd(nsIWebBrowser* mWebBrowser)
{
	qaWebBrowser = mWebBrowser;
}


CNsIClipBoardCmd::~CNsIClipBoardCmd()
{
}


void CNsIClipBoardCmd::OnStartTests(UINT nMenuID)
{
	// Calls  all or indivdual test cases on the basis of the 
	// option selected from menu.

	switch(nMenuID)
	{
		case ID_INTERFACES_NSICLIPBOARDCOMMANDS_PASTE	:
			OnPasteTest();
			break ;
		case ID_INTERFACES_NSICLIPBOARDCOMMANDS_COPYSELECTION :
			OnCopyTest();
			break ;
		case ID_INTERFACES_NSICLIPBOARDCOMMANDS_SELECTALL :
			OnSelectAllTest();
			break ;
		case ID_INTERFACES_NSICLIPBOARDCOMMANDS_SELECTNONE :
			OnSelectNoneTest();
			break ;
		case ID_INTERFACES_NSICLIPBOARDCOMMANDS_CUTSELECTION :
			OnCutSelectionTest();
			break ;
		case ID_INTERFACES_NSICLIPBOARDCOMMANDS_COPYLINKLOCATION :
			copyLinkLocationTest();
			break ;
		case ID_INTERFACES_NSICLIPBOARDCOMMANDS_CANCOPYSELECTION :
			canCopySelectionTest();
			break ;
		case ID_INTERFACES_NSICLIPBOARDCOMMANDS_CANCUTSELECTION :
			canCutSelectionTest();
			break ;
		case ID_INTERFACES_NSICLIPBOARDCOMMANDS_CANPASTE :
			canPasteTest();
			break ;
	}

}

// ***********************************************************************
//DHARMA	- nsIClipboardCommands
// Checking the paste() method.
void CNsIClipBoardCmd::OnPasteTest()
{
    QAOutput("testing paste command", 1);
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(qaWebBrowser);
    if (clipCmds)
	{
        rv = clipCmds->Paste();
		RvTestResult(rv, "nsIClipboardCommands::Paste()' rv test", 1);

	}
	else
		QAOutput("We didn't get the clipboard object.", 1);
}

// Checking the copySelection() method.
void CNsIClipBoardCmd::OnCopyTest()
{
    QAOutput("testing copyselection command");
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(qaWebBrowser);
    if (clipCmds)
	{
        rv = clipCmds->CopySelection();
		RvTestResult(rv, "nsIClipboardCommands::CopySelection()' rv test", 1);
	}
	else
		QAOutput("We didn't get the clipboard object.", 1);
}

// Checking the selectAll() method.
void CNsIClipBoardCmd::OnSelectAllTest()
{
    QAOutput("testing selectall method");
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(qaWebBrowser);
    if (clipCmds)
	{
        rv = clipCmds->SelectAll();
		RvTestResult(rv, "nsIClipboardCommands::SelectAll()' rv test", 1);
	}
	else
		QAOutput("We didn't get the clipboard object.", 1);
}

// Checking the selectNone() method.
void CNsIClipBoardCmd::OnSelectNoneTest()
{
    QAOutput("testing selectnone method");
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(qaWebBrowser);
    if (clipCmds)
	{
        rv = clipCmds->SelectNone();
		RvTestResult(rv, "nsIClipboardCommands::SelectNone()' rv test", 1);
	}
	else
		QAOutput("We didn't get the clipboard object.", 1);
}

// Checking the cutSelection() method.
void CNsIClipBoardCmd::OnCutSelectionTest()
{
    QAOutput("testing cutselection method");
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(qaWebBrowser);
    if (clipCmds)
	{
        rv = clipCmds->CutSelection();
		RvTestResult(rv, "nsIClipboardCommands::CutSelection()' rv test", 1);
	}
	else
		QAOutput("We didn't get the clipboard object.", 1);
}

// Checking the copyLinkLocation() method.
void CNsIClipBoardCmd::copyLinkLocationTest()
{
    QAOutput("testing CopyLinkLocation method", 2);
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(qaWebBrowser);
    if (clipCmds)
	{
        rv = clipCmds->CopyLinkLocation();
		RvTestResult(rv, "nsIClipboardCommands::CopyLinkLocation()' rv test", 1);
	}
	else
		QAOutput("We didn't get the clipboard object.", 1);
}

// Checking the canCopySelection() method.
void CNsIClipBoardCmd::canCopySelectionTest()
{
    PRBool canCopySelection = PR_FALSE;
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(qaWebBrowser);
    if (clipCmds)
	{
       rv = clipCmds->CanCopySelection(&canCopySelection);
	   RvTestResult(rv, "nsIClipboardCommands::CanCopySelection()' rv test", 1);

       if(canCopySelection)
          QAOutput("The selection you made Can be copied", 2);
       else
          QAOutput("Either you did not make a selection or The selection you made Cannot be copied", 2);
	}
	else
		QAOutput("We didn't get the clipboard object.", 1);
}

// Checking the canCutSelection() method.
void CNsIClipBoardCmd::canCutSelectionTest()
{
    PRBool canCutSelection = PR_FALSE;
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(qaWebBrowser);
    if (clipCmds)
	{
       rv = clipCmds->CanCutSelection(&canCutSelection);
	   RvTestResult(rv, "nsIClipboardCommands::CanCutSelection()' rv test", 1);

	   if(canCutSelection)
          QAOutput("The selection you made Can be cut", 2);
       else
          QAOutput("Either you did not make a selection or The selection you made Cannot be cut", 2);
	}
	else
		QAOutput("We didn't get the clipboard object.", 1);
}

// Checking the canPaste() method.
void CNsIClipBoardCmd::canPasteTest()
{
    PRBool canPaste = PR_FALSE;
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(qaWebBrowser);
    if (clipCmds)
	{
        rv = clipCmds->CanPaste(&canPaste);
	    RvTestResult(rv, "nsIClipboardCommands::CanPaste()' rv test", 1);

		if(canPaste)
			QAOutput("The clipboard contents can be pasted here", 2);
		else
			QAOutput("The clipboard contents cannot be pasted here", 2);
	}
	else
		QAOutput("We didn't get the clipboard object.", 1);
}

//DHARMA
