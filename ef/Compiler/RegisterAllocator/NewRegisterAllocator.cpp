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

#include "Fundamentals.h"

#define INCLUDE_EMITTER
#include "CpuInfo.h"

#include "RegisterAllocator.h"
#include "VirtualRegister.h"
#include "DoublyLinkedList.h"
#include "ControlNodes.h"
#include "Instruction.h"

#include "Liveness.h"

#if !defined(DEBUG_LOG) || !defined(DEBUG_laurentm)
#define NO_TIMER
#endif // !DEBUG_LOG  || !DEBUG_laurentm
#include "Timer.h"

//
// Annotate the phi nodes. We call an annotation a VirtualRegister assignment to a DataProducer.
// Phi nodes are special data nodes that will not generate an instruction. For this reason, 
// it is possible that after code generation, some DataProducer were not annotated. 
// The RegisterAllocator assumes that each DataProducer has been annotated. So we must annotate
// the DataProducer that were not.
//
void RegisterAllocator::annotatePhiNodes()
{
	for (Uint32 n = 0; n < nNodes; n++) { // For each nodes in this graph.
		DoublyLinkedList<PhiNode>& phiNodes = dfsList[n]->getPhiNodes();

		for (DoublyLinkedList<PhiNode>::iterator p = phiNodes.begin(); !phiNodes.done(p); p = phiNodes.advance(p)) { // For each phi nodes.
			PhiNode& phiNode = phiNodes.get(p);

			VirtualRegisterKind constraint;
			switch (phiNode.getKind()) {
				case vkCond:
				case vkMemory:
					continue;								// Condition and Memory phi nodes are not seen after code generation.
				
				case vkInt:
					constraint = IntegerRegister;
					break;
				case vkLong:
					constraint = IntegerRegister;			// FIXME: But should annotate 2 registers.
					break;
				case vkFloat:
					constraint = FloatingPointRegister;
					break;
				case vkDouble:
					constraint = FloatingPointRegister;		// FIXME: Is it always the case ?.
					break;
				case vkAddr:
					constraint = IntegerRegister;
					break;
				default:
					PR_ASSERT(!"Unhandled kind of phi node");
			}
			// Check the outgoing edge annotation. We do not check the consumers because 
			// they must be or have been defined by either a primitive or another phi node.
			if (phiNode.getVirtualRegisterAnnotation() == NULL)
				phiNode.annotate(vrManager.newVirtualRegister(constraint));
		}
	}
}

