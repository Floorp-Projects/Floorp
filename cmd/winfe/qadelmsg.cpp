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
//
// Filename: qadelmsg.cpp
// Original author: ppandit
// Purpose: To accept handoff from whitebox UI via call to WhiteBox_DeleteMessage1().
//          Proceed with deleting the sixth message in the Inbox. During the deletion,
//          trace statements will record parameter and debug information in
//          a file (ex. Test1.log). When the deletion is completed, a verification
//          procedure will confirm if that message is actually gone. Notifications
//          play a big part. We check for MSG_PaneNotifyMessageLoaded and
//          MSG_PaneNotifyMessageDeleted in thrdfrm.cpp. Code looks scattered
//          because we have to deal with the asychronous nature of Communicator.

#include "stdafx.h"
#include "stdlib.h"
#include "msgcom.h"
#include "wfemsg.h"
#include "thrdfrm.h"
#include "testcase.h"
#include "resource.h"
#include "qaoutput.h"
#include "qa.h"
#include "qatrace.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// globals for this test case
C3PaneMailFrame *qainbox;
MessageKey qaSaveMessageKey = MSG_MESSAGEKEYNONE;
MSG_Pane* qapane;
CTestCaseManager *qatcmanager;

void WhiteBox_DeleteMessage1(CTestCaseManager *tc)
{ 
   // QA_TestCase.PhaseTwoIsFalse();
   bDoWhiteBox = TRUE;
   bWaitForInbox = TRUE;
   QATestCaseStarted = FALSE;
   QATestCaseDone = FALSE;
   qatcmanager = tc;
   qainbox = (C3PaneMailFrame*)WFE_MSGOpenInbox(TRUE);  // make it so getNewMail = TRUE
}

void QADoDeleteMessageEventHandler()
{
	if (bWaitForInbox == FALSE && bDoWhiteBox == TRUE)
	{
		// set flag to nolonger enter here
		QATestCaseStarted = TRUE;
		// do real work
		CMsgListFrame* qaMsgListFrame = qainbox;
		qaMsgListFrame->SelectItem(qainbox->GetPane(),5);
		qainbox->WhiteBox_OnSelect();

		qapane = qainbox->GetPane();

		MessageKey qaSaveMessageKey = MSG_MESSAGEKEYNONE;
		// should chose a better method than GetCurMessage() to get MessageKey.
		qaSaveMessageKey = qainbox->GetCurMessage();

		// use the following instead of
		// qainbox->SendMessage(WM_COMMAND, ID_EDIT_DELETEMESSAGE, 0);
		qainbox->WhiteBox_OnDeleteMessage();

		// unneeded code but kept here for future use
		// MSG_Master *pMaster = WFE_MSGGetMaster();
		// MSG_Pane* qapane = qainbox->GetPane();
		// qainbox->SendMessage(WM_COMMAND, ID_EDIT_DELETEMESSAGE, 0);

	}
}

void QADoDeleteMessageEventHandler2()
{
	static count = 0;

	if (count > 0) return;

	MSG_ViewIndex qaTestMessageExists = MSG_VIEWINDEXNONE;

	qaTestMessageExists = MSG_GetMessageIndexForKey(qapane, qaSaveMessageKey, TRUE);
	if (qaTestMessageExists == MSG_VIEWINDEXNONE) {
		QA_TestCase.PhaseTwoIsTrue();
	} else {
		QA_TestCase.PhaseTwoIsFalse();
	}

	count += 1;

	// done with testcase
	// QA_TestCase.DoWrapup();
	QATestCaseDone = TRUE;
}

void TrialTestCase2(CTestCaseManager *qatc)
{ 
   AfxMessageBox("This test has not been developed");
}


