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
/*
	InstructionEmitter.h

	Scott M. Silver
*/

#ifndef _H_INSTRUCTIONEMITTER_
#define _H_INSTRUCTIONEMITTER_

#include "Instruction.h"
#include "VirtualRegister.h"
#include "Primitives.h"
#include "Value.h"

class Pool;
struct InstructionDefine;

typedef uint16 NamedRule;

enum VirtualRegisterID { vidLow, vidHigh };

class InstructionEmitter
{
public:
	InstructionEmitter(Pool& inPool, VirtualRegisterManager& vrMan) : 
		mPool(inPool), 
		mVRAllocator(vrMan) { }
					
	VirtualRegister& 	defineTemporary(Instruction& inDefineInsn, Uint8 inWhichOutput, VirtualRegisterKind constraint = IntegerRegister);
	VirtualRegister&	redefineTemporary(Instruction& inDefineInsn, VirtualRegister& inReuseRegister, Uint8 inWhichOutput, VirtualRegisterKind constraint = IntegerRegister);
	void				useTemporaryVR(Instruction& inInstruction, VirtualRegister& inVR, uint8 inWhichInput, VirtualRegisterKind constraint = IntegerRegister);
	
	InstructionDefine&	defineTemporaryOrder(Instruction& inDefineInsn, Uint8 inWhichOutput);
	InstructionDefine&	redefineTemporaryOrder(Instruction& inDefineInsn, InstructionDefine& inReuseOrder, Uint8 inWhichOutput);
	void				useTemporaryOrder(Instruction& inDefineInsn, InstructionDefine& inOrder, Uint8 inWhichInput);

	VirtualRegister*	defineProducer(DataNode& inProducer, Instruction& inDefineInsn, Uint8 inWhichOutput, VirtualRegisterKind constraint = IntegerRegister, VirtualRegisterID inID = vidLow);
	void				defineProducerWithExistingVirtualRegister(VirtualRegister& inVr, DataNode& inProducer, Instruction& inDefineInsn, Uint8 inWhichOutput, VirtualRegisterKind constraint = IntegerRegister);
	VirtualRegister* 	useProducer(DataNode& inProducer, Instruction& inUseInstruction, Uint8 inWhichInput, VirtualRegisterKind constraint = IntegerRegister, VirtualRegisterID inID = vidLow);
	Instruction& 		pushAbsoluteBranch(ControlNode& inSrc, ControlNode& inTarget);


private:
	VirtualRegister* 	defineProducerInternal(DataNode& inProducer, Instruction* inDefineInsn = NULL, uint8 inWhichOutput = 0, VirtualRegister* inVr = NULL, VirtualRegisterKind constraint = IntegerRegister, VirtualRegisterID inID = vidLow);

	virtual Instruction& emitAbsoluteBranch(DataNode& inDataNode, ControlNode& inTarget) = 0;

protected:
	Pool&	mPool;

public:
	VirtualRegisterManager& mVRAllocator;
};
#endif // _H_INSTRUCTIONEMITTER_
