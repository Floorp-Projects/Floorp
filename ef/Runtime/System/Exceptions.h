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
#ifndef _EXCEPTIONS_H_
#define _EXCEPTIONS_H_

#ifdef _WIN32
#include "md/x86/x86Win32ExceptionHandler.h"
// For now use inline assembly, tidy this up later
#define installHardwareExceptionHandler(HARDWARE_EXCP) __asm {	__asm push HARDWARE_EXCP	__asm push fs:[0]		__asm mov fs:[0],esp  }                                                                                                                         
#define removeHardwareExceptionHandler()  __asm {	__asm mov eax,[esp]	__asm mov fs:[0],eax	__asm add esp,8 }

#else

static void installHardwareExceptionHandler() {}
static void removeHardwareExceptionHandler() {}
#endif

#endif /* _EXCEPTIONS_H_ */
