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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
// qaui.cpp


#include "qaoutput.h"
#include "qatrace.h"
#include "qaui.h"
#include "qa.h"
// #include "qatcnmes.h"	// Rindy's header

// global variable
BOOL bDoWhiteBox = FALSE;   // need to come up with better solution
BOOL bWaitForInbox = TRUE;
BOOL QATestCaseStarted;
BOOL QATestCaseDone;

void TakeFromMenu()
{
	AssignFilename("qaoutput.log");	// get testcase name from Rindy's data structure
	StartTestcase();				// get testcase ID from Rindy
}

void StartTestcase()
{
	// use a switch statement or Rindy will take care of this
	DeleteMsgToTrash();
}

void DeleteMsgToTrash()
{
	// add code to confirm configuration is for move to trash

	// code to bring up Inbox and delete a message

}

