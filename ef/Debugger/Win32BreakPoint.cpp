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
#include <stdio.h>
#include "Fundamentals.h"
#include <windows.h>

unsigned char old_instruction;

extern "C" int
SetBreakPoint(void *pc)
{
	old_instruction = (*((unsigned char *) pc));
	*((unsigned char *) pc) = 0xcc; /* break point opcode */
	fprintf(stderr, "breakpoint at: %x\n", pc);
	fprintf(stderr, "value: %x\n", &old_instruction);
	/* Need to flush icache here */
	return 0;
}

__declspec(dllexport) EXCEPTION_DISPOSITION __cdecl 
_arun_handler(struct _EXCEPTION_RECORD
				*ExceptionRecord, 
				void * EstablisherFrame, 
				struct _CONTEXT *ContextRecord, 
				void * DispatcherContext ) 

{ 
	// Indicate that we made it to our exception handler 
	fprintf(stderr, "Faulted with: 0x%x\n",ExceptionRecord->ExceptionCode); 
	// Change EIP in the context record so that it points to someplace 
	// where we can successfully continue 
	switch (ExceptionRecord->ExceptionCode) { 
	case STATUS_BREAKPOINT: 
		fprintf(stderr, "Restoring the instruction at %x\n", 
				ContextRecord->Eip);
		(*(unsigned char *) (ContextRecord->Eip)) = old_instruction;
		fprintf(stderr, "Restored: %x\n", old_instruction);
		// Reexecute the same instruction
		return ExceptionContinueExecution; 
	default: 
		return ExceptionContinueSearch; 
	} 
} 
