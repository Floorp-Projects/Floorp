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
// DataOutput.cpp
//
// Scott M. Silver


#include <assert.h>
#include <stdio.h>
#include <Windows.h>
#include <ImageHlp.h>
#include "Debugee.h"
#include "XDisAsm.h"
#include "Win32Util.h"
#include "DataOutput.h"
#include "DebuggerChannel.h"
#include "Breakpoints.h"

void* disassembleToString(DebugeeProcess& inProcess, void* inStart, const char* inOutputBuffer);
void* printOneInstruction(DebugeeProcess& inProcess, void* inStart);

extern bool gCommandLineLoopActive;
extern "C" rl_clear_screen();
extern DebugeeThread* gCurThread;


void
disassembleBytes(DebugeeProcess& inProcess, const char* inStart, int /*length*/)
{
	printOneInstruction(inProcess, (void*) inStart);
	putchar('\n');
}


void*
disassembleToString(DebugeeProcess& inProcess, void* inStart, char* outOutputBuffer)
{
	unsigned char data[32];
	if (!inProcess.readMemory(inStart, &data, 32, NULL))
		showLastError();

	// if the current pc is the site of breakpoint
	// we replace the breakpoint instruction with 
	// the replaced instruction
	Breakpoint* bp = BreakpointManager::findBreakpoint(inStart);
	if (bp)
		data[0] = bp->getReplaced();

	unsigned char* tempStart = data;

	if (data[0] == 0xe8 || data[0] == 0xe9)
	{
		// call/jmp with 32 bit offset relative to pc from end of the instruction
		char	symbol[512];
		DWORD	offset;
		void*	address = (void*) ((Uint8*) inStart + *((Int32*) (data + 1)) + 5); // 5 is length of instruction
		if (inProcess.getSymbol(address, symbol, sizeof(symbol), offset))
		{
			sprintf(outOutputBuffer, "%s\t  %s + %d; %p", (data[0] == 0xe8) ? "call" : "jmp", symbol, offset, address);
			goto FinishDisassembling;
		}
		// else false through and do the normal thing
	}

	// regular instruction
	char* str;
	str = disasmx86(0, (char**) &tempStart, (char*)data + 32, kDisAsmFlag32BitSegments);
	strcpy(outOutputBuffer, str);

FinishDisassembling:
	tempStart = data;
	return ((void*) ((char*) inStart + x86InstructionLength(0, (char**)&tempStart, (char*) data + 32, kDisAsmFlag32BitSegments)));
}


DebugeeProcess::SymbolKind
printMethodName(DebugeeProcess& inProcess, const void* inMethodAddress)
{
	char						symbol[512];
	DWORD						offset;
	DebugeeProcess::SymbolKind	kind;

	if ((kind = inProcess.getSymbol(inMethodAddress, symbol, sizeof(symbol), offset)) != DebugeeProcess::kNil)
		printf("%s + %d", symbol, offset);

	return (kind);
}


void*
printOneInstruction(DebugeeProcess& inProcess, void* inStart)
{
	void*	nextPC;
	char	buffer[1024];

	nextPC = disassembleToString(inProcess, inStart, buffer);
	printf("%.8x: %s", inStart, buffer);
	return (nextPC);
}


void*
disassembleN(DebugeeProcess& inProcess, const char* inStart, int inInstructions)
{
	int		curInstruction;
	void*	curPC =  (void*) inStart;

	if (printMethodName(inProcess, inStart));
		putchar('\n');

	for (curInstruction = 0; curInstruction < inInstructions; curInstruction++)
	{
		curPC = printOneInstruction(inProcess, curPC);
		printf("\n");
	}

	return (curPC);
}


void beginOutput()
{
	if (gCommandLineLoopActive)
		putchar('\n');
}


void endOutput()
{
	if (gCommandLineLoopActive)
		rl_clear_screen();
}


