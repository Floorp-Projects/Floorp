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

#if !defined(_X86WIN32_CPU_H_) || defined(INCLUDE_EMITTER)
#define _X86WIN32_CPU_H_

#define FIRST_CALLEE_SAVED_GR       4
#define LAST_CALLEE_SAVED_GR        6
#define FIRST_CALLER_SAVED_GR       0
#define LAST_CALLER_SAVED_GR        3
#define FIRST_CALLEE_SAVED_FPR      0
#define LAST_CALLEE_SAVED_FPR       -1
#define FIRST_CALLER_SAVED_FPR      0
#define LAST_CALLER_SAVED_FPR       -1

#define FIRST_GREGISTER             0
#define LAST_GREGISTER              5
#define FIRST_FPREGISTER            0
#define LAST_FPREGISTER             -1
#define NUMBER_OF_SPECIAL_REGISTERS 0

class x86Win32Emitter;
class x86Formatter;
typedef x86Formatter MdFormatter;
typedef x86Win32Emitter MdEmitter;

#ifdef INCLUDE_EMITTER
#include "x86Win32Emitter.h"
#include "x86Formatter.h"
#endif

#define CPU_IS_SUPPORTED

#endif /* _X86WIN32_CPU_H_ */
