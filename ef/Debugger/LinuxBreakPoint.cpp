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
#include <signal.h>
#include <asm/sigcontext.h>

int old_instruction;

int
SetBreakPoint(void *pc)
{
	old_instruction = *((unsigned char *) pc);
	*((unsigned char *) pc) = 0xcc; /* break point opcode */
#ifdef DEBUG
	fprintf(stderr, "breakpoint at: %x\n", (unsigned int) pc);
#endif
	/* Need to flush icache here */
	return old_instruction;
}

extern "C" void
sigtrap_handler(int sig, struct sigcontext_struct context)
{
#ifdef DEBUG
	fprintf(stderr, "Handler: Restoring old instruction\n");
	fprintf(stderr, "ip: %x\n", (unsigned int) context.eip);
#endif
	
	/* Learnt this the hard way. eip points to the next instruction */
	(*((unsigned char *) (context.eip -1))) = old_instruction;
	context.eip = context.eip - 1;

	/* Send a debugger event and do all kinds of things */
	
	return;
}

void
SetupBreakPointHandler()
{
	struct sigaction sigtrap_action;

	sigemptyset(&sigtrap_action.sa_mask);
	sigtrap_action.sa_flags = 0;
	sigtrap_action.sa_handler = (__sighandler_t) sigtrap_handler;

	sigaction(SIGTRAP, &sigtrap_action, NULL);
}
