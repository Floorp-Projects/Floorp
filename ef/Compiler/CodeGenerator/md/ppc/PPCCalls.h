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
// PPCCalls.h
//
// Scott M. Silver
//

#ifndef _H_PPCCALLS
#define _H_PPCCALLS

#include "PPC601AppleMacOSEmitter.h"
#include "AssembledInstructions.h"

template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic>
class Call :
	public PPCInstructionXY
{
public:
	static inline bool 			hasReturnValue(DataNode& inDataNode);				// determines whether Call Primitive has return value
	static inline uint8			numberOfArguments(DataNode& inDataNode);			// determines number of real arguments (not including store)

								Call(DataNode*	inDataNode, Pool& inPool, uint8 inRegisterArguments, bool inHasReturnValue, PPCEmitter& inEmitter, void* inFunc = NULL);

	virtual void				formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& inFormatter);
	virtual size_t				getFormattedSize(MdFormatter& /*inFormatter*/) { return (8); }
	virtual InstructionFlags	getFlags() const { return (ifCall); }


protected:
	int16						mTOCOffset;				// offset in accumulator TOC 
	TVector*					mCalleeAddress;			// addres of cuntion 

#ifdef DEBUG
public:
	virtual void				printPretty(FILE* f) { fprintf(f, "call %X", mCalleeAddress); }
#endif
};


template<bool tHasIncomingStore, bool tHasOutgoingStore>
class CallS :
	public Call<tHasIncomingStore, tHasOutgoingStore, false, false>
{
public:
	inline 	CallS(DataNode*	inDataNode, Pool& inPool, uint8 inRegisterArguments, bool inHasReturnValue, PPCEmitter& inEmitter, void* inFunc) :
				Call<tHasIncomingStore, tHasOutgoingStore, false, false>(inDataNode, inPool, inRegisterArguments, inHasReturnValue, inEmitter, inFunc) { }
};

#ifdef MANUAL_TEMPLATES
template class Call<true, true, false, false>;
template class Call<true, false, false, false>;
template class Call<false, false, false, false>;
template class Call<true, true, true, false>;
template class Call<true, true, true, true>;
#endif

typedef CallS<true, true> CallS_V;
typedef CallS<true, false> CallS_;
typedef CallS<false, false>	CallS_C;
typedef Call<true, true, true, false> Call_;


// Dynamically dispatched call
class CallD_ :
	public Call<true, true, true, true>
{
public:
	inline CallD_(DataNode*	inDataNode, Pool& inPool, uint8 inRegisterArguments, bool inHasReturnValue, PPCEmitter& inEmitter) :
			Call<true, true, true, true>(inDataNode, inPool, inRegisterArguments, inHasReturnValue, inEmitter) { }		

	void	formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& inFormatter);
};


template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic> bool
Call<tHasIncomingStore, tHasOutgoingStore, tHasFunctionAddress, tIsDynamic>::
hasReturnValue(DataNode& inDataNode)
{
	bool hasReturnValue = (inDataNode.getOutgoingEdgesBegin() + tHasOutgoingStore < inDataNode.getOutgoingEdgesEnd());
		
	return (hasReturnValue);
}

template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic> uint8
Call<tHasIncomingStore, tHasOutgoingStore,tHasFunctionAddress, tIsDynamic>::
numberOfArguments(DataNode& inDataNode)
{
	DataConsumer*	firstArg;
	DataConsumer*	lastArg;
	
	assert(!(tHasFunctionAddress && !tHasIncomingStore));	// no such primitive
	firstArg = inDataNode.getInputsBegin() + tHasFunctionAddress + tHasIncomingStore;
	lastArg = inDataNode.getInputsEnd();
	
	return (lastArg - firstArg);
}


