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
// CodeGenerator.cpp
//
// Scott M. Silver
// Peter Desantis
//
// A code generator is a loose organization of code which
// finds roots in a given control node of BURG-labellable expression
// trees.  The trees are labelled, and an emit routine is called
// for each labelled tree.

#define INCLUDE_EMITTER
#include "CpuInfo.h"

#include "Vector.h"
#include "Primitives.h"
#include "CodeGenerator.h"
#include "Burg.h"
#include "Scheduler.h"
#include "ControlNodes.h"
#include "InstructionEmitter.h"

#ifdef USE_VISUALIZER
#include "IGVisualizer.h"
#endif


//	sFakeRegPrimitives
//
//	A table of primitives which are used as the children
//	of primitives which are defined to be leaves in a
//	control node.  They are indexed by DataKind, ie
//	vkInt, etc...
//Primitive	sFakeReg_V(coReg_V);	// no such register
Primitive	sFakeReg_I(coReg_I, 0);
Primitive	sFakeReg_L(coReg_L, 0);
Primitive	sFakeReg_F(coReg_F, 0);
Primitive	sFakeReg_D(coReg_D, 0);
Primitive	sFakeReg_P(coReg_A, 0);
Primitive	sFakeReg_C(coReg_C, 0);
Primitive	sFakeReg_M(coReg_M, 0);	// no such register
//Primitive	sFakeReg_T(coReg_T);	// no such register

Primitive* sFakeRegPrimitives[nValueKinds] =
{
	NULL,				// no such register
	&sFakeReg_I,
	&sFakeReg_L,
	&sFakeReg_F,
	&sFakeReg_D,
	&sFakeReg_P,
	&sFakeReg_C,
	&sFakeReg_M,			
	NULL				// no such register
};


// This needs to be here because of header include problems.
CodeGenerator::CodeGenerator(Pool& inPool, MdEmitter& inEmitter) :
	mPool(inPool),
	mEmitter(inEmitter)
#ifdef USE_VISUALIZER
	,mVisualizer(*(new IGVisualizer()))
#endif
{
}


// generate
//
// Emit argumetns for the begin node.
// Find all roots of trees in inControlNode
// Label each root
// Call the emitter to emit for each root
void CodeGenerator::
generate(ControlNode& inControlNode)
{
	Vector<RootPair>	roots;

	// First find all the roots in this control node
	findRoots(inControlNode, roots);

	if (inControlNode.hasControlKind(ckBegin))
		mEmitter.emitArguments(inControlNode.getBeginExtra());
	else
	{
		// for each root label, and emit code
		RootPair*	curRoot;

		for (curRoot = roots.begin(); curRoot < roots.end(); curRoot++)
		{
			label(*(curRoot->root));
			emit(curRoot->root, 1);
		}
	}
	
	// schedule and output instructions
	LinearInstructionScheduler scheduler;
	scheduler.schedule(roots, inControlNode);

#ifdef IGVISUALIZE
	mVisualizer.addRoots(roots);	
#endif 
}


#ifdef IGVISUALIZE
void CodeGenerator::
visualize()
{
	mVisualizer.visualize();
}
#endif 


//	label
//
//	Label the treee rooted at inPrimitive
void CodeGenerator::
label(Primitive& inPrimitive)
{

	burm_label(&inPrimitive);	
}


//	emit
//	
//	Actually traverse the primitives and emit instructions
//	once labelled.
void CodeGenerator::
emit(Primitive* p, int goalnt)
{
	int 		eruleno = burm_rule(p->getBurgState(), goalnt);
        short*		nts = burm_nts[eruleno];
	Primitive* 	kids[10];
	int i;
	
	if (eruleno == 0) 
	{
		trespass("BURG matching -- no cover");
	}

	burm_kids(p, eruleno, kids);
	
	for (i = 0; nts[i]; i++)
		emit(kids[i], nts[i]);

	mEmitter.emitPrimitive(*p, eruleno);
}



