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
// qa.h
// 
// Purpose: This header should be used to contain global variables needed by
// QA whitebox testing. This could possibly contain QA API testing as well
//

#ifndef _qa_h_
#define _qa_h_

#include "xp_core.h"

extern BOOL bDoWhiteBox;        // defined in qaui.cpp
extern BOOL bWaitForInbox;		// defined in qaui.cpp
extern BOOL QATestCaseStarted;	// defined in qaui.cpp
extern BOOL QATestCaseDone;		// defined in qaui.cpp
void QADoDeleteMessageEventHandler();
void QADoDeleteMessageEventHandler2();


#endif // _qa_h_