// -> mem regarg1 regarg2 regarg3 regarg3 
// <- mem [returnval]
template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic> 
Call<tHasIncomingStore, tHasOutgoingStore, tHasFunctionAddress, tIsDynamic>::
Call(DataNode*	inDataNode, Pool& inPool, uint8 inRegisterArguments, bool /*inHasReturnValue*/, PPCEmitter& inEmitter, void* inFunc) :
	PPCInstructionXY(inDataNode, inPool, inRegisterArguments + tHasIncomingStore + tIsDynamic, 1 + tHasOutgoingStore)
{
	if (!tIsDynamic)
		inEmitter.mCrossTOCPtrGlCount++;

 	const DoublyLinkedList<DataConsumer>& 		projectionConsumers = inDataNode->getConsumers();
 	DoublyLinkedList<DataConsumer>::iterator 	curProjectionEdge = projectionConsumers.begin();
	DataNode*									returnValProducer;
	
	// outgoing store
	if (tHasOutgoingStore)
	{	
		DataNode&	projectionA = projectionConsumers.get(curProjectionEdge).getNode();
		DataNode*	projectionB = (projectionConsumers.done(curProjectionEdge = projectionConsumers.advance(curProjectionEdge))) ? 0 : &projectionConsumers.get(curProjectionEdge).getNode();

		if (projectionA.hasKind(vkMemory))
		{
			inEmitter.defineProducer(projectionA, *this, 0);		// -> mem
			returnValProducer = projectionB;
		}
		else
		{
			assert(projectionB);									// projectionB must be the memory producer
			inEmitter.defineProducer(*projectionB, *this, 0);		// -> mem
			returnValProducer = &projectionA;
		}
	}
	else
		returnValProducer = (projectionConsumers.done(curProjectionEdge = projectionConsumers.advance(curProjectionEdge))) ? 0 : &projectionConsumers.get(curProjectionEdge).getNode();
		
	if (returnValProducer)
	{
		VirtualRegister*	returnValVR;
	
		// the Call defines a temporary register, which is precolored to 
		// the appropriate return register
		returnValVR = &inEmitter.defineTemporary(*this, 1);
		
		switch (returnValProducer->getKind())
		{
			case vkInt: case vkAddr:
			{
				returnValVR->preColorRegister(kR3Color);
				
				// now create a "buffer" copy between the precolored return register
				// and make the Copy define the return value edge from the pkCall
				Copy_I& 	copyInsn =  *new(inPool) Copy_I(inDataNode, inPool);
				inEmitter.useTemporaryVR(copyInsn, *returnValVR, 0);
				inEmitter.defineProducer(*returnValProducer, copyInsn, 0);
				break;
			}
			case vkLong:
				assert(false);
				break;			
			case vkFloat: case vkDouble:
				assert(false);
				break;
			default:
				assert(false);
		}
	}
	else
		getInstructionDefineBegin()[1].kind = udNone;		// zero out the unused outgoing edge
	
	DataConsumer*	firstArgument;
	DataConsumer*	curArgument;
	
	// incoming store
	if (tHasIncomingStore)
		inEmitter.useProducer(inDataNode->nthInputVariable(0), *this, 0);	// -> mem

	firstArgument = inDataNode->getInputsBegin() + tHasFunctionAddress + tHasIncomingStore;
		
	if (tHasFunctionAddress)
	{
		assert(inFunc == NULL);	// programmer error to specify a function address if this Call has a function address

		if (tIsDynamic)
			inEmitter.useProducer(inDataNode->nthInputVariable(1), *this, 1);			// -> incoming address
		else
			mCalleeAddress = (TVector*) addressFunction(PrimConst::cast(inDataNode->nthInputVariable(1)).value.a);
	}
	else
		mCalleeAddress = (TVector*) inFunc;
	
	inEmitter.mAccumulatorTOC.addData(&mCalleeAddress, sizeof(TVector), mTOCOffset);
	
	// move all arguments <= 8 words into registers, handle most of the stack passed arguments later			
	uint8			curFixedArg = 0;									// number of words passes as fixed arguments
	uint8			curFloatArg = 0;									// number of words passed as float arguments
	uint8			curTotalArgNumber = tHasIncomingStore + tIsDynamic;	// use number, which starts at 1 after the store, if it exists
	
	for (curArgument = firstArgument; curArgument < inDataNode->getInputsEnd(); curArgument++, curTotalArgNumber++)
	{
		VirtualRegister*	vr;
		
		switch (curArgument->getKind())
		{
			case vkInt:
			case vkAddr:
				if (curFixedArg <= 8)
				{
					if (curArgument->isConstant())
						vr = &inEmitter.genLoadConstant_I(*inDataNode, curArgument->getConstant().i);
					else
					{
						Copy_I& 	copyInsn =  *new(inPool) Copy_I(inDataNode, inPool);

						inEmitter.useProducer(curArgument->getVariable(), copyInsn, 0);
						vr = &inEmitter.defineTemporary(copyInsn, 0);
					}
					
					inEmitter.useTemporaryVR(*this, *vr, curTotalArgNumber);	
					vr->preColorRegister(curFixedArg);
				}
				else
				{
					StD_FixedDestRegister<kStackRegister>&	storeParam = *new(inPool) StD_FixedDestRegister<kStackRegister>(inDataNode, inPool, dfLwz, 24 + (curFixedArg + curFloatArg) * 4);
					
					if (curArgument->isConstant())
					{
						VirtualRegister&	vr = inEmitter.genLoadConstant_I(*inDataNode, curArgument->getConstant().i);
						inEmitter.useTemporaryVR(storeParam, vr, 1);	
					}
					else
						inEmitter.useProducer(curArgument->getVariable(), storeParam, 1);	
				
					// have the store use the incoming store edge, and define a temporary outgoing edge
					// make the call depend on this temporary outgoing store edge
					inEmitter.useProducer(inDataNode->nthInputVariable(0), storeParam, 0);	
					inEmitter.useTemporaryOrder(*this, inEmitter.defineTemporaryOrder(storeParam, 0), curTotalArgNumber);				
				}
				curFixedArg++;
				break;
			case vkFloat:
			case vkDouble:
				if (curFloatArg <= 13)
					vr->preColorRegister(curFloatArg++);
				else
					assert(false);
				break;
			case vkLong:
				assert(false);
				break;
			default:
				assert(false);
		}		
	}

	// keep track of maximum words used
	inEmitter.mMaxArgWords = PR_MAX(inEmitter.mMaxArgWords, curFixedArg + curFloatArg);
}