//	instructionUseToInstruction
//
//	Move to the definer of this use. 
//	
//	There are three cases.  
//	
//	1. If there is a defining Instruction
//	in the Use, then that is the defining Instruction.  
//	
//	2. If the Use is Store or Cond Use, then if there is a
//	defining Instruction then it will be attached to the
//	DP at ID 0.
//	
//	3. If the Use is a Register Use, then there must be some
//	VR associated with the Use.  The Instruction which defines
//	the VR is the defining Instruction for this Use.
//	
//	It is possible that the result of 2 or 3 could be NULL after 
//	emitting for a given ControlNode.  This means that the resource
//	(outgoing edge) has not been defined, and is in another ControlNode.
//	(or there was a programmer error, how do we detect which one)
Instruction* CodeGenerator::
instructionUseToInstruction(InstructionUse& inIUse)
{
	Instruction* nextInsn;
	
	if (inIUse.src != NULL)
		nextInsn = inIUse.src;
	else
	{
		switch (inIUse.kind)
		{
			case udStore: case udCond:
				if (inIUse.name.dp != NULL)
				{
					if (inIUse.name.dp->getInstructionAnnotation() != NULL)
						nextInsn = inIUse.name.dp->getInstructionAnnotation();
					else
						nextInsn = NULL;
				}
				else
					nextInsn = NULL;
				break;
			case udRegister:
				//assert(inIUse.name.vr); can't check this because it's a VR Pointer
				nextInsn = inIUse.name.vr.getVirtualRegister().getDefiningInstruction();
				break;
			case udNone:
				nextInsn = NULL;
				break;
			case udOrder:
				nextInsn = inIUse.name.instruction;
				break;
			case udUninitialized:
				assert(false);
			default:
				assert(false);
		}
	}
	
	return (nextInsn);
}



//	getExpressionLeftChild
//
//	The BURG implementation of LEFT_CHILD
Primitive* CodeGenerator::
getExpressionLeftChild(Primitive* inPrimitive)
{
	assert(inPrimitive);
	
	bool hasIncomingStore = inPrimitive->hasCategory(pcLd) || inPrimitive->hasCategory(pcSt) || inPrimitive->hasCategory(pcCall);
								
	DataConsumer* leftConsumer = &inPrimitive->nthInput(hasIncomingStore);
	
	return (consumerToPrimitive(inPrimitive, leftConsumer));
}


// getExpressionRightChild
//
// Grab the right child of inPrimitive, skip over store edges
// [The BURG implementation of RIGHT_CHILD]
Primitive* CodeGenerator::
getExpressionRightChild(Primitive* inPrimitive)
{
	assert(inPrimitive);
	
	DataConsumer* rightConsumer;
	
	bool hasIncomingStore = inPrimitive->hasCategory(pcLd) || inPrimitive->hasCategory(pcSt) || inPrimitive->hasCategory(pcCall);

	rightConsumer = &inPrimitive->nthInput(1 + hasIncomingStore);
	
	return (consumerToPrimitive(inPrimitive, rightConsumer));				
}


//	consumerToPrimitive
//	
//	Takes a consumer of a value and finds the primitive which produces
//	the value that is consumed.
//
//	an example:
//
//	isLeaf
//
//	child			parent
//	P2 pr 	<-> 	co P1
//	
//	co: inConsumer
//	P1: inPrimitive
//	
//	co is a leaf edge if
//	
//	1. co and pr (or P1 and P2) are in different control nodes -> return fake reg primitive
//	2. P2 is a root -> return fake reg primitive
//	
//	else
//	
//	return P2
Primitive* CodeGenerator::
consumerToPrimitive(Primitive* inPrimitive, DataConsumer* inConsumer)
{
	// if it's already a fake reg primitive it has no children
	if (inPrimitive->getOperation() >= coReg_V && inPrimitive->getOperation() <= coReg_I)
		return NULL;	
	else if (inConsumer->isConstant())
		return (sFakeRegPrimitives[inConsumer->getKind()]);
		
	DataNode&	consumerChildNode = inConsumer->getVariable();
	Primitive*	consumerChildPrimitive;
	
	// extract P2 as above 
	if (!consumerChildNode.hasCategory(pcPhi))
		consumerChildPrimitive = &Primitive::cast(consumerChildNode);
	else
	  return (sFakeRegPrimitives[inConsumer->getKind()]);	// phi node
	
	// now perform the check to see if this edge connects to a "leaf" edge
	if	(consumerChildPrimitive->getContainer() != inPrimitive->getContainer() ||
		isRoot(*consumerChildPrimitive))
	{
		return (sFakeRegPrimitives[inConsumer->getKind()]);
	}				
	else
		return (consumerChildPrimitive);
}


// search through all primitives in a control node
// return a vector of all the roots of expression trees
void CodeGenerator::
findRoots(ControlNode& inControlNode, Vector<RootPair>& outRoots)
{
	DoublyLinkedList<Primitive>& primitives = inControlNode.getPrimitives();
	
	for (DoublyLinkedList<Primitive>::iterator i = primitives.begin(); !primitives.done(i); i = primitives.advance(i))
	{
		Primitive&	prim = primitives.get(i);
		
		RootKind root = isRoot(prim);

		if ((root == rkRoot) || (root == rkPrimary))
		{
			RootPair newRoot;
			newRoot.root = &prim;
			//prim.setRoot(true);
			if(root == rkPrimary)
				newRoot.isPrimary = true;
			else
				newRoot.isPrimary = false;
			outRoots.append(newRoot);
		}

		// root member of prim defaults to false
	}
}


