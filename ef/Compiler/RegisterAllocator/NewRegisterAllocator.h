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

#ifndef _REGISTER_ALLOCATOR_H_
#define _REGISTER_ALLOCATOR_H_


#include "Fundamentals.h"

#define INCLUDE_EMITTER
#include "CpuInfo.h"

#include "FastBitMatrix.h"
#include "ControlGraph.h"

class Pool;
class VirtualRegisterManager;

// ----------------------------------------------------------------------------
// RegisterAllocator

class RegisterAllocator
{
private:
	Pool& 					pool;							// RegisterAllocator's pool.
	ControlNode** 			dfsList;						// List of nodes in a Depth First Search order.
	ControlNode** 			lndList;						// List of nodes in a Loop Nesting Depth order.
	Uint32					nNodes;							// Number of nodes in this graph.
	bool					isAcyclic;						// True if the graph is acyclic.
	VirtualRegisterManager& vrManager;						// VirtualRegister's manager.
	MdEmitter& 				emitter;						// The machine dependant instruction emitter.

	FastBitMatrix			interferenceMatrix;				// The interference matrix. Each row is assign to a VirtualRegister.
#ifdef DEBUG
	Uint32					interferenceMatrixSize;			// The number of rows and cols in the interference matrix.
#endif // DEBUG
	Uint32*					interferenceDegrees;			// The interference degree of all the VirtualRegisters in the graph.
	Uint32					interferenceDegreesSize;		// The size of interferenceDegrees.

	FastBitSet				tempBitSet;						// Temporary bitset (one time allocation).


	// Annotate the phi nodes.
	void annotatePhiNodes();
	// Get the liveness information and build the interference graph.
	void buildInterferenceGraph();
	// Coalesce the VirtualRegisters source to dest if it is possible.
	bool coalesceRegistersIfPossible(VirtualRegister& sourceRegister, VirtualRegister& destRegister);
	// Remove uneeded compy instructions (when the source and destination don not interfere);
	void removeUneededCopyInstructions();
	// Replace the remaining phi nodes by their equivalent instruction (Copy, Load, Store).
	bool replacePhiNodesByCopyInstructions();
	// Get the spilling cost for each VirtualRegister.
	void calculateSpillCosts();

#ifdef DEBUG
	// Check if the coalescing didn't corrupt the interferenceMatrix.
	void checkCoalescingValidity();
#endif // DEBUG

public:
	inline RegisterAllocator(Pool& pool, ControlGraph& graph, VirtualRegisterManager& vrManager, MdEmitter& emitter);

	void allocateRegisters();
};

// ----------------------------------------------------------------------------
// Inlines

inline RegisterAllocator::RegisterAllocator(Pool& pool, ControlGraph& graph, VirtualRegisterManager& vrManager, MdEmitter& emitter) :
	pool(pool), vrManager(vrManager), emitter(emitter), interferenceDegreesSize(0)
{
	dfsList = graph.dfsList;
	lndList = graph.lndList;
	nNodes = graph.nNodes;
	isAcyclic = !graph.hasBackEdges;
}

#endif // _REGISTER_ALLOCATOR_H_