//
// Get the liveness information and build the interference graph. The interference graph 
// is represented by a bitmatrix. Each row of this bitmatrix represents a VirtualRegister, 
// and each bits on this row an interference with another VirtualRegister.
// 2 registers will interfere if they are live at the same time. A register is alive between
// its definition and its last use.
//
void RegisterAllocator::buildInterferenceGraph()
{
	// To get the number of VirtualRegisters in the manager, we must be sure
	// that no one will try to allocate new VirtualRegisters. 
	// We are locking the manager for the entire method.
	DEBUG_ONLY(TIMER_SAFE(vrManager.lockAllocation()));

	Uint32 nVirtualRegisters = vrManager.getSize(); // Maximum of allocated VirtualRegister.

	// ControlNodes hold some fields for the register allocation (liveAtBegin, liveAtEnd, etc ...).
	// We need to reset these fields and allocate the memory fro the bitsets.
	for (Uint32 n = 0; n < nNodes; n++) {
		ControlNode& node = *dfsList[n];

		node.liveAtBegin.sizeToAndClear(pool, nVirtualRegisters);
		node.liveAtEnd.sizeToAndClear(pool, nVirtualRegisters);
		node.hasNewLiveAtEnd = true;
		node.liveAtBeginIsValid = false;
	}

	// Reset the interference matrix. This matrix must hold nVirtualRegisters rows and
	// nVirtualRegisters column.
	interferenceMatrix.sizeToAndClear(pool, nVirtualRegisters, nVirtualRegisters);
	DEBUG_ONLY(interferenceMatrixSize = nVirtualRegisters);

	FastBitSet currentLive(pool, nVirtualRegisters);	// Bitset for the registers currently alive.

	while(true) {
		bool needsToLoop = false;

		for (Int32 n = (nNodes - 1); n >= 0; n--) { // For each nodes in this graph.
			ControlNode& node = *dfsList[n];

			// We skip the nodes that we already looked at and that do 
			// not have any new liveness information at the end.
			if (node.liveAtBeginIsValid && !node.hasNewLiveAtEnd)
				continue;

			// The register currently alive are the register alive at
			// the end of this node.
			currentLive = node.liveAtEnd; 
			// We are looking at this node so for now it doesn't have
			// any new liveness information at the end.
			node.hasNewLiveAtEnd = false;

			// We walk backward all the instructions defined in this node.
			// A register becomes alive with its last use (first found), interfere with
			// all the registers alive at its definition, and is killed by its definition.
			InstructionList& instructions = node.getInstructions();
			for (InstructionList::iterator i = instructions.end(); !instructions.done(i); i = instructions.retreat(i)) {
				Instruction&       instruction = instructions.get(i);
				InstructionUse*    useBegin    = instruction.getInstructionUseBegin();
				InstructionUse*    useEnd      = instruction.getInstructionUseEnd();
				InstructionUse*    usePtr;
				InstructionDefine* defBegin    = instruction.getInstructionDefineBegin();
				InstructionDefine* defEnd      = instruction.getInstructionDefineEnd();
				InstructionDefine* defPtr;

				InstructionFlags flags = instruction.getFlags();

				// Move is handled specially to avoid adding an interference between the
				// source and the destination. If an interference exists between these
				// registers another instruction will set it. For now the source register
				// and the destination register will contain the same value.
				if (flags & ifCopy) {
				  PR_ASSERT(useBegin[0].isVirtualRegister());
				  currentLive.clear(useBegin[0].getVirtualRegister().index);
				}

				// Registers are defined. This definition has an interference with all the
				// registers currently alive.
				for (defPtr = defBegin; defPtr < defEnd; defPtr++)
					if (defPtr->isVirtualRegister()) {
						VirtualRegisterIndex index = defPtr->getVirtualRegister().index;
						interferenceMatrix.orRows(index, currentLive, index);
					}

				// The definition just killed these registers.
				for (defPtr = defBegin; defPtr < defEnd; defPtr++)
					if (defPtr->isVirtualRegister())
						currentLive.clear(defPtr->getVirtualRegister().index);

				// If this instruction is a Call instruction, we want all the caller-save 
				// registers to interfere with the current live registers. We do not want an 
				// interference with the callee-save registers because the call will save &
				// restore these registers.
				if (flags & ifCall) {
					// FIXME
				}

				if (flags & ifSpecialCall) {
					// FIXME
				}

				// Each use of a register makes it alive.
				for (usePtr = useBegin; usePtr < useEnd; usePtr++)
					if (usePtr->isVirtualRegister())
						currentLive.set(usePtr->getVirtualRegister().index);
			}

			// If the liveness information is different than the previous one, we
			// need to propagate the changes to each predecessor of this node.
			// We also need to add the liveness information from the phi nodes.
			if (currentLive != node.liveAtBegin) {
				FastBitSet liveAtEnd(pool, nVirtualRegisters);	// Bitset for the registers alive at the end of a predecessor of this node.

				DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();
				const DoublyLinkedList<ControlEdge>& predecessors = node.getPredecessors();
				DoublyLinkedList<ControlEdge>::iterator i;
				Uint32 predecessorIndex;

				for (predecessorIndex = 0, i = predecessors.begin(); !predecessors.done(i); predecessorIndex++, i = predecessors.advance(i)) {
					ControlNode& predecessor = predecessors.get(i).getSource();
					bool predecessorHasNewLiveAtEnd = false;
				  
					if (!phiNodes.empty()) {
						liveAtEnd = currentLive;
					  
						for (DoublyLinkedList<PhiNode>::iterator p = phiNodes.end(); !phiNodes.done(p); p = phiNodes.retreat(p)) {
							PhiNode& phiNode = phiNodes.get(p);
						  
							if (isStorableKind(phiNode.getKind())) {
								DataNode& producer = phiNode.nthInput(predecessorIndex).getVariable();
								VirtualRegisterIndex use = producer.getVirtualRegisterAnnotation()->index;
								VirtualRegisterIndex define = phiNode.getVirtualRegisterAnnotation()->index;

								// If this phi node has been coalesced then the producer and the consumer for this
								// edge hold the same VirtualRegister. In this case we don't update the liveness
								// information.
								if (define != use) {
								// A phiNode, if not coalesced will generate a Copy instruction. So we do
								// not want the use and the definition to interfere.
									liveAtEnd.clear(use);
									interferenceMatrix.orRows(define, liveAtEnd, define);
									liveAtEnd.clear(define);
									liveAtEnd.set(use);
								}
							}
						}
						predecessorHasNewLiveAtEnd |= predecessor.liveAtEnd.set(liveAtEnd);
					} else { // phiNode.empty() == true 
						predecessorHasNewLiveAtEnd = predecessor.liveAtEnd.set(currentLive);
					}

					predecessor.hasNewLiveAtEnd |= predecessorHasNewLiveAtEnd;
					needsToLoop |= predecessorHasNewLiveAtEnd;
				}
			}
			node.liveAtBegin = currentLive;
			node.liveAtBeginIsValid = true;
		}

		// If this graph is an acyclic graph we are sure that after the first pass all 
		// the interferences are found. We stop this loop in this case and also
		// if no new live registers are found.
		if (!needsToLoop || isAcyclic)
			break;
	}

	// Summarize the interference matrix. This matrix must be diagonale and some interferences
	// are not valid (i.e. IntegerRegister with FloatingPointRegister, 2 precolored registers
	// to different colors.

	FastBitMatrix trans(pool, nVirtualRegisters, nVirtualRegisters);
	for (VirtualRegisterIndex r = vrManager.first(); !vrManager.done(r); r = vrManager.advance(r)) {
		interferenceMatrix.clear(r, r); // A register do not interfere with itself;

		FastBitSet row(interferenceMatrix.getRowBits(r), nVirtualRegisters);
		for (Int32 i = row.firstOne(); !row.done(i); i = row.nextOne(i))
			trans.set(i, r);
	}

	for (VirtualRegisterIndex s = vrManager.first(); !vrManager.done(s); s = vrManager.advance(s)) {
		interferenceMatrix.orRows(s, FastBitSet(trans.getRowBits(s), nVirtualRegisters), s);
	}

	interferenceMatrix &= vrManager.getValidInterferencesMatrix();

	// Get the interference degrees from the interference matrix.
	if (interferenceDegreesSize < nVirtualRegisters) {
		interferenceDegreesSize = nVirtualRegisters;
		interferenceDegrees = new(pool) Uint32[interferenceDegreesSize];
	}

	for (Uint32 j = 0; j < nVirtualRegisters; j++)
		interferenceDegrees[j] = FastBitSet(interferenceMatrix.getRowBits(j), nVirtualRegisters).countOnes();

	// We are done with this method. We can now unlock the VirtualRegisterManager.
	DEBUG_ONLY(TIMER_SAFE(vrManager.unlockAllocation()));
}

