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
//  x86StdCall.h - StdCall calling convention
//

#include "x86Win32Instruction.h"

// Utility function to push one function-call argument on the stack
void pushCallArg(DataConsumer &curArgument, DataNode* usesNode, InstructionDefine*& orderingDependency, Pool& inPool, x86Win32Emitter& inEmitter);

// -> mem regarg1 regarg2 regarg3 regarg3 
// <- mem [returnval]


// inHasReturnValue is deprecated, and is ignored
template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic> 
Call<tHasIncomingStore, tHasOutgoingStore, tHasFunctionAddress, tIsDynamic>::
Call(	DataNode* inDataNode,
		Pool& inPool, 
		Uint8 inRegisterArguments, 
		bool /*inHasReturnValue*/, 
		x86Win32Emitter& inEmitter, 
		void (*inFunc)(),
		DataNode* inUseDataNode) :
	InsnUseXDefineYFromPool(inDataNode, inPool, ((inRegisterArguments != 0) ? 1 : 0) + tHasIncomingStore + tIsDynamic, 3)
												// add extra use for ordering edge (see below) if more than one argument
{
	DataNode*									returnValProducer; // node whose outgoing edge is the return value

	if (inDataNode->hasKind(vkTuple))
	{
		// if called with a Primitive with kind vkTuple, we know we will have to deal with a projection edge
		const DoublyLinkedList<DataConsumer>& 		projectionConsumers = inDataNode->getConsumers();
 		DoublyLinkedList<DataConsumer>::iterator 	curProjectionEdge = projectionConsumers.begin();
		
		// grab the outgoing store (if there is one)
		// set up returnValProducer to point to the return value producer (if it exists)
		// one of the two consumers of the projection node must be the memory node, the other must be the Return value node
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
	}
	else
	{
		// non-call Primitive treated as an out-of-line call
		assert(!tHasFunctionAddress && !tIsDynamic);

		// since outgoing stores are handled separately and are not
		// a real return type, we make the return value code ignore
		// them
		if (tHasOutgoingStore)
		{
			assert(inDataNode->hasKind(vkMemory));
			inEmitter.defineProducer(*inDataNode, *this, 0);	//-> mem
			returnValProducer = NULL;
		}
		else
			returnValProducer = inDataNode;
	}

	// pre-color (and buffer) the return value
	if (returnValProducer)
	{
		switch (returnValProducer->getKind())
		{
			case vkInt:
			case vkAddr:
			{
				VirtualRegister*	returnValVR;
	
				// the Call defines a temporary register, which is precolored to 
				// the appropriate return register
				returnValVR = &inEmitter.defineTemporary(*this, 1);
				returnValVR->preColorRegister(x86GPRToColor[EAX]);
				
				// now create a "buffer" copy between the precolored return register
				// and make the Copy define the return value edge from the poCall
				InsnDoubleOpDir& copyInsn = newCopyInstruction(*inDataNode, inPool);				
				inEmitter.useTemporaryVR(copyInsn, *returnValVR, 0);
				inEmitter.defineProducer(*returnValProducer, copyInsn, 0);
				// fiX-ME none-out unused return value argument
				break;
			}

			case vkLong:
				{
					// results returned in eax (low) and edx (high)
					VirtualRegister*	lowVR;
					VirtualRegister*	highVR;
	
					lowVR = &inEmitter.defineTemporary(*this, 1);
					highVR = &inEmitter.defineTemporary(*this, 2);
					lowVR->preColorRegister(x86GPRToColor[EAX]);
					highVR->preColorRegister(x86GPRToColor[EDX]);
					
					InsnDoubleOpDir& copyHiInsn = newCopyInstruction(*inDataNode, inPool);
					inEmitter.useTemporaryVR(copyHiInsn, *highVR, 0);
					inEmitter.defineProducer(*returnValProducer, copyHiInsn, 0, vrcInteger, vidHigh);

					InsnDoubleOpDir& copyLowInsn = newCopyInstruction(*inDataNode, inPool);
					inEmitter.useTemporaryVR(copyLowInsn, *lowVR, 0);
					inEmitter.defineProducer(*returnValProducer, copyLowInsn, 0, vrcInteger, vidLow);
				}
				break;
                
			case vkFloat:
                inEmitter.emit_CallReturnF(*this, *inDataNode, *returnValProducer);
                break;

			case vkDouble:
                inEmitter.emit_CallReturnD(*this, *inDataNode, *returnValProducer);
                break;

			default:
				assert(false);
		break;
		}
	}
	else if (!tHasOutgoingStore)
		inDataNode->setInstructionRoot(this);	// if no outgoing edges (no return value or memory edge, we need to hook this on)
	
	DataConsumer*	firstArgument;
	DataConsumer*	curArgument;

	// uses may begin at some passed in node, instead of the node from which the call originates
	DataNode* usesNode = (inUseDataNode) ? inUseDataNode : inDataNode;
	
	// incoming store
	if (tHasIncomingStore)
		inEmitter.useProducer(usesNode->nthInputVariable(0), *this, tIsDynamic + (inRegisterArguments > 0));	// -> mem

	firstArgument = usesNode->getInputsBegin() + tHasFunctionAddress + tHasIncomingStore;

	// grab the callee address (it might not be a static call)
	if (tHasFunctionAddress)
	{
		assert(inFunc == NULL);	// programmer error to specify a function address if this Call has a function address
		if (tIsDynamic)
			inEmitter.useProducer(usesNode->nthInputVariable(1), *this, 0);			// -> incoming address
		else
			mCalleeAddress = (Uint32) nthInputConstantUint32(*usesNode, 1);
	}
	else
		mCalleeAddress = (Uint32) inFunc;
		
	// start at last argument and push arguments from right to left

	curArgument	 = usesNode->getInputsEnd() - 1;	// arguments array is last + 1

	// BASIC IDEA:
	//
	// We are creating a bunch of pushes followed by a call instruction
	// Since pushes are clearly ordered operations we need an edge to maintain that ordering.
	// We use an order edge defined by the first push, and which is used and by each
	// subsequent push.  This ends in a use by the Call (*this).
	//
	// In the end we will have somethinglike Call -> Arg0 -> Arg1 -> ... -> ArgN-1 -> ArgN
	// where Call is the call instruction and the Arg's are pushes.
	//
	// The scheduler will walk up the use chain and do the last arg first and then come
	// back down the chain.
	InstructionDefine*	orderingDependency = NULL;			// maintains "thread" to order the pushes of the arguments

	if(curArgument >= firstArgument)
	{
		// Generate pushes for the arguments.  Order them with Order Edges.
		// Note x86 arguments are pushed on the stack from right to left.
		for (; curArgument >= firstArgument; curArgument--)
		    pushCallArg(*curArgument, usesNode, orderingDependency, inPool, inEmitter);

		// this is the final order edge which is attached to the Call instruction
		inEmitter.useTemporaryOrder(*this, *orderingDependency, tIsDynamic);	
	}
}
