/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Adam Lock <adamlock@netscape.com>
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