//
// Return true if the VirtualRegisters source to dest were coalesced.
//
bool RegisterAllocator::coalesceRegistersIfPossible(VirtualRegister& sourceRegister, VirtualRegister& destRegister)
{
	VirtualRegisterIndex source = sourceRegister.index;
	VirtualRegisterIndex dest = destRegister.index;

	if (source == dest)
		return true;

#ifdef DEBUG
	if (sourceRegister.isPreColored())
		PR_ASSERT(destRegister.isPreColored());
#endif // DEBUG

	if (destRegister.isPreColored() && sourceRegister.isPreColored())
		if (sourceRegister.getPreColor() != destRegister.getPreColor())
			return false;
		else // 2 precolored registers cannot interfere. 
			PR_ASSERT(!interferenceMatrix.test(source, dest));
	else if (interferenceMatrix.test(source, dest))
		return false;

	Uint32 nVirtualRegisters = vrManager.getSize();
	FastBitSet sourceInterferences(interferenceMatrix.getRowBits(source), nVirtualRegisters);

	tempBitSet.sizeTo(pool, nVirtualRegisters);
	tempBitSet = sourceInterferences;
	tempBitSet -= FastBitSet(interferenceMatrix.getRowBits(dest), nVirtualRegisters);

	Uint32 nNewInterferences = tempBitSet.countOnes();
	if ((nNewInterferences + interferenceDegrees[dest]) >= virtualRegisterClasses[destRegister.kind]->getNumberOfColor())
		return false;

	// Coalesce the registers.
	vrManager.coalesceVirtualRegisters(source, dest);
	
	// Update the interference matrix.
	for (Int32 i = sourceInterferences.firstOne(); !sourceInterferences.done(i); i = sourceInterferences.nextOne(i)) {
		interferenceMatrix.clear(i, source);
		if (tempBitSet.test(i)) // This is a new interference for dest.
			interferenceMatrix.set(i, dest);
		else
			interferenceDegrees[i]--;
	}

	interferenceDegrees[dest] += nNewInterferences;
	interferenceMatrix.orRows(source, dest, dest);

#ifdef DEBUG
START_TIMER_SAFE
	interferenceMatrix.clear(source);
	interferenceDegrees[source] = 0;

	// Check the validity of the dest row.
	PR_ASSERT(FastBitSet(interferenceMatrix.getRowBits(dest), nVirtualRegisters).countOnes() == interferenceDegrees[dest]);
END_TIMER_SAFE
#endif

	return true;
}

