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

#ifndef _CONTROL_NODE_SCHEDULER_H_
#define _CONTROL_NODE_SCHEDULER_H_

#include "Fundamentals.h"
#include "Pool.h"

class ControlNode;

struct ScheduledNode : public DoublyLinkedEntry<ScheduledNode>
{
	// A ScheduledNode must be a linked in the execution order to other ScheduledNode. 
	// As ControlNode is already a linked to other ControlNode in the graph we create 
	// an instance of this class for each ControlNode of the graph and copy the field dfsNum.
	Int32 dfsNum;
};

class Clique : public DoublyLinkedList<ScheduledNode>, public DoublyLinkedEntry<Clique>
{
	// A Clique is a linked list of ScheduledNode and also an entry in a linked list (list of Cliques in a loop).
};

class LoopHierarchyNode : public DoublyLinkedEntry<LoopHierarchyNode>
{
private:
	DoublyLinkedList<LoopHierarchyNode> successors;

public:
	Int32						header;					// Loop header's dfsNum.
	FastBitSet					nodes;					// Nodes contained in this loop.
	DoublyLinkedList<Clique>	cliques;				// Linked list of Clique in this loop.

	inline void addSuccessor(LoopHierarchyNode& successor) {successors.addLast(successor);}
	inline DoublyLinkedList<LoopHierarchyNode>& getSuccessors() {return successors;}
};

class LoopHierarchySet
{
private:
	LoopHierarchyNode* 	nodes;							// Array of loop nodes in the loop hierarchy tree.
	Int32*				indexes;						// Array of indexes of nodes in the dfs order.
	Uint32				nextFree;						// Next available slot in nodes.

	LoopHierarchySet (const LoopHierarchySet &);		// Copying forbidden
	void operator = (const LoopHierarchySet &);			// Copying forbidden

public:
	LoopHierarchySet(Pool& pool, LoopHierarchyNode* nodesArray, Uint32 nNodes);

	LoopHierarchyNode& operator[] (const Uint32 nodeIndex);
};

class ControlNodeScheduler
{
private:
	Pool& pool;

	ControlNode** dfsList;								// List of ControlNodes in depth first search order (from the ControlGraph).
	const Uint32  nNodes;								// Number of ControlNodes in this graph.

	FastBitMatrix dominatorsMatrix;						// Matrix of dominators for all the ControlNodes.
	FastBitSet loopHeaders;								// BitSet of the loop headers in the graph.
	FastBitSet nodesAlreadyScheduled;					// BitSet of the ControlNodes currently scheduled.

	ScheduledNode* scheduledNodes;						// Array of ScheduledNodes. Used to chain the nodes in the Cliques.

	LoopHierarchyNode& buildLoopHierarchyTree();
	Uint32 findLoopHeaders();
	void scheduleLoop(LoopHierarchyNode& loopToSchedule);
	void initializeLongEdgeFlag(ControlEdge& edge, LoopHierarchyNode& inLoop, FastBitSet& RR, FastBitSet& Rlocal);
	void findDominators();

public:
	ControlNodeScheduler(Pool& p, ControlNode** nodes, Uint32 n) : 
		pool(p), dfsList(nodes), nNodes(n), dominatorsMatrix(n + 1, n), loopHeaders(n) {}

	ControlNode** getScheduledNodes();					// Return the array of ControlNode in a scheduled order.
};

#endif /* _CONTROL_NODE_SCHEDULER_H_ */
