/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 */
#ifndef BROWSER_DIAGNOSTICS_H
#define BROWSER_DIAGNOSTICS_H

#ifdef _DEBUG
#  include <assert.h>
#  define NG_TRACE NgTrace
#  define NG_TRACE_METHOD(fn) \
    { \
        NG_TRACE(_T("0x%04x %s()\n"), (int) GetCurrentThreadId(), _T(#fn)); \
    }
#  define NG_TRACE_METHOD_ARGS(fn, pattern, args) \
    { \
        NG_TRACE(_T("0x%04x %s(") _T(pattern) _T(")\n"), (int) GetCurrentThreadId(), _T(#fn), args); \
    }
#  define NG_ASSERT(expr) assert(expr)
#  define NG_ASSERT_POINTER(p, type) \
    NG_ASSERT(((p) != NULL) && NgIsValidAddress((p), sizeof(type), FALSE))
#  define NG_ASSERT_NULL_OR_POINTER(p, type) \
    NG_ASSERT(((p) == NULL) || NgIsValidAddress((p), sizeof(type), FALSE))
#else
#  define NG_TRACE 1 ? (void)0 : NgTrace
#  define NG_TRACE_METHOD(fn)
#  define NG_TRACE_METHOD_ARGS(fn, pattern, args)
#  define NG_ASSERT(X)
#  define NG_ASSERT_POINTER(p, type)
#  define NG_ASSERT_NULL_OR_POINTER(p, type)
#endif

#define NG_TRACE_ALWAYS AtlTrace

inline void _cdecl NgTrace(LPCSTR lpszFormat, ...)
{
    va_list args;
    va_start(args, lpszFormat);

    int nBuf;
    char szBuffer[512];

    nBuf = _vsnprintf(szBuffer, sizeof(szBuffer), lpszFormat, args);
    NG_ASSERT(nBuf < sizeof(szBuffer)); //Output truncated as it was > sizeof(szBuffer)

    OutputDebugStringA(szBuffer);
    va_end(args);
}

inline BOOL NgIsValidAddress(const void* lp, UINT nBytes, BOOL bReadWrite = TRUE)
{
    return (lp != NULL && !IsBadReadPtr(lp, nBytes) &&
        (!bReadWrite || !IsBadWritePtr((LPVOID)lp, nBytes)));
}

#endif