template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic>
void Call<tHasIncomingStore, tHasOutgoingStore,tHasFunctionAddress, tIsDynamic>::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& inFormatter)
{
	assert(!tIsDynamic);
	
	uint32	*curPC = curPC = (uint32 *) inStart;
	int32	branchOffset;
	bool	sameTOC = (mCalleeAddress->toc == inFormatter.mRealTOC->mGlobalPtr);
	bool	isStub = true;		// callee is stub
	
    if (!isStub && sameTOC && (uint32) mCalleeAddress->functionPtr < 0x3FFFFFF)
    {
		*curPC++ = kBla | ( (uint32) mCalleeAddress->functionPtr & 0x03FFFFFF);	
		*curPC++ = kNop;
	}
	else if (!isStub && sameTOC && canBePCRelative(inStart, (void*) mCalleeAddress, branchOffset))
	{
		*curPC++ = kBl | (branchOffset & 0x03FFFFFF);	
		*curPC++ = kNop;
	}
	else 
	{
		// we can or the offset right into the bl instruction (because aa and lk nicely use the last 2 bits)		
		*curPC++ = kBl | (((uint32) inFormatter.mNextFreePostMethod - (uint32)inStart) & 0x03FFFFFF);	
	
		inFormatter.mNextFreePostMethod  = (uint32*) formatCrossTocPtrGl(inFormatter.mNextFreePostMethod, mTOCOffset + inFormatter.mRealTOCOffset);
		*curPC++ = makeDForm(32, 2, 1, 20);				// stw		rtoc, 20(sp)
	}
}

inline void CallD_::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/)
{
	uint32	*curPC = (uint32 *) inStart;

	*curPC++ = kBl | (((uint32) getDynamicPtrGl(inStart, udToRegisterNumber(getInstructionUseBegin()[1])) - (uint32) inStart) & 0x03FFFFFF);	
	*curPC++ = makeDForm(32, 2, 1, 20);					// stw		rtoc, 20(sp)
}

#endif // _H_PPCCALLS
