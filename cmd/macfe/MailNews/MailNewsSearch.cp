/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// MailNewsSearch.cp

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#define DEBUGGER_ASSERTIONS

#include "MailNewsSearch.h"
#include "SearchHelpers.h"
#include "CDateView.h"
#include "CMessageSearchWindow.h"
#include "CSearchTableView.h"
#include "CCaption.h"
#include "CSingleTextColumn.h"
#include "CTSMEditField.h"
#include "URobustCreateWindow.h"
#include "LCommander.h"

#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

//-----------------------------------
void CSearchWindowManager::RegisterSearchClasses()
//	Register all classes associated with the search window.
//-----------------------------------
{

	USearchHelper::RegisterClasses();
	CDateView::RegisterDateClasses();

	RegisterClass_(CMessageSearchWindow);
	
	RegisterClass_(CSearchTableView);
	RegisterClass_(CListenerCaption);
	
	RegisterClass_(LTableView);
	RegisterClass_(CSingleTextColumn);
	

} // CSearchWindowManager::RegisterSearchClasses

//-----------------------------------
void CSearchWindowManager::ShowSearchWindow()
//	Show the search dialog by bringing it to the front if it is not already. Create it
//	if needed.
//-----------------------------------
{

	// Find out if the window is already around
	CMessageSearchWindow *searchWindow = dynamic_cast<CMessageSearchWindow *>(
			CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_SearchMailNews));

	if ( searchWindow == nil )
	{
		// Search dialog has not yet been created, so create it here and display it.
		searchWindow = dynamic_cast<CMessageSearchWindow *>(
				URobustCreateWindow::CreateWindow(CMessageSearchWindow::res_ID, 
												  LCommander::GetTopCommander()));
	}
	
	// Select the window
	if (searchWindow)
	{
		// We set the scope of the search window only when the user issues the
		// Search command. Previously, it was done on Activate and ReadWindowStatus.
		searchWindow->SetUpBeforeSelecting();
		searchWindow->Select();
	}
} // CSearchWindowManager::ShowSearchWindow