//  1. is a primitive which is a pcIfCond (if or switch) OR
// 	2. is a primitive which is a pcResult OR
//  3. is a primitive all of whose outputs are in a different control node from its own OR
//	4. is an interior primitive whose inputs are shared by two primitives in the same (cse)
//  5. is a primitive connected to another pcCall/pcSysCall primitive
// In cases 1 2 3 the root is primary
RootKind CodeGenerator::
isRoot(Primitive& inPrimitive)
{
	bool isAtleastRoot = false;
	// 1 or 2
	if (inPrimitive.hasCategory(pcIfCond) || inPrimitive.hasCategory(pcResult) || 
		inPrimitive.hasCategory(pcSwitch))
		goto isRoot_rkPrimary;

	// 3
	DataNode*	curEdge;

	isAtleastRoot = inPrimitive.hasCategory(pcSt) || inPrimitive.hasCategory(pcCall) || inPrimitive.hasCategory(pcSysCall);

	// all dp's primitive's containers hooked to curEdge must not be in same control node as inPrimitive's container
	for (curEdge = inPrimitive.getOutgoingEdgesBegin(); curEdge < inPrimitive.getOutgoingEdgesEnd(); curEdge++)
	{
		const DoublyLinkedList<DataConsumer>& consumers = curEdge->getConsumers();
		
		for (	DoublyLinkedList<DataConsumer>::iterator curConsumer = consumers.begin(); 
				!consumers.done(curConsumer); 
				curConsumer = consumers.advance(curConsumer))
		{
			ControlNode*	curConsumerContainer;	// container of parentNode
			DataNode*		node;					// parentNode (node which consume's inPrimitive's input)
			
			node = &consumers.get(curConsumer).getNode();

			// can ignore LdV because it has another incoming edge
			isAtleastRoot |= ((node->getOutgoingEdgesEnd() - node->getOutgoingEdgesBegin() > 1) ||
							 (node->hasCategory(pcCall) || node->hasCategory(pcSysCall)));

			if (node->hasCategory(pcCall))
			{
				DataNode* calleeAddress = &node->nthInput(1).getVariable();
				if (&inPrimitive == calleeAddress)
				{
					isAtleastRoot = false;
					goto isRoot_rkNotRoot;		
				}
			}

			curConsumerContainer =  node->getContainer();
			
			if (!node->hasCategory(pcPhi) && curConsumerContainer == inPrimitive.getContainer())
			{
				// If there are any consumers in the current container, then the primitive is a root 
				// iff it produces a cse----look for another consumer in the primitive's container
				
				// Continue looking through the curEdge
				curConsumer = consumers.advance(curConsumer);
				for ( ; 
					!consumers.done(curConsumer); 
					curConsumer = consumers.advance(curConsumer))
				{
					node = &consumers.get(curConsumer).getNode();
					
					if (node->hasCategory(pcCall) || node->hasCategory(pcSysCall))
						goto isRoot_rkRoot;
						
					curConsumerContainer =  node->getContainer();

					if (curConsumerContainer == inPrimitive.getContainer()) // cse found.
						goto isRoot_rkRoot;
				}
				curEdge++;
				// Now continue looking through the other edges
				for (;curEdge < inPrimitive.getOutgoingEdgesEnd(); curEdge++)
				{
					const DoublyLinkedList<DataConsumer>& consumers = curEdge->getConsumers();
		
					for (	DoublyLinkedList<DataConsumer>::iterator thisConsumer = consumers.begin(); 
							!consumers.done(thisConsumer); 
							thisConsumer = consumers.advance(thisConsumer))
					{
						node = &consumers.get(thisConsumer).getNode();
						if (node->hasCategory(pcCall) || node->hasCategory(pcSysCall))
							goto isRoot_rkRoot;		
								
						curConsumerContainer =  node->getContainer();
			
						if (curConsumerContainer == inPrimitive.getContainer())	// cse found
							goto isRoot_rkRoot;
					}
				}
				// We did not find a cse => not a root
				goto isRoot_rkNotRoot;
			} 
		}
	}

// if we passed through the above loop,  3. must have been met (fall through to rkPrimary)
isRoot_rkPrimary:
	return (rkPrimary);
isRoot_rkRoot:
	return (rkRoot);
isRoot_rkNotRoot:
	return (isAtleastRoot ? rkRoot : rkNotRoot);
}


