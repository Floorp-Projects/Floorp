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
#include <windows.h>

#ifdef _DEBUG
BOOL AfxAssertFailedLine(LPCSTR lpszFileName, int nLine);

#ifndef WIN16
void CDECL AfxTrace(LPCTSTR lpszFormat, ...);
#else
void CDECL AfxTrace(LPCSTR lpszFormat, ...);
#endif

#define TRACE              ::AfxTrace
#define THIS_FILE          __FILE__
#define ASSERT(f) \
	do \
	{ \
	if (!(f) && AfxAssertFailedLine(THIS_FILE, __LINE__)) \
		AfxDebugBreak(); \
	} while (0) \

#define VERIFY(f)          ASSERT(f)
#define TRACE_FN(name) LogFn __logFn(name)
class LogFn
{
public:
	LogFn(LPCSTR pFnName) {m_pFnName = pFnName; TRACE("--%s: In--\n", pFnName);}
	~LogFn() {TRACE("--%s: Out--\n", m_pFnName);}
private:
	LPCSTR m_pFnName;
};
#else
// NDEBUG
#define ASSERT(f)          ((void)0)
#define VERIFY(f)          ((void)(f))
#define ASSERT_VALID(pOb)  ((void)0)
#define DEBUG_ONLY(f)      ((void)0)   
#ifdef WIN32
inline void CDECL AfxTrace(LPCTSTR, ...) { }
#else
inline void CDECL AfxTrace(LPCSTR, ...) { }
#endif
#define TRACE              1 ? (void)0 : ::AfxTrace
#define TRACE_FN(name)
#endif // _DEBUG