//
// Check all Copy instructions & phiNodes for possible register coalescing. 
// If two registers are coalesced there's no need to keep the instruction (or the phiNode). 
// The ControlFlow graph is analysed from the most executed node to the least.
//
// TODO: loop nesting depth is not good enough (should analyse the program regions instead).
//
void RegisterAllocator::removeUneededCopyInstructions()
{
	for (Int32 n = nNodes - 1; n >= 0; n--) {
		ControlNode& node = *lndList[n];

		// 
		// Check the Copy in the instruction list.
		//
		InstructionList& instructions = node.getInstructions();
		InstructionList::iterator i = instructions.begin();

		while (!instructions.done(i)) { // For each instruction in this node.
			Instruction& instruction = instructions.get(i);
			i = instructions.advance(i);

			if (instruction.getFlags() & ifCopy) { // This is a Copy Instruction
				PR_ASSERT(instruction.getInstructionUseBegin()[0].isVirtualRegister());
				PR_ASSERT(instruction.getInstructionDefineBegin()[0].isVirtualRegister());

				VirtualRegister& sourceRegister = instruction.getInstructionUseBegin()[0].getVirtualRegister();
				VirtualRegister& destRegister = instruction.getInstructionDefineBegin()[0].getVirtualRegister();
				
				// If a VirtualRegister is precolored then this register must be the destination
				// register if the coalescing is possible.
				if (destRegister.isPreColored()) {
					if (coalesceRegistersIfPossible(sourceRegister, destRegister))
						instruction.remove();
				} else {
					if  (coalesceRegistersIfPossible(destRegister, sourceRegister))
						instruction.remove();
				}
			}
		}

		//
		// Check the phi nodes.
		//
		DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();
		DoublyLinkedList<PhiNode>::iterator p = phiNodes.end();

		while (!phiNodes.done(p)) { // For each phi nodes.
			PhiNode& phiNode = phiNodes.get(p);
			p = phiNodes.retreat(p);

			if (isStorableKind(phiNode.getKind())) {
				bool canRemoveThisPhiNode = true; // Can we remove the phi node from the graph ?

				DataConsumer* limit = phiNode.getInputsEnd();
				for (DataConsumer* consumer = phiNode.getInputsBegin(); consumer < limit; consumer++) {
					VirtualRegister& sourceRegister = *phiNode.getVirtualRegisterAnnotation();
					VirtualRegister& destRegister = *consumer->getVariable().getVirtualRegisterAnnotation();


					// If a VirtualRegister is precolored then this register must be the destination
					// register if the coalescing is possible.
					if (destRegister.isPreColored()) {
						if (!coalesceRegistersIfPossible(sourceRegister, destRegister))
							canRemoveThisPhiNode = false;
					} else {
						if  (!coalesceRegistersIfPossible(destRegister, sourceRegister))
							canRemoveThisPhiNode = false;
					}
				}

				if (canRemoveThisPhiNode)
					phiNode.remove();
			}
		}
	}
}

