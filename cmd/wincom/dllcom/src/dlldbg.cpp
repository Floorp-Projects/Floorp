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

#include "dllcom.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef DLL_DEBUG

#define TRACE_BUFFERSIZE 256

void DLL_Trace(const char *pFormat, ...)
{
    LPSTR	pBuffer = (char *)CoTaskMemAlloc(TRACE_BUFFERSIZE);

    if(pBuffer) {
        //  Output module name.
        strcpy(pBuffer, CComDll::m_pTraceID);
        strcat(pBuffer, ":");

        //  Output module instance.
        _ltoa(CProcess::GetProcessID(), pBuffer + lstrlen(pBuffer), 10);
        strcat(pBuffer, ":  ");

        //  Output what programmer wanted now.
	    va_list args;
	    va_start(args, pFormat);
    
        vsprintf(pBuffer + lstrlen(pBuffer), pFormat, args);

        //  Offending trace statement.
        //  Reduce output, or increase trace size.
        DLL_ASSERT(lstrlen(pBuffer) < TRACE_BUFFERSIZE);

        OutputDebugString(pBuffer);

	    va_end(args);

		CoTaskMemFree((LPVOID)pBuffer);
        pBuffer = NULL;
    }
}
#endif
