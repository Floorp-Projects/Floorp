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

#if !defined(_CPU_INFO_H_) || defined(INCLUDE_EMITTER)
#define _CPU_INFO_H_

#ifdef GENERATE_FOR_PPC
		#include "PPC601Cpu.h"
#elif defined(GENERATE_FOR_X86)
		#include "x86Win32Cpu.h"
#elif defined(GENERATE_FOR_SPARC)
		//#include "SparcCpu.h"
#elif defined(GENERATE_FOR_HPPA)
		#include "HPPACpu.h"
#endif

#ifndef CPU_IS_SUPPORTED
#error "Processor not supported"
#endif

#define NUMBER_OF_CALLER_SAVED_GR  (LAST_CALLER_SAVED_GR - FIRST_CALLER_SAVED_GR + 1)
#define NUMBER_OF_CALLEE_SAVED_GR  (LAST_CALLEE_SAVED_GR - FIRST_CALLEE_SAVED_GR + 1)
#define NUMBER_OF_CALLER_SAVED_FPR (LAST_CALLER_SAVED_FPR - FIRST_CALLER_SAVED_FPR + 1)
#define NUMBER_OF_CALLEE_SAVED_FPR (LAST_CALLEE_SAVED_FPR - FIRST_CALLEE_SAVED_FPR + 1)

#define NUMBER_OF_GREGISTERS       (LAST_GREGISTER - FIRST_GREGISTER + 1)
#define NUMBER_OF_FPREGISTERS      (LAST_FPREGISTER - FIRST_FPREGISTER + 1)
#define NUMBER_OF_REGISTERS        (NUMBER_OF_GREGISTERS + NUMBER_OF_FPREGISTERS + NUMBER_OF_SPECIAL_REGISTERS)


#endif /* _CPU_INFO_H_ */
