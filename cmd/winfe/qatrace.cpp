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
#include "stdafx.h"
#include <stddef.h>
#include <stdio.h>

#include "..\..\include\xp_trace.h"
#include "..\..\include\xp_mcom.h"
#include "qaoutput.h"
#include "qatrace.h"
#include "testcase.h"

void QA_TraceV (const char* message, va_list args)
{
#ifdef DEBUG_WHITEBOX
    char buf[2000]; 
	PR_vsnprintf(buf, sizeof(buf), message, args);
	// Trying Rindy's approach
	CString msg = buf;
	QA_TestCase.QA_Trace(msg);
	// QAAddToOutputFile(buf);
	// Uncomment if desire visual output
	// AfxMessageBox(msg);
#endif /* DEBUG_WHITEBOX */
}

/* Trace with trailing newline */
void QA_Trace (const char* message, ...)
{
#ifdef DEBUG_WHITEBOX
    va_list args;

	// QAOpenOutputFile();
    va_start(args, message);
	QA_TraceV(message, args);
    va_end(args);
	// QACloseOutputFile();
#endif /* DEBUG_WHITEBOX */
}