void 
printThreads(DebugeeProcess& inProcess)
{
	Vector<DebugeeThread*>	threads = inProcess.getThreads();

	printf(" %4s%5s%10s(%5s)%9s %s\n", "id", "hndl", "state", "cnt", "eip", "symbol");

	DebugeeThread**	curThread;

	for(curThread = threads.begin(); curThread < threads.end(); curThread++)
	{
		if (gCurThread == *curThread)
			printf("*");
		else
			printf(" ");

		(*curThread)->print();	
		printf("\n");
	}
}


void
printThreadStack(DebugeeThread& inThread)
{
	STACKFRAME			stackFrame;
	CONTEXT				context;

	::ZeroMemory(&stackFrame, sizeof(stackFrame));

	inThread.getContext(CONTEXT_FULL | CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS, context);
	stackFrame.AddrPC.Offset       = context.Eip;
	stackFrame.AddrPC.Mode         = AddrModeFlat;

	stackFrame.AddrStack.Offset    = context.Esp;
	stackFrame.AddrStack.Mode      = AddrModeFlat;

	stackFrame.AddrFrame.Offset    = context.Ebp;
	stackFrame.AddrFrame.Mode      = AddrModeFlat;

	DebugeeProcess::SymbolKind	state;

	for (;;)
	{
		if (stackFrame.AddrPC.Offset == 0)
			break;

		state = printMethodName(inThread.getProcess(), (void*) stackFrame.AddrPC.Offset);

		if (state == DebugeeProcess::kJava)
		{
			printf("\n");
			inThread.getProcess().readMemory(((DWORD*)stackFrame.AddrFrame.Offset + 1), &stackFrame.AddrPC.Offset, 4);
			inThread.getProcess().readMemory((void*) stackFrame.AddrFrame.Offset, &stackFrame.AddrFrame.Offset, 4);
			stackFrame.AddrStack.Offset = stackFrame.AddrFrame.Offset;
		}
		else if (state == DebugeeProcess::kNative)
		{
			printf("\n");
			if (!::StackWalk(IMAGE_FILE_MACHINE_I386,
						inThread.getProcess().getProcessHandle(),
						inThread.getThreadHandle(),
						&stackFrame,
						NULL,
						NULL,
						SymFunctionTableAccess,
						SymGetModuleBase,
						NULL))
			{
				inThread.getProcess().readMemory(((DWORD*)stackFrame.AddrFrame.Offset + 1), &stackFrame.AddrPC.Offset, 4);
				inThread.getProcess().readMemory((void*) stackFrame.AddrFrame.Offset, &stackFrame.AddrFrame.Offset, 4);
				stackFrame.AddrStack.Offset = stackFrame.AddrFrame.Offset;
			}

		}
		else
		{
			// try one more native frame
			if (::StackWalk(IMAGE_FILE_MACHINE_I386,
						inThread.getProcess().getProcessHandle(),
						inThread.getThreadHandle(),
						&stackFrame,
						NULL,
						NULL,
						SymFunctionTableAccess,
						SymGetModuleBase,
						NULL))
				state = printMethodName(inThread.getProcess(), (void*) stackFrame.AddrPC.Offset);
			else
			{
				printf("<anonymous>\n");
				break;
			}
		}
	}
//	showLastError();
}


void
printIntegerRegs(DebugeeThread& inThread)
{
	CONTEXT	context;
	gCurThread->getContext(CONTEXT_INTEGER | CONTEXT_CONTROL, context);

	printf(	"EDI=%8p ESI=%8p EBX=%8p EDX=%8p\n"
			"ECX=%8p EAX=%8p EBP=%8p ESP=%8p\n"
			"EIP=%8p\n", context.Edi, context.Esi, 
			context.Ebx, context.Edx, context.Ecx, 
			context.Eax, context.Ebp, context.Esp, 
			context.Eip);
}


void
printFlags(DebugeeThread& inThread)
{
	CONTEXT	context;
	gCurThread->getContext(CONTEXT_CONTROL, context);
	const char* flagString = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	const char* cp = flagString;

	while (context.EFlags)
	{
		if (context.EFlags & 1)
			putchar(*cp);
		else
			putchar(*cp - 32);

		cp++;
		context.EFlags >>= 1;
	}
}

