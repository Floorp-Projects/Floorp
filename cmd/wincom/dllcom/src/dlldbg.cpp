/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