//
// Replace the remaining phi nodes by their equivalent instruction (Copy, Load, Store).
// Return true if the register allocator needs one more pass.
//
bool RegisterAllocator::replacePhiNodesByCopyInstructions()
{
	bool addedANode = false;
	bool needsRebuild = false;

	for (Uint32 n = 0; n < nNodes; n++) { // For all nodes.
		ControlNode& node = *dfsList[n];

		DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();
		const DoublyLinkedList<ControlEdge>& predecessors = node.getPredecessors();
		bool hasOnlyOneInput = predecessors.lengthIs(1);

		// If this node has only one predecessor, it is possible to insert the copy instruction
		// at the begining of this node. To respect the interference order, we have to append
		// each phiNode in the same order they are in the linked list.
		DoublyLinkedList<PhiNode>::iterator i = hasOnlyOneInput ? phiNodes.begin() : phiNodes.end();
		while (!phiNodes.done(i)) {
			PhiNode& phiNode = phiNodes.get(i);
			i = hasOnlyOneInput ? phiNodes.advance(i) : phiNodes.retreat(i);

			if (isStorableKind(phiNode.getKind())) {
				VirtualRegister& definedVR = *phiNode.getVirtualRegisterAnnotation();

				DoublyLinkedList<ControlEdge>::iterator e = predecessors.begin();
				DataConsumer* limit = phiNode.getInputsEnd();
				for (DataConsumer* consumer = phiNode.getInputsBegin(); consumer < limit; consumer++) { // For each consumer
					ControlEdge& edge = predecessors.get(e);
					e = predecessors.advance(e);

					VirtualRegister& usedVR = *consumer->getVariable().getVirtualRegisterAnnotation();

					if (definedVR.index != usedVR.index) { // We have to emit an instruction.
						InstructionList::iterator place;

						if (hasOnlyOneInput)
							place = node.getInstructions().begin()->prev;
						else {
							ControlNode& source = edge.getSource();
							if (source.nSuccessors() != 1) { 	// This predecessor has more than one outgoing edge. We need to add
																// a new ControlBlock to insert the copy instruction.
								ControlNode& cn = edge.getSource().controlGraph.newControlNode();
								cn.setControlBlock();
								ControlEdge& newEdge = cn.getNormalSuccessor();

								newEdge.substituteTarget(edge);
								cn.addPredecessor(edge);

								// append at the begin of this new node.
								place = cn.getInstructions().begin(); 
								addedANode = true;
							} else
								place = edge.getSource().getInstructions().end();
						}
						
						needsRebuild |= emitter.emitCopyAfter(phiNode, place, usedVR, definedVR);
					}
				}
				phiNode.remove();
			}
		}
	}

	if (addedANode) {
		ControlGraph& cg = dfsList[0]->controlGraph;
		cg.dfsSearch();
	  
		if (needsRebuild) {
			nNodes = cg.nNodes;
			dfsList = cg.dfsList;
			cg.lndSearch();
			lndList = cg.lndList;
		}
	}

	return needsRebuild;
}

//
// Calculate the spill cost for each VirtualRegister.
//
void RegisterAllocator::calculateSpillCosts()
{
	Uint32 nVirtualRegisters = vrManager.getSize();
	FastBitSet currentLive(pool, nVirtualRegisters);

	// Reset the spillCosts.
	for (VirtualRegisterIndex r = vrManager.first(); !vrManager.done(r); r = vrManager.advance(r)) {
		VirtualRegister& vReg = vrManager.getVirtualRegister(r);
		vReg.spillCosts.copyCosts = 0.;
		vReg.spillCosts.loadOrStoreCosts = 0.;
	}

	for (Int32 n = (nNodes - 1); n >= 0; n--) { // For each node in this graph.
		ControlNode& node = *dfsList[n];

		currentLive = node.liveAtEnd;

		InstructionList& instructions = node.getInstructions();
		for (InstructionList::iterator i = instructions.end(); !instructions.done(i); i = instructions.retreat(i)) {
			Instruction&       instruction = instructions.get(i);
			InstructionUse*    useBegin    = instruction.getInstructionUseBegin();
			InstructionUse*    useEnd      = instruction.getInstructionUseEnd();
			InstructionUse*    usePtr;
			InstructionDefine* defBegin    = instruction.getInstructionDefineBegin();
			InstructionDefine* defEnd      = instruction.getInstructionDefineEnd();
			InstructionDefine* defPtr;

			if (instruction.getFlags() & ifCopy) {				
				PR_ASSERT(useBegin[0].isVirtualRegister() && defBegin[0].isVirtualRegister());
				// If one of these registers are spilled, this instruction will be replaced by a load or a store.
				useBegin[0].getVirtualRegister().spillCosts.copyCosts -= node.nVisited;
				defBegin[0].getVirtualRegister().spillCosts.copyCosts -= node.nVisited;
			}

			// Handle the definition a new VirtualRegister.
			for (defPtr = defBegin; defPtr < defEnd; defPtr++)
				if (defPtr->isVirtualRegister()) {
					VirtualRegister& vReg = defPtr->getVirtualRegister();
					if (virtualRegisterClasses[vReg.kind]->canSpill()) {
					}
				}
					
		}
	}
}

