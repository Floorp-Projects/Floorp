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
//
//  x86Float.h - Floating-point code-generation for x86 processors
//

#ifndef X86_FLOAT_H
#define X86_FLOAT_H

#include "x86Win32Instruction.h"

//-----------------------------------------------------------------------------------------------------------
// Floating-point instructions in which one operand is a memory location.
// If there is a second operand, then it is implicitly the top of the FPU stack.

class  InsnFloatMemory :
  public x86ArgListInstruction
{
public:
    InsnFloatMemory(DataNode* inPrimitive, Pool& inPool, x86FloatMemoryType inOpInfo, int inX, int inY): 
        x86ArgListInstruction (inPrimitive, inPool, inX, inY )
        {
            opcodeInfo = &opcodeTable[inOpInfo];
            iArgumentList = new x86SingleArgumentList(atRegAllocStackSlot);
        }

    InsnFloatMemory(DataNode* inPrimitive, Pool& inPool, x86FloatMemoryType inOpInfo, x86ArgumentType argType,
                    Int32 displacement, int inX, int inY): 
        x86ArgListInstruction (inPrimitive, inPool, inX, inY )
        {
            opcodeInfo = &opcodeTable[inOpInfo];
            iArgumentList = new x86SingleArgumentList(argType, displacement);
        }

    InsnFloatMemory(DataNode* inPrimitive, Pool& inPool, x86FloatMemoryType inOpInfo, x86ArgumentType argType,
                    int inX, int inY): 
        x86ArgListInstruction (inPrimitive, inPool, inX, inY )
        {
            opcodeInfo = &opcodeTable[inOpInfo];
            iArgumentList = new x86SingleArgumentList(argType);
        }

    virtual Uint8		opcodeSize() {return 1;}

	virtual void		formatToMemory(void * /*inStart*/, Uint32 /*inCurOffset*/, MdFormatter& /*inEmitter*/);

	// spilling
    virtual bool		switchUseToSpill(Uint8 /*inWhichUse*/, VirtualRegister &/*inVR*/) { return false; }
    virtual bool		switchDefineToSpill(Uint8 /*inWhichDefine*/, VirtualRegister &/*inVR*/) { return false; }

	// Control Spilling
    virtual bool		opcodeAcceptsSpill() { return false; }
    virtual void		switchOpcodeToSpill() { assert(0); }

protected:
	x86OpcodeInfo*      opcodeInfo;

private:
    static x86OpcodeInfo opcodeTable[];

public:
#ifdef DEBUG_LOG
	// Debugging Methods
    virtual void		printOpcode(LogModuleObject &f) { UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%-9s", opcodeInfo->oiText)); }
#endif // DEBUG_LOG
};

//-----------------------------------------------------------------------------------------------------------
// Floating-point instructions in which all operands and results are implicitly on the FPU stack

enum x86FloatRegType
{
    fucomip,
    fneg,
    fnstsw
};

class  InsnFloatReg :
  public InsnUseXDefineYFromPool
{
public:
    InsnFloatReg(DataNode* inPrimitive, Pool& inPool, x86FloatRegType inOpInfo, Uint8 inX, Uint8 inY): 
        InsnUseXDefineYFromPool (inPrimitive, inPool, inX, inY ) 
        {
            opcodeInfo = &opcodeTable[inOpInfo];
        }

    virtual Uint8		opcodeSize() {return 2;}

    virtual size_t  	getFormattedSize(MdFormatter& /*inFormatter*/) { return 2; }; 
	virtual void		formatToMemory(void * /*inStart*/, Uint32 /*inCurOffset*/, MdFormatter& /*inEmitter*/);

	// spilling
    virtual bool		switchUseToSpill(Uint8 /*inWhichUse*/, VirtualRegister &/*inVR*/) { return false; }
    virtual bool		switchDefineToSpill(Uint8 /*inWhichDefine*/, VirtualRegister &/*inVR*/) { return false; }

	// Control Spilling
    virtual bool		opcodeAcceptsSpill() { return false; }
    virtual void		switchOpcodeToSpill() { assert(0); }

protected:
	x86OpcodeInfo*      opcodeInfo;

private:
    static x86OpcodeInfo opcodeTable[];

public:
#ifdef DEBUG_LOG
	// Debugging Methods
    virtual void		printPretty(LogModuleObject &f) { UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%-9s", opcodeInfo->oiText)); }
#endif // DEBUG_LOG
};

#endif // X86_FLOAT_H

