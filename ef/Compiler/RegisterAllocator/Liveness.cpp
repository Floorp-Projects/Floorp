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
#include "Liveness.h"

#include "FastBitSet.h"
#include "VirtualRegister.h"
#include "ControlGraph.h"
#include "Timer.h"

//
// Build the liveness analysis.
//
// The liveness is defined by the following data-flow equations:
//		LiveIn(n) = LocalLive(n) U (LiveOut(n) - Killed(n)).
//		LiveOut(n) = U LiveIn(s) (s a successor of n).
//	where LocalLive(n) is the set of used registers in the block n, Killed(n) is the set of defined registers 
//	in the block n, LiveIn(n) is the set of live registers at the begining a the block n and LiveOut(n) is the
//	set of live registers at the end of the block n.
//
// We will compute the liveness analysis in two stages:
//	1- Build LocalLive(n) (wich is an approximation of LiveIn(n)) and Killed(n) for each block n.
//	2- Perform a backward data-flow analysis to propagate the liveness information through the entire control-flow graph.

void Liveness::buildAcyclicLivenessAnalysis(const ControlGraph& /*controlGraph*/, const VirtualRegisterManager& /*vrManager*/)
{
	trespass("not implemented");
}

//
// Build the sets LiveIn & LiveOut for each node of the given cyclic graph (controlGraph).
//
void Liveness::buildCyclicLivenessAnalysis(const ControlGraph& controlGraph, const VirtualRegisterManager& vrManager)
{
	Uint32 nNodes = size = controlGraph.nNodes;	// Number of nodes in this graph.
	Uint32 nRegisters = vrManager.getSize();	// Number of allocated VirtualRegisters.

	startTimer("liveness");

	// Allocate and clear the sets LiveIn, LiveOut and Killed (Killed is a temporary, 
	// it is no longer needed after the liveness analysis).
	liveIn = new(pool) FastBitSet[nNodes](pool, nRegisters);
	liveOut = new(pool) FastBitSet[nNodes](pool, nRegisters);
	FastBitSet* killed = new(pool) FastBitSet[nNodes](pool, nRegisters);
	FastBitSet* usedByPhiNodes = new(pool) FastBitSet[nNodes](pool, nRegisters);

	ControlNode** nodes = controlGraph.dfsList;
	
	//
	// First stage of the liveness analysis: Compute the sets LocalLive(stored in LiveIn) and Killed.
	//
	for (Uint32 n = 0; n < (nNodes - 1); n++) {
		ControlNode& node = *nodes[n];
		FastBitSet& currentLocalLive = liveIn[n];
		FastBitSet& currentKilled = killed[n];
		
		// If a Virtual Register is defined by a phi node then we add it to the set Killed. Add also the
		// used VirtualRegister of this phi node in the set usedByPhiNodes.
		DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();
		for (DoublyLinkedList<PhiNode>::iterator p = phiNodes.begin(); !phiNodes.done(p); p = phiNodes.advance(p)) {
			PhiNode& phiNode = phiNodes.get(p);
			if (isStorableKind(phiNode.getKind())) {
				currentKilled.set(phiNode.getVirtualRegisterAnnotation()->index);

				// Walk the uses.
				DataConsumer* consumers = phiNode.getInputsBegin();
				Uint32 predecessorIndex = 0;

				const DoublyLinkedList<ControlEdge>& predecessors = node.getPredecessors();
				for (DoublyLinkedList<ControlEdge>::iterator e = predecessors.begin(); !predecessors.done(e); e = predecessors.advance(e)) {

					VirtualRegister& usedRegister = *consumers[predecessorIndex++].getVariable().getVirtualRegisterAnnotation();
					usedByPhiNodes[predecessors.get(e).getSource().dfsNum].set(usedRegister.index);
				}
			}
		}

		// Find the instructions contributions to the sets LocalLive and Killed.

		InstructionList& instructions = node.getInstructions();
		for (InstructionList::iterator i = instructions.begin(); !instructions.done(i); i = instructions.advance(i)) {
			Instruction& instruction = instructions.get(i);

			// If a VirtualRegister is 'used' before being 'defined' then we add it to set LocalLive.
			InstructionUse* useEnd = instruction.getInstructionUseEnd();
			for (InstructionUse* usePtr = instruction.getInstructionUseBegin(); usePtr < useEnd; usePtr++)
				if (usePtr->isVirtualRegister()) {
					VirtualRegisterIndex index = usePtr->getVirtualRegister().index;

					if (!currentKilled.test(index))
						currentLocalLive.set(index);
				}

			// If a Virtualregister is 'defined' then we add it to the set Killed.
			InstructionDefine* defineEnd = instruction.getInstructionDefineEnd();
			for (InstructionDefine* definePtr = instruction.getInstructionDefineBegin(); definePtr < defineEnd; definePtr++)
				if (definePtr->isVirtualRegister())
					currentKilled.set(definePtr->getVirtualRegister().index);
		}
	}

	//
	// Second stage of the liveness analysis: We propagate the LiveIn & LiveOut through the entire 
	// control-flow graph.
	//
	
	FastBitSet temp(pool, nRegisters);

	bool changed;
	do {
		changed = false;

		for (Int32 n = (nNodes - 2); n >= 0; n--) { // For all nodes in this graph (backward && end block not included).
			ControlNode& node = *nodes[n];
			FastBitSet& currentLiveIn = liveIn[n];
			FastBitSet& currentLiveOut = liveOut[n];

			Uint32 nSuccessors = node.nSuccessors();
			if (nSuccessors != 0) {
				temp = liveIn[node.nthSuccessor(0).getTarget().dfsNum];
				for (Uint32 s = 1; s < nSuccessors; s++)
					temp |= liveIn[node.nthSuccessor(s).getTarget().dfsNum];
			} else 
				temp.clear();

			ControlEdge* successorsEnd = node.getSuccessorsEnd();
			for (ControlEdge* e = node.getSuccessorsBegin(); e < successorsEnd; e++)
				temp |= liveIn[e->getTarget().dfsNum];

			temp |= usedByPhiNodes[n];

			if (currentLiveOut != temp) {
				currentLiveOut = temp;
				temp -= killed[n];
				temp |= currentLiveIn;
			
				if (currentLiveIn != temp) {
					currentLiveIn = temp;
					changed = true;
				}
			}
		}
	} while(changed);

	stopTimer("liveness");
	
	DEBUG_LOG_ONLY(printPretty(stdout));
}


#ifdef DEBUG_LOG
void Liveness::printPretty(FILE* f)
{
	for (Uint32 n = 0; n < size; n++) {
		fprintf(f, "Node %d: \n\tliveIn=", n);
		liveIn[n].printPrettyOnes(f);
		fprintf(f, "\tliveOut=");
		liveOut[n].printPrettyOnes(f);
	}
} 
#endif // DEBUG_LOG