void RegisterAllocator::allocateRegisters()
{
	//
	// Timer names.
	//
	const char* ANNOTATE_PHINODES = 		"RegAlloc.annotatePhiNodes";
	const char* BUILD_INTERFERENCEGRAPH = 	"RegAlloc.buildInterferenceGraph";
	const char* ALLOCATE_REGISTERS = 		"RegAlloc.allocateRegisters";
	const char* COALESCE_REGISTERS = 		"RegAlloc.coalesceRegisters";
	const char* REPLACE_PHI_NODES =			"RegAlloc.replacePhiNodesByCopyInstructions";

	startTimer(ALLOCATE_REGISTERS);

	// Add the missing annotations to the phi nodes.
	startTimer(ANNOTATE_PHINODES);
	annotatePhiNodes();
	stopTimer(ANNOTATE_PHINODES);

	// Build the interference matrix.
	startTimer(BUILD_INTERFERENCEGRAPH);
	buildInterferenceGraph();
	stopTimer(BUILD_INTERFERENCEGRAPH);

#if 0
	// Remove uneeded copy & phi nodes.
	startTimer(COALESCE_REGISTERS);
	removeUneededCopyInstructions();
	stopTimer(COALESCE_REGISTERS);

	// Just to be sure that the coalescing didn't corrupt the interference matrix.
	DEBUG_ONLY(TIMER_SAFE(checkCoalescingValidity()));

	// Get the cost for register spilling.
	calculateSpillCosts();
#endif

#if 0
	bool successfulColoring = colorRegisters();

	if (!successfulColoring)
		insertSpillCode();

	// Emit the instructions for the remaining phi nodes in the graph.
	startTimer(REPLACE_PHI_NODES);
	replacePhiNodesByCopyInstructions();
	stopTimer(REPLACE_PHI_NODES);
#endif

	stopTimer(ALLOCATE_REGISTERS);


	interferenceMatrix.printPretty(stdout);

#if 1
	for (VirtualRegisterIndex i = vrManager.first(); !vrManager.done(i); i = vrManager.advance(i)) {
		fprintf(stdout, "degree of vr%d = %d\n", i, interferenceDegrees[i]);
		vrManager.getVirtualRegister(i).setColor(0);
	}

	for (Uint32 m = 0; m < nNodes; m++) {
		ControlNode& node = *dfsList[m];
		node.printRef(stdout);
		fprintf(stdout, "\n");

		InstructionList& instructions = node.getInstructions();
		for(InstructionList::iterator i = instructions.begin(); !instructions.done(i); i = instructions.advance(i)) {
			instructions.get(i).printDebug(stdout);
			fprintf(stdout, "\n");
		}
	}
#endif
	
	ControlGraph& cg = dfsList[0]->controlGraph;
	Liveness liveness(pool);
	liveness.buildLivenessAnalysis(cg, vrManager);

	exit(-1);
}

#ifdef DEBUG
//
//
//
void RegisterAllocator::checkCoalescingValidity()
{
	FastBitMatrix backup(pool, interferenceMatrixSize, interferenceMatrixSize);
	backup = interferenceMatrix;

	buildInterferenceGraph();
	if (backup != interferenceMatrix) {
#ifdef DEBUG_LOG
		fprintf(stdout, "\n"
						"!!! RegisterAllocator Warning: Interference matrix is invalid after coalescing.\n"
						"!!! The RegisterAllocator will reconstruct a valid interferenceMatrix.\n"
						"!!!\n"
						"!!! Please mail your java source file to laurentm@netscape.com with\n"
						"!!! the subject \"RegAlloc Bug\". Thanks, Laurent.\n"
						"\n");
#ifdef DEBUG_laurentm
		interferenceMatrix.printDiff(stdout, backup);
#endif // DEBUG_laurentm
#else // DEBUG_LOG
		PR_ASSERT(!"Invalid interferenceMatrix after coalescing");
#endif // DEBUG_LOG
	}
}
#endif // DEBUG
