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
//  x86StdCall.cpp - StdCall calling convention
//

#include "x86StdCall.h"
#include "x86Win32Emitter.h"

// Utility function to push one function-call argument on the stack
void
pushCallArg(DataConsumer& argument, DataNode* usesNode, InstructionDefine*& orderingDependency, Pool& inPool, x86Win32Emitter& inEmitter)
{
    // Do the first argument specially because that sets up the chain of ordering dependencies
    int numOrderingUses = orderingDependency ? 1 : 0;
    
    // If pushing constant arguments, use the immediate form of the push instruction
    if (argument.getVariable().getCategory() == pcConst) {       
        switch (argument.getKind()) {

        // 32-bit constants
        case vkInt:
        case vkAddr:
        case vkFloat:
            {
                Uint32 constant = PrimConst::cast(argument.getVariable()).value.i;
                x86Instruction& pushInsn = *new(inPool) x86Instruction(usesNode, inPool, iaPushImm, constant, atImmediateOnly, numOrderingUses, 1);
                
                // First arg is handled differently
                if (orderingDependency) {
                    inEmitter.useTemporaryOrder(pushInsn, *orderingDependency, 0);
                    inEmitter.redefineTemporaryOrder(pushInsn, *orderingDependency, 0);
                } else {
                    orderingDependency = &inEmitter.defineTemporaryOrder(pushInsn, 0);
                }
            }
            break;
            
        // 64-bit constants require two immediate-form push instructions
        case vkDouble:
        case vkLong:
            {
                Int64 constant = PrimConst::cast(argument.getVariable()).value.l;
                Int32 low  = (Int32)((Uint64)constant & 0xFFFFFFFF);
                Int32 high = (Int32)((Uint64)constant >> 32);
                
                x86Instruction& pushInsnHi = *new(inPool) x86Instruction(usesNode, inPool, iaPushImm, high, atImmediateOnly, numOrderingUses, 1);
                
                // First arg is handled differently
                if (orderingDependency) {
                    inEmitter.useTemporaryOrder(pushInsnHi, *orderingDependency, 0);
                    inEmitter.redefineTemporaryOrder(pushInsnHi, *orderingDependency, 0);
                } else {
                    orderingDependency = &inEmitter.defineTemporaryOrder(pushInsnHi, 0);
                }
                
                x86Instruction& pushInsnLo = *new(inPool) x86Instruction(usesNode, inPool, iaPushImm, low, atImmediateOnly, 1, 1);
                inEmitter.useTemporaryOrder(pushInsnLo, *orderingDependency, 0);
                inEmitter.redefineTemporaryOrder(pushInsnLo, *orderingDependency, 0);
            }
            break;

        default:
            assert(0);
        }
        return;
    }
    
    // Push non-constant function call argument
    switch (argument.getKind()) { 
        
    case vkInt:
    case vkAddr:
        {
            x86Instruction&	storeParam = *new(inPool) x86Instruction(usesNode, inPool, ePush, atRegDirect, 1 + numOrderingUses, 1);
            inEmitter.useProducer(argument.getVariable(), storeParam, 0);
            
            // First arg is handled differently
            if (orderingDependency) {
                inEmitter.useTemporaryOrder(storeParam, *orderingDependency, 1);
                inEmitter.redefineTemporaryOrder(storeParam, *orderingDependency, 0);
            } else {
                orderingDependency = &inEmitter.defineTemporaryOrder(storeParam, 0);
            }
        }
        break;
        
    case vkFloat:
        {
            InsnDoubleOpDir& copyParamInsn = *new(inPool) InsnDoubleOpDir(usesNode, inPool, raCopyI, atRegAllocStackSlot, atRegDirect, 1, 1);
            inEmitter.useProducer(argument.getVariable(), copyParamInsn, 0, vrcStackSlot);
            VirtualRegister& vr = inEmitter.defineTemporary(copyParamInsn, 0);
            
            x86Instruction&	storeParamInsn = *new(inPool) x86Instruction(usesNode, inPool, ePush, atRegDirect, 1 + numOrderingUses, 1);
            inEmitter.useTemporaryVR(storeParamInsn, vr, 0);
            
            // First arg is handled differently
            if (orderingDependency) {
                inEmitter.useTemporaryOrder(storeParamInsn, *orderingDependency, 1);
                inEmitter.redefineTemporaryOrder(storeParamInsn, *orderingDependency, 0);
            } else {
                orderingDependency = &inEmitter.defineTemporaryOrder(storeParamInsn, 0);
            }
        }
        break;
        
    case vkDouble:
        {
            // Push high 32-bits of double
            InsnDoubleOpDir& copyParamInsnHi = *new(inPool) InsnDoubleOpDir(usesNode, inPool, raCopyI, atRegAllocStackSlotHi32, atRegDirect, 1, 2);
            inEmitter.useProducer(argument.getVariable(), copyParamInsnHi, 0, vrcStackSlot);
            VirtualRegister& vrHi = inEmitter.defineTemporary(copyParamInsnHi, 0);
            
            x86Instruction&	storeParamInsnHi = *new(inPool) x86Instruction(usesNode, inPool, ePush, atRegDirect, 1 + numOrderingUses, 1);
            inEmitter.useTemporaryVR(storeParamInsnHi, vrHi, 0);

            // First arg is handled differently
            if (orderingDependency) {
                inEmitter.useTemporaryOrder(storeParamInsnHi, *orderingDependency, 1);
                inEmitter.redefineTemporaryOrder(storeParamInsnHi, *orderingDependency, 0);
            } else {
                orderingDependency = &inEmitter.defineTemporaryOrder(storeParamInsnHi, 0);
            }
            
            // Push low 32-bits of double
            InsnDoubleOpDir& copyParamInsnLo = *new(inPool) InsnDoubleOpDir(usesNode, inPool, raCopyI, atRegAllocStackSlot, atRegDirect, 1, 2);
            inEmitter.useProducer(argument.getVariable(), copyParamInsnLo, 0, vrcStackSlot);
            VirtualRegister& vrLo = inEmitter.defineTemporary(copyParamInsnLo, 0);
            
            x86Instruction&	storeParamInsnLo = *new(inPool) x86Instruction(usesNode, inPool, ePush, atRegDirect, 2, 1);
            inEmitter.useTemporaryVR(storeParamInsnLo, vrLo, 0);
            inEmitter.useTemporaryOrder(storeParamInsnLo, *orderingDependency, 1);
            inEmitter.redefineTemporaryOrder(storeParamInsnLo, *orderingDependency, 0);
        }
        break;
        
    case vkLong:
        {
            x86Instruction&	storeParamHigh = *new(inPool) x86Instruction(usesNode, inPool, ePush, atRegDirect, 1 + numOrderingUses, 1);
            inEmitter.useProducer(argument.getVariable(), storeParamHigh, 0, vrcInteger, vidHigh);

            // First arg is handled differently
            if (orderingDependency) {
                inEmitter.useTemporaryOrder(storeParamHigh, *orderingDependency, 1);
                inEmitter.redefineTemporaryOrder(storeParamHigh, *orderingDependency, 0);
            } else {
                orderingDependency = &inEmitter.defineTemporaryOrder(storeParamHigh, 0);
            }
            
            x86Instruction&	storeParamLow = *new(inPool) x86Instruction(usesNode, inPool, ePush, atRegDirect, 2, 1);
            inEmitter.useProducer(argument.getVariable(), storeParamLow, 0, vrcInteger, vidLow);
            inEmitter.useTemporaryOrder(storeParamLow, *orderingDependency, 1);				
            inEmitter.redefineTemporaryOrder(storeParamLow, *orderingDependency, 0);
        }
        break;
        
    default:
        assert(false);
    }
}


