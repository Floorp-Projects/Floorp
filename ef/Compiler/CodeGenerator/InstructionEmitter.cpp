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
//	InstructionEmitter.cp	
//
//	Scott M. Silver
//	

#include "InstructionEmitter.h"
#include "Primitives.h"
#include "Instruction.h"
#include "ControlNodes.h"

// define a temporary platform specific register
VirtualRegister& InstructionEmitter::
defineTemporary(Instruction& inDefineInsn, Uint8 inWhichOutput, VirtualRegisterKind constraint)
{
	// newRegister stores inDefineInsn in the vr	
	VirtualRegister& vr = mVRAllocator.newVirtualRegister(constraint);

	vr.setDefiningInstruction(inDefineInsn);
	
	// make this instruction's output this register
	inDefineInsn.addDefine(inWhichOutput, vr, constraint);

	return vr;
}

// reuse register, basically update who defines this register
// and note that the passed in instruction redefines it
VirtualRegister& InstructionEmitter::
redefineTemporary(Instruction& inDefineInsn, VirtualRegister& inReuseRegister, Uint8 inWhichOutput, VirtualRegisterKind constraint)
{
	inReuseRegister.setDefiningInstruction(inDefineInsn);
	
	inDefineInsn.addDefine(inWhichOutput, inReuseRegister, constraint);	
	
	return (inReuseRegister);
}

InstructionDefine& InstructionEmitter::
defineTemporaryOrder(Instruction& inDefineInsn, Uint8 inWhichOutput)
{
	InstructionDefine&	newOrder = *new(mPool) InstructionDefine();
	newOrder.name.instruction =  &inDefineInsn;
	
	inDefineInsn.addDefine(inWhichOutput, newOrder);
	
	return (newOrder);
}

InstructionDefine& InstructionEmitter::
redefineTemporaryOrder(Instruction& inDefineInsn, InstructionDefine& inReuseOrder, Uint8 inWhichOutput)
{
	inReuseOrder.name.instruction = &inDefineInsn;
	
	inDefineInsn.addDefine(inWhichOutput, inReuseOrder);
	
	return (inReuseOrder);
}

void InstructionEmitter::
useTemporaryOrder(Instruction& inDefineInsn, InstructionDefine& inOrder, Uint8 inWhichInput)
{
	inDefineInsn.addUse(inWhichInput, inOrder);
}



// inID is used to indicate which of the two assigned registers to a producer
// you wish to use, eg for 32 bit types, etc
VirtualRegister* InstructionEmitter:: 
defineProducer(DataNode& inProducer, Instruction& inDefineInsn, Uint8 inWhichOutput, VirtualRegisterKind constraint, VirtualRegisterID inID)
{
	return defineProducerInternal(inProducer, &inDefineInsn, inWhichOutput, NULL, constraint, inID);
}

void InstructionEmitter::
defineProducerWithExistingVirtualRegister(VirtualRegister& inVr, DataNode& inProducer, Instruction& inDefineInsn, Uint8 inWhichOutput, VirtualRegisterKind constraint)
{
	defineProducerInternal(inProducer, &inDefineInsn, inWhichOutput, &inVr, constraint, vidLow);
}

