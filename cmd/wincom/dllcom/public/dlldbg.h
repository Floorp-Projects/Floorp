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

#ifndef __DllDebug_H
#define __DllDebug_H

#ifdef DLL_RELEASE
#define DLL_ASSERT(a) ((void)0)
#endif
#ifdef DLL_DEBUG
#define DLL_ASSERT(a) ((a) ? ((void)0) : DebugBreak())
#endif

#ifdef DLL_RELEASE
#define DLL_VERIFY(a) ((void)(a))
#endif
#ifdef DLL_DEBUG
#define DLL_VERIFY(a) DLL_ASSERT(a)
#endif

#ifdef DLL_RELEASE
#define DLL_TRACE 1 ? ((void)0) : DLL_Trace
#endif
#ifdef DLL_DEBUG
#define DLL_TRACE DLL_Trace
#endif

#ifdef DLL_RELEASE
inline void DLL_Trace(const char *, ...) { }
#endif
#ifdef DLL_DEBUG
void DLL_Trace(const char *pFormat, ...);
#endif

#endif // __DllDebug_H