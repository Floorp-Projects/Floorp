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
// Backend.cpp
//
// Scott M. Silver
// 
// Translate a ControlGraph into native code


#if defined(DEBUG) && (defined(WIN32) || defined(USE_MESA)) && defined(IGVISUALIZE)
#define USE_VISUALIZER 
#endif

#ifdef USE_VISUALIZER
#define IGVISUALIZE_ONLY(x)	x
#include "IGVisualizer.h"
#else
#define IGVISUALIZE_ONLY(x)
#endif

#include "Backend.h"
#include "ControlGraph.h"
#include "JavaVM.h"

#define INCLUDE_EMITTER
#include "CpuInfo.h"
#include "RegisterAllocator.h"
#include "CodeGenerator.h"
#include "CGScheduler.h"
#include "NativeFormatter.h"

#include "FieldOrMethod.h"
#include "LogModule.h"

#if DEBUG_laurentm
#include "ControlNodeScheduler.h"
#endif

static void 
explodeImmediatePrimitives(ControlGraph &cg);

#ifdef DEBUG_LOG
static void 
printInstructions(LogModuleObject inLogObject, ControlNode& inControlNode);

static void 
printInstructions(LogModuleObject inLogObject, ControlNode& inControlNode)
{
	InstructionList& instructions = inControlNode.getInstructions();

	for(InstructionList::iterator i = instructions.begin(); !instructions.done(i); i = instructions.advance(i))
	{
		instructions.get(i).printDebug(inLogObject);
		UT_OBJECTLOG(inLogObject, PR_LOG_ALWAYS, ("\n"));
	}
}
#endif

UT_DEFINE_LOG_MODULE(Backend);

void* 
translateControlGraphToNative(ControlGraph& inControlGraph, Method& inMethod)
{
	VirtualRegisterManager 	vrMan(inControlGraph.pool);
	MdEmitter               emitter(inControlGraph.pool, vrMan);
	CodeGenerator 			codeGenerator(inControlGraph.pool, emitter);

	// break out constants
	explodeImmediatePrimitives(inControlGraph);
	
	// * generate code
	DoublyLinkedList<ControlNode>::iterator j;
	for (j = inControlGraph.controlNodes.begin(); !inControlGraph.controlNodes.done(j); j = inControlGraph.controlNodes.advance(j)) 
	{
		ControlNode &cn = inControlGraph.controlNodes.get(j);

		codeGenerator.generate(cn);

#ifdef PRINT_INSTRUCTIONS_BEFORE_REGALLOC
		cn.printRef(UT_LOG_MODULE(Backend));

		UT_SET_LOG_LEVEL(Backend, PR_LOG_DEBUG);
		printInstructions(UT_LOG_MODULE(Backend), cn);
#endif
	}

	// If the debugger is enabled, generate pc2bci table
	if (VM::debugger.getEnabled()) {
		// Use an arbitrary number - needs to be set appropriately later
		inMethod.getPC2Bci().setSize(1000);
	}

	// * register allocation
	if (!inControlGraph.dfsListIsValid()) 
		inControlGraph.dfsSearch();
	if (!inControlGraph.lndListIsValid()) 
		inControlGraph.lndSearch();

	RegisterAllocator registerAllocator(inControlGraph.pool, inControlGraph.dfsList, inControlGraph.lndList, inControlGraph.nNodes, vrMan, emitter);
	registerAllocator.allocateRegisters();

#ifdef PRINT_INSTRUCTIONS_AFTER_REGALLOC
	for (j = inControlGraph.controlNodes.begin(); !inControlGraph.controlNodes.done(j); j = inControlGraph.controlNodes.advance(j)) 
	{
		ControlNode &cn = inControlGraph.controlNodes.get(j);

		printInstructions(UT_LOG_MODULE(Backend), cn);
	}
#endif


#ifdef NEW_CG_SCHEDULER
	ControlNodeScheduler cns(inControlGraph.pool, inControlGraph.dfsList, inControlGraph.nNodes);
	ControlNode** scheduledNodes = cns.getScheduledNodes();
#else // NEW_CG_SCHEDULER
	ControlGraphScheduler cgs(inControlGraph, emitter);
	ControlNode** scheduledNodes = cgs.scheduleNodes();
#endif // NEW_CG_SCHEDULER

	// * output to memory
	NativeFormatter formatter(emitter, scheduledNodes, inControlGraph.nNodes);

	void* func = formatter.format(inMethod);

	IGVISUALIZE_ONLY(codeGenerator.visualize();)
	return (func);
}


//
// Transform all immediate operations into
// operations which no longer intern a constant
//
// For example:
//
//	vi2 = AddI_I 6, vi1  
//
// 	becomes
//
//	vi1 = Const_I 6
//	vi3 = Add_I vi1, vi2
//
static void 
explodeImmediatePrimitives(ControlGraph &cg)
{
	cg.dfsSearch();
	ControlNode **dfsList = cg.dfsList;
	ControlNode **pcn = dfsList + cg.nNodes;

	while (pcn != dfsList) 
	{
		ControlNode &cn = **--pcn;

		// Search the primitives backwards; explode primitives as necessary
		DoublyLinkedList<Primitive> &primitives = cn.getPrimitives();
		DoublyLinkedList<Primitive>::iterator primIter = primitives.end();
		while (!primitives.done(primIter)) 
		{
			Primitive &p = primitives.get(primIter);
			primIter = primitives.retreat(primIter);
			
			// loop through all consumers of p; if a consumer is constant create 
			// a new PrimConst node, and attach it to p
			DataConsumer*	input;
			for (input = p.getInputsBegin(); input < p.getInputsEnd(); input++)
			{
				if (input->isConstant() && isRegOrMemKind(input->getKind()))
				{
					PrimConst *prim = new(cn.getPrimitivePool())
						PrimConst(input->getKind(), input->getConstant(), 
								  p.getBytecodeIndex());

					input->setVariable(*prim);
					cn.appendPrimitive(*prim);
				}
			}
		}
	}
}
