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
	Instruction.cpp

	Scott M. Silver
*/

#include "Instruction.h"
#include "Pool.h"
#include "InstructionEmitter.h"


Instruction::
Instruction(DataNode* inSrcPrimitive) 
{ 
	//DEBUG_ONLY(initialize());
	DEBUG_ONLY(mDebug = 0xDEADBEEF);
	mSrcPrimitive = inSrcPrimitive;
}



// unlinkRegisters
//
// remove all references to VirtualRegister
// for this Instruction.
void Instruction::
unlinkRegisters()
{
  InstructionUse* use_limit = getInstructionUseEnd();
  for(InstructionUse* use_ptr = getInstructionUseBegin(); use_ptr < use_limit; use_ptr++)
	if (use_ptr->kind == udRegister)
	  use_ptr->name.vr.uninitialize();

  InstructionDefine* def_limit = getInstructionDefineEnd();
  for(InstructionDefine* def_ptr = getInstructionDefineBegin(); def_ptr < def_limit; def_ptr++)
	if (def_ptr->kind == udRegister)
	  def_ptr->name.vr.uninitialize();
}

//#ifdef DEBUG
void Instruction::
initialize()
{
	// clear out use's
	InstructionUse*	curUse;
	
	for (	curUse = getInstructionUseBegin(); 
			curUse < getInstructionUseEnd();
			curUse++)
	{
		curUse[0].name.dp = NULL;
		curUse[0].src = NULL;
		curUse[0].kind = udUninitialized;
	}
	
	InstructionDefine*	curDefine;
	
	for (	curDefine = getInstructionDefineBegin(); 
			curDefine < getInstructionDefineEnd();
			curDefine++)
	{
		curDefine[0].name.dp = NULL;
		curDefine[0].kind = udUninitialized;
	}
}
//#endif


// addUse
//
//
void Instruction::					
addUse(Uint8 inWhichInput, VirtualRegister& inVR, VirtualRegisterKind constraint)
{
	InstructionUse* instructionUseBegin = getInstructionUseBegin();
	
	assert(instructionUseBegin != NULL && getInstructionUseEnd() - inWhichInput > instructionUseBegin);
	
	instructionUseBegin[inWhichInput].name.vr.initialize(inVR, constraint);
	instructionUseBegin[inWhichInput].src = inVR.getDefiningInstruction();
	instructionUseBegin[inWhichInput].kind = udRegister;
}


void Instruction::						
addUse(Uint8 inWhichInput, DataNode& inProducer)
{
	InstructionUse* instructionUseBegin = getInstructionUseBegin();
	
	assert(instructionUseBegin != NULL && getInstructionUseEnd() - inWhichInput > instructionUseBegin);
	assert(inProducer.hasKind(vkCond) || inProducer.hasKind(vkMemory));
	
	instructionUseBegin[inWhichInput].name.dp = &inProducer;
	
	if (inProducer.hasKind(vkCond))
		instructionUseBegin[inWhichInput].kind = udCond;
	else
		instructionUseBegin[inWhichInput].kind = udStore;
}


void Instruction::						
addUse(Uint8 inWhichInput, UseDefineKind inKind)
{
	InstructionUse* instructionUseBegin = getInstructionUseBegin();
	
	assert(instructionUseBegin != NULL && getInstructionUseEnd() - inWhichInput > instructionUseBegin);
	
	instructionUseBegin[inWhichInput].name.dp = NULL;
	instructionUseBegin[inWhichInput].kind = inKind;
}


void Instruction::
addUse(Uint8 inWhichInput, InstructionDefine& inUseOrder)
{
	InstructionUse* instructionUseBegin = getInstructionUseBegin();
	
	assert(instructionUseBegin != NULL && getInstructionUseEnd() - inWhichInput > instructionUseBegin);
	
	instructionUseBegin[inWhichInput].name.instruction = inUseOrder.name.instruction;
	instructionUseBegin[inWhichInput].kind = udOrder;
}


void Instruction::			
addDefine( Uint8 inWhichOutput, VirtualRegister& inVR, VirtualRegisterKind constraint)
{
	InstructionDefine* instructionDefineBegin = getInstructionDefineBegin();
	
	assert(instructionDefineBegin != NULL && getInstructionDefineEnd() - inWhichOutput > instructionDefineBegin);
	
	instructionDefineBegin[inWhichOutput].name.vr.initialize(inVR, constraint);
	instructionDefineBegin[inWhichOutput].kind = udRegister;
}


void Instruction::					
addDefine(Uint8 inWhichOutput, DataNode& inProducer)
{
	InstructionDefine* instructionDefineBegin = getInstructionDefineBegin();
	
	assert(instructionDefineBegin != NULL && getInstructionDefineEnd() - inWhichOutput > instructionDefineBegin);
	assert(inProducer.hasKind(vkCond) || inProducer.hasKind(vkMemory));
	
	if (inProducer.hasKind(vkCond))
		instructionDefineBegin[inWhichOutput].kind = udCond;
	else
		instructionDefineBegin[inWhichOutput].kind = udStore;
}