void x86Win32Emitter::
emitArguments(ControlNode::BeginExtra& inBeginNode)
{
	if (inBeginNode.nArguments == 0)
		return;

	InsnExternalDefine& defineInsn = *new(mPool) InsnExternalDefine(&inBeginNode[0], mPool, inBeginNode.nArguments * 2);
	
	// do appropriate loads for each argument
	Uint32 curStackOffset = 8;

	Uint8 curIndex = 0;  // Index into defineInsn 
	unsigned int curArgument;
	for (curArgument = 0; curArgument < inBeginNode.nArguments; curArgument++)
	{
		PrimArg&	curArg = inBeginNode[curArgument];

		switch (curArg.getOperation()) 
		{
			case poArg_I:
			case poArg_A:
				if (curArg.hasConsumers())
				{

					InsnDoubleOpDir& loadParam = *new(mPool) InsnDoubleOpDir(&curArg, mPool, raLoadI, curStackOffset, atStackOffset, atRegDirect, 1, 1);
					useTemporaryOrder(loadParam, defineTemporaryOrder(defineInsn, curIndex), 0);
					defineProducer(curArg, loadParam, 0);	
				}
				curIndex++;
				curStackOffset += 4;
				break;

            case poArg_F:
                if (curArg.hasConsumers())
                    emit_ArgF(curArg, defineTemporaryOrder(defineInsn, curIndex), curStackOffset);
				curIndex++;
				curStackOffset += 4;
				break;

			case poArg_L:
				if (curArg.hasConsumers())
				{
					InsnDoubleOpDir& loadHi = *new(mPool) InsnDoubleOpDir(&curArg, mPool, raLoadI, curStackOffset + 4, atStackOffset, atRegDirect, 1, 1);
					InsnDoubleOpDir& loadLo = *new(mPool) InsnDoubleOpDir(&curArg, mPool, raLoadI, curStackOffset, atStackOffset, atRegDirect, 1, 1);
					useTemporaryOrder(loadLo, defineTemporaryOrder(defineInsn, curIndex), 0);
					useTemporaryOrder(loadHi, defineTemporaryOrder(defineInsn, curIndex + 1), 0);
					defineProducer(curArg, loadLo, 0, vrcInteger, vidLow);	
					defineProducer(curArg, loadHi, 0, vrcInteger, vidHigh);	
				} 
				curIndex += 2;
				curStackOffset += 8;
				break;

			case poArg_D:
                if (curArg.hasConsumers())
                    emit_ArgD(curArg, defineTemporaryOrder(defineInsn, curIndex), curStackOffset);
                curIndex++;
				curStackOffset += 8;
                break;

			default:
				assert(false);
		}
	}
	
	// continue counting from before
	// make all the rest of the defines as type none
	for (;curIndex < inBeginNode.nArguments * 2; curIndex++)
	{
		defineInsn.getInstructionDefineBegin()[curIndex].kind = udNone;
	}
}

