/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; -*-  
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

/*******************************************************************************
                            S P O R T   M O D E L    
                              _____
                         ____/_____\____
                        /__o____o____o__\        __
                        \_______________/       (@@)/
                         /\_____|_____/\       x~[]~
             ~~~~~~~~~~~/~~~~~~~|~~~~~~~\~~~~~~~~/\~~~~~~~~~

    Advanced Technology Garbage Collector
    Copyright (c) 1997 Netscape Communications, Inc. All rights reserved.
    Recovered by: Warren Harris
*******************************************************************************/

#ifdef DEBUG

#include "smtrav.h"
#include "smheap.h"

SM_IMPLEMENT(void)
SM_Assert(const char* ex, const char* file, int line)
{
    char buf[400];

    PR_snprintf(buf, sizeof(buf),
		"Assertion failure: %s, in file \"%s\" line %d\n",
		ex, file, line);

#ifndef XP_MAC /* takes too long */
    fprintf(sm_DebugOutput, buf);
    SM_DumpStats(sm_DebugOutput, PR_TRUE);
    SM_DumpHeap(sm_DebugOutput, SMDumpFlag_Detailed | SMDumpFlag_GCState);
    SM_DumpPageMap(sm_DebugOutput, SMDumpFlag_Detailed | SMDumpFlag_GCState);
#endif
    fflush(sm_DebugOutput);

#ifdef XP_UNIX
    fprintf(stderr, buf);
#endif

#ifdef XP_MAC
    {
    	Str255		debugStr;
    	uint32		length;
    	
    	length = strlen(buf);
    	if (length > 255)
            length = 255;
    	debugStr[0] = length;
    	memcpy(debugStr + 1, buf, length);
    	DebugStr(debugStr);    
    }
#endif

#if defined(XP_PC) && defined(DEBUG)
    {
	char* msg = PR_smprintf("%s\nPress Retry to debug the application.", buf);
	int status = MessageBox((HWND)NULL, msg, "NSPR Assertion",
				MB_ABORTRETRYIGNORE | MB_ICONEXCLAMATION);
	PR_smprintf_free(msg);
	switch (status) {
	  case IDABORT:
#ifdef _WIN32
	    PR_Abort();
#endif
	    break;
	  case IDRETRY:
	    DebugBreak();
	    break;
	  case IDIGNORE:
	    break;
	}
    }
#endif /* XP_PC && DEBUG */

#if !defined(XP_PC)
    PR_Abort();
#endif
}

#endif /* DEBUG */

/******************************************************************************/