void Instruction::			
addDefine(Uint8 inWhichOutput, UseDefineKind inKind)
{
	InstructionDefine* instructionDefineBegin = getInstructionDefineBegin();
	
	assert(instructionDefineBegin != NULL && getInstructionDefineEnd() - inWhichOutput > instructionDefineBegin);
	
	instructionDefineBegin[inWhichOutput].name.dp = NULL;
	instructionDefineBegin[inWhichOutput].kind = inKind;
}


void Instruction::			
addDefine(Uint8 inWhichOutput, InstructionDefine& /*inDefineOrder*/)
{
	InstructionDefine* instructionDefineBegin = getInstructionDefineBegin();
	
	assert(instructionDefineBegin != NULL && getInstructionDefineEnd() - inWhichOutput > instructionDefineBegin);
	
	//instructionDefineBegin[inWhichOutput].name.instruction = this;
	instructionDefineBegin[inWhichOutput].kind = udOrder;
}

void Instruction::
standardUseDefine(InstructionEmitter& inEmitter)
{
	InstructionUse* 	instructionUseBegin = getInstructionUseBegin();
	Uint8				curIndex;					
	
	standardDefine(inEmitter);
	
	InstructionUse*		curUse;	

	if (instructionUseBegin != NULL)
	{
		for (curUse = instructionUseBegin, curIndex = 0; curUse < getInstructionUseEnd(); curUse++, curIndex++)
		{
			addStandardUse(inEmitter, curIndex);
		}
	}
}


VirtualRegister* Instruction::
addStandardUse(InstructionEmitter& inEmitter, Uint8 curIndex)
{
	assert (mSrcPrimitive != NULL);
	return inEmitter.useProducer(mSrcPrimitive->nthInputVariable(curIndex), *this, curIndex);	
}

void Instruction::
standardDefine(InstructionEmitter& inEmitter)
{
	InstructionDefine* 	instructionDefineBegin = getInstructionDefineBegin();
	Uint8				curIndex;					

	assert (mSrcPrimitive != NULL);
	
	if (instructionDefineBegin != NULL)
	{
		InstructionDefine*	curDefine;
		
		for (curDefine = instructionDefineBegin, curIndex = 0; curDefine < getInstructionDefineEnd(); curDefine++, curIndex++)
		{
			assert(curIndex == 0);
			inEmitter.defineProducer(*mSrcPrimitive, *this, curIndex);
			//inEmitter.defineProducer(nthOutputProducer(*mSrcPrimitive, curIndex), *this, curIndex);
		}		
	}
	else
	{
		mSrcPrimitive->setInstructionRoot(this);	
	}
}

#ifdef DEBUG_LOG
void Instruction::				
printDebug(LogModuleObject &f)
{
	printPretty(f);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
	// clear out use's
	InstructionUse*	curUse;
	int i;

	i=0;	
	for (	curUse = getInstructionUseBegin(); 
			curUse < getInstructionUseEnd();
			curUse++)
	{
		switch (curUse[0].kind)
		{
			case udRegister:
			  {
				VirtualRegister& vReg = curUse[0].getVirtualRegister();\
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("u[%d]: %scolor = %d index = %d \n", i, vReg.isPreColored() ? "pre" : "", vReg.isPreColored() ? 
						vReg.getPreColor() : vReg.getColor(),  vReg.getRegisterIndex()));
			  }
			  break;
			default:
			break;
		}	
		i++;
	}
	
	InstructionDefine*	curDefine;
	
	i=0;
	for (	curDefine = getInstructionDefineBegin(); 
			curDefine < getInstructionDefineEnd();
			curDefine++)
	{
		switch (curDefine[0].kind)
		{
			case udRegister:
			  {
				VirtualRegister& vReg = curDefine[0].getVirtualRegister();
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("d[%d]: %scolor = %d index = %d\n", i, vReg.isPreColored() ? "pre" : "",
						vReg.isPreColored() ? vReg.getPreColor() : vReg.getColor(), vReg.getRegisterIndex()));
			  }
			  break;
			default:
			break;
		}	
		i++;
	}
}
#endif

InsnUseXDefineYFromPool::
InsnUseXDefineYFromPool(DataNode* inSrcPrimitive, Pool& inPool, Uint8 inX, Uint8 inY) :
	Instruction(inSrcPrimitive)
{ 
	mX = inX;
	mY = inY;
	if (inX > 0)
		mInsnUse = new(inPool) InstructionUse[inX];
	else 
		mInsnUse = NULL;
	
	if (inY > 0)
		mDefineResource = new(inPool) InstructionDefine[inY];
	else
		mDefineResource = NULL;
		
	initialize();
}
