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