inline VirtualRegister*
getVirtualRegisterAnnotationByID(DataNode& inDataNode, VirtualRegisterID inID)
{
	if (inID == vidLow)
		return (inDataNode.getLowVirtualRegisterAnnotation());
	else if (inID == vidHigh)
		return (inDataNode.getHighVirtualRegisterAnnotation());
	else
	{
		assert(false);
		return (NULL);
	}
}
								
				
// inDefineInsn default to NULL
// inWhichOutput default to 0xFF
VirtualRegister* InstructionEmitter::
defineProducerInternal(DataNode& inProducer, Instruction* inDefineInsn, Uint8 inWhichOutput, VirtualRegister* inVr, VirtualRegisterKind constraint, VirtualRegisterID inID)
{
	bool reallyDefiningProducer;
	
	reallyDefiningProducer = (inDefineInsn != NULL);

	switch (inProducer.getKind())
	{
		default:
			assert(false);
			break;
		case vkCond:
			// fix-me should we set the fact that this instruction
			// touches a condition code here?
			if (reallyDefiningProducer)
			{
				inProducer.annotate(*inDefineInsn);
				inDefineInsn->addDefine(inWhichOutput, inProducer);
				return (NULL);
			}
			break;
		case vkMemory:
			if (reallyDefiningProducer)
			{
				inProducer.annotate(*inDefineInsn);
				inDefineInsn->addDefine(inWhichOutput, inProducer);
				return (NULL);
			}
			break;	
		case vkDouble:
		case vkInt:
		case vkFloat:
		case vkAddr:
			assert(inID == vidLow);
		case vkLong:
			{	
				VirtualRegister* vr;
				
				// check to see if a VR was assigned to inProducer
				if (getVirtualRegisterAnnotationByID(inProducer, inID) == NULL)
				{
					// allocate a new VR
					if (inVr == NULL)
						vr = &mVRAllocator.newVirtualRegister(constraint);
					else
						vr = inVr;
					
					vr->setDefiningInstruction(*inDefineInsn);
					

					// attach to inProducer
					if (inID == vidLow)
						inProducer.annotateLow(*vr, constraint);
					else if (inID == vidHigh)
						inProducer.annotateHigh(*vr, constraint);
					else
						assert(false);
				}
				else
				{
					// there is already a VR attached to this producer
					// make sure user did not try to specify a VR, this is 
					// programmer error
					assert(inVr == NULL);
					
					vr = getVirtualRegisterAnnotationByID(inProducer, inID);
					
					vr->setDefiningInstruction(*inDefineInsn);
				}
	
				if (reallyDefiningProducer)
				  {
					inDefineInsn->addDefine(inWhichOutput, *vr, constraint);
					vr->setClass(constraint);
				  }
					
				return (vr);
			}
			break;
	}
	
	// NEVER REACHED
	return (NULL);
}


VirtualRegister* InstructionEmitter:: 
useProducer(DataNode& inProducer, Instruction& inUseInstruction, Uint8 inWhichInput, VirtualRegisterKind constraint, VirtualRegisterID inID)
{	
	// find out if there is a register assigned to this {producer, id}
	// fix-me check cast
	switch (inProducer.getKind())
	{
		default:
			assert(false);
			break;
		case vkCond:
			inUseInstruction.addUse(inWhichInput, inProducer);
			break;
		case vkMemory:
			// now mark our dependence on VR
			inUseInstruction.addUse(inWhichInput, inProducer);
			break;
		case vkDouble:
		case vkInt:
		case vkFloat:
		case vkLong:
		case vkAddr:
			VirtualRegister*	vr;

			// make sure there is a register assigned to this already
			// if not assign a new one with a NULL defining instruction
			if (getVirtualRegisterAnnotationByID(inProducer, inID) == NULL)
				vr = defineProducerInternal(inProducer, NULL, 0, NULL, constraint, inID); // define NULL instruction for this producer
			else
				vr = getVirtualRegisterAnnotationByID(inProducer, inID);
			
			// now mark our dependence on VR
			inUseInstruction.addUse(inWhichInput, *vr, constraint);
			return vr;
	}
	return NULL;
}

void InstructionEmitter::
useTemporaryVR(Instruction& inInstruction, VirtualRegister& inVR, Uint8 inWhichInput, VirtualRegisterKind constraint)
{
	inInstruction.addUse(inWhichInput, inVR, constraint);
}

Instruction& InstructionEmitter::
pushAbsoluteBranch(ControlNode& inSrc, ControlNode& inTarget)
{
	InstructionList& instructions = inSrc.getInstructions();
	
	Instruction& branchInsn = emitAbsoluteBranch(*(instructions.get(instructions.end())).getPrimitive(), inTarget);
		
	instructions.addLast(branchInsn);

	return (branchInsn);
}


