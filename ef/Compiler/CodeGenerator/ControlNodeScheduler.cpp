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
#include "Pool.h"
#include "ControlNodes.h"
#include "ControlNodeScheduler.h"
#include "FastBitSet.h"

// ----------------------------------------------------------------------------
// LoopHierarchySet

//
// Create a new set of LoopHierachyNodes. Allocate the LoopHierarchyNode array of 
// length nLoops and the indexes array of length nNodes in the given pool. 
//
LoopHierarchySet::LoopHierarchySet(Pool& pool, LoopHierarchyNode* nodesArray, Uint32 nNodes) : nodes(nodesArray)
{
	indexes = new(pool) Int32[nNodes];
	nextFree = 0;

	fill(indexes, &indexes[nNodes], -1);
}

//
// Return the set's element corresponding to the ControlNode dfsNum equals nodeIndex.
//
LoopHierarchyNode& LoopHierarchySet::operator [] (const Uint32 nodeIndex)
{
	Int32 index = indexes[nodeIndex];

  // If index is -1 then this node is not part of the set yet.
	if (index == -1) {
		index = nextFree++;
		indexes[nodeIndex] = index;
	}

	return nodes[index];
}

// ----------------------------------------------------------------------------
// ControlNodeScheduler

//
// Return true if node is ready. We will call a node N ready if each of its 
// incoming edges E: A->N are long.
//
inline bool nodeIsReady(ControlNode& node)
{
	const DoublyLinkedList<ControlEdge>& predecessors = node.getPredecessors();
	for (DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin(); !predecessors.done(i); i = predecessors.advance(i))
		if (!predecessors.get(i).longFlag)
			return false;
	return true;
}

//
// Return true if node is ready with respect to 'respect'. We will call a node N 
// ready with respect to node P if, for each of N's incoming edges E: A->N, either 
// E is long or A equals P.
//
inline bool nodeIsReadyWithRespectTo(ControlNode& node, ControlNode& respect)
{
	const DoublyLinkedList<ControlEdge>& predecessors = node.getPredecessors();
	for (DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin(); !predecessors.done(i); i = predecessors.advance(i)) {
		ControlEdge& edge = predecessors.get(i);
		if (!edge.longFlag && (&edge.getSource() != &respect))
			return false;
	}
	return true;
}

//
// Schedule the working nodes in loopToSchedule.
//
void ControlNodeScheduler::scheduleLoop(LoopHierarchyNode& loopToSchedule)
{
#if defined(DEBUG_SCHEDULER)
	fprintf(stdout, "ControlNodeScheduler: Will schedule loop headed by N%d\n", loopToSchedule.header);
#endif

	//
	// Determine the nodes in a region.
	//

	FastBitSet& REG = loopToSchedule.nodes;	// REG is the region of nodes contained in this loop.
	FastBitSet  Rlocal(nNodes);				// Rlocal is the set of nodes in REG that are not contained in any subloop of REG. 
	FastBitSet  Rsubheaders(nNodes);		// Rsubheaders is the set of headers of immediate subloops of REG. 
	FastBitSet  Rcombined(nNodes);			// Rcombined is the union of Rlocal and Rsubheaders. 
	FastBitSet  R(nNodes);					// R is the intersection of Rcombined and the set of all working nodes.
	FastBitSet  RR(nNodes);					// RR is the intersection of REG and the set of all working nodes.

	Rlocal = REG;
	DoublyLinkedList<LoopHierarchyNode>& subLoops = loopToSchedule.getSuccessors();
	for (DoublyLinkedList<LoopHierarchyNode>::iterator i = subLoops.begin(); !subLoops.done(i); i = subLoops.advance(i)) {
		LoopHierarchyNode& subLoop = subLoops.get(i);
		Rsubheaders.set(subLoop.header);
		Rlocal -= subLoop.nodes;
	}

	Rcombined = Rlocal;
	Rcombined |= Rsubheaders;

	R = Rcombined;
	RR = REG;

	FastBitSet nonWorkingNodes(nNodes);
	for (Int32 j = REG.firstOne(); j != -1; j = REG.nextOne(j))
		if (!dfsList[j]->workingNode)
			nonWorkingNodes.set(j);

	R -= nonWorkingNodes;
	RR -= nonWorkingNodes;

	//
	// Initialize long edge flag for each edge E: A->B for which at least one of A or B is in R
	//

	Uint32 generation = ControlNode::getNextGeneration();

	for (Int32 j = R.firstOne(); j != -1; j = R.nextOne(j)) {
		ControlNode& node = *dfsList[j];
	  
		ControlEdge* limit = node.getSuccessorsEnd();
		for (ControlEdge* edge = node.getSuccessorsBegin(); edge != limit; edge++)
			if (edge->getTarget().generation != generation)
				initializeLongEdgeFlag(*edge, loopToSchedule, RR, Rlocal);

		const DoublyLinkedList<ControlEdge>& predecessors = node.getPredecessors();
		for (DoublyLinkedList<ControlEdge>::iterator p = predecessors.begin(); !predecessors.done(p); p = predecessors.advance(p)) {
			ControlEdge& edge = predecessors.get(p);
			if (edge.getSource().generation != generation)
				initializeLongEdgeFlag(edge, loopToSchedule, RR, Rlocal);
		}

		node.generation = generation;
	}

	//
	// Linear scheduling.
	//

	FastBitSet W(nNodes); 								// W is the set of nodes in the region remaining to be scheduled. 
	W = R;												// Initially W contains all nodes in R except the loop header. 
	W.clear(loopToSchedule.header);						//

	Clique** cliqueStack = new(pool) Clique*[nNodes];	// Stack of Cliques waiting to be scheduled.
	Clique** cliqueStackPointer = cliqueStack;			// Stack pointer.

	Int32 P = loopToSchedule.header; 					// P is the last node scheduled. It is initially the loop header.
	Int32 N;											// Next node to schedule.

	Clique* currentClique = new(pool) Clique();
	currentClique->addLast(scheduledNodes[P]);			// The loop header is always the first node of the first clique.
	nodesAlreadyScheduled.set(P);

	loopToSchedule.cliques.addLast(*currentClique);

	while (!W.empty() || (cliqueStackPointer != cliqueStack)) {
		// Alternative 1.
		// If there exists a node N in W such that there exists at least one short edge from P to N 
		// and node N is ready with respect to P, then we pick any such node N to be the next node.
		ControlNode& nodeP = *dfsList[P];

		ControlEdge* limit = nodeP.getSuccessorsEnd();
		for (ControlEdge* edge = nodeP.getSuccessorsBegin(); edge != limit; edge++) {
			ControlNode& target = edge->getTarget();
			if (!edge->longFlag && nodeIsReadyWithRespectTo(target, nodeP) && W.test(target.dfsNum)) {
				N = target.dfsNum;
				goto foundNextNode;
			}
		}

		// Alternative 2. 
		// Otherwise, if the stack isn't empty, we pop a clique Ck from the stack, make it become the next clique of 
		// this region, make every edge from P to any node A remaining in W into a long edge, set P to be the 
		// last node of Ck, and continue with alternative 1 above.
		if (cliqueStackPointer != cliqueStack) {
			Clique* nextClique = *--cliqueStackPointer;
			loopToSchedule.cliques.addLast(*nextClique);

			for (ControlEdge* edge = nodeP.getSuccessorsBegin(); edge != limit; edge++)
				if (W.test(edge->getTarget().dfsNum))
					edge->longFlag = true;

			P = nextClique->last().dfsNum;
			currentClique = nextClique;
			continue;
		}

		// Alternative 3. 
		// Otherwise, there must exist a node N in W such that node N is ready with respect to P. 
		// We pick any such node N to be the next node. 
		for (N = W.firstOne(); N != -1; N = W.nextOne(N))
			if (nodeIsReadyWithRespectTo(*dfsList[N], *dfsList[P]))
				goto foundNextNode;

		PR_ASSERT(false); // Shouldn't happen.

	foundNextNode:


		// After we have picked node N, we remove N from the set W and make every edge 
		// from P to any node A remaining in W into a long edge.
		W.clear(N);

		for (ControlEdge* e = nodeP.getSuccessorsBegin(); e != limit; e++)
			if (W.test(e->getTarget().dfsNum))
				e->longFlag = true;
	  
		// If N is not the header of a subloop.
		if (!Rsubheaders.test(N)) {
			ControlNode& nodeN = *dfsList[N];
			bool nodePhasEdgeToNodeN = false;

			for (ControlEdge* edge = nodeP.getSuccessorsBegin(); edge != limit; edge++) {
				if (&edge->getTarget() == &nodeN) {
					// If there exists some edge from P to N then we append node N to the current 
					// clique in our region's schedule
					nodePhasEdgeToNodeN = true;
					currentClique->addLast(scheduledNodes[N]);
					nodesAlreadyScheduled.set(N);
					break;
				}
			}

			if (!nodePhasEdgeToNodeN) {
				// There is node edge from P to N. We start a new clique in this region 
				// and make N this clique's first node.
				Clique* newClique = new(pool) Clique();

				newClique->addLast(scheduledNodes[N]);
				loopToSchedule.cliques.addLast(*newClique);
				nodesAlreadyScheduled.set(N);
				currentClique = newClique;
			}

			P = N;
		} else { // N is the header of a subloop.
			LoopHierarchyNode* subLoop = NULL;

			// Find the clique of the subloop that contains node N.
			DoublyLinkedList<LoopHierarchyNode>& subLoops = loopToSchedule.getSuccessors();
			for (DoublyLinkedList<LoopHierarchyNode>::iterator i = subLoops.begin(); !subLoops.done(i); i = subLoops.advance(i)) {
				subLoop = &subLoops.get(i);
				if (subLoop->header == N)
					break;
			}
			PR_ASSERT(subLoop);

			Clique* subLoopClique = NULL;
			for (DoublyLinkedList<Clique>::iterator c = subLoop->cliques.begin(); !subLoop->cliques.done(c); c = subLoop->cliques.advance(c)) {
				bool foundClique = false;
				subLoopClique = &subLoop->cliques.get(c);

				for (DoublyLinkedList<ScheduledNode>::iterator n = subLoopClique->begin(); !subLoopClique->done(n); n = subLoopClique->advance(n))
					if (subLoopClique->get(n).dfsNum == N) {
						foundClique = true;
						break;
					}
			  
				if (foundClique)
					break;
			}
			PR_ASSERT(subLoopClique);

			ControlNode& nodeN = *dfsList[N];
			bool nodePhasEdgeToNodeN = false;

			// If N is the first node of the subloop's clique and there exists some edge from P to N then we append 
			// the entire clique to the current clique. Otherwise, we start a new clique and make the subloop's 
			// clique become this new clique's beginning.
			for (ControlEdge* edge = nodeP.getSuccessorsBegin(); edge != limit; edge++)
				if (&edge->getTarget() == &nodeN) {
					nodePhasEdgeToNodeN = true;
					break;
				}

			if (!((subLoopClique->first().dfsNum == N) && nodePhasEdgeToNodeN)) {
				currentClique = new(pool) Clique();
				loopToSchedule.cliques.addLast(*currentClique);
			}

			for (DoublyLinkedList<ScheduledNode>::iterator s = subLoopClique->begin(); !subLoopClique->done(s);) {
				ScheduledNode& node = subLoopClique->get(s);
				s = subLoopClique->advance(s);
				node.remove();
				currentClique->addLast(node);
			}

			subLoopClique->remove();

			// If the subloop contained more than one clique, we push all remaining cliques onto the stack.
			// We are carefull to keep the scheduling order.
			for (DoublyLinkedList<Clique>::iterator t = subLoop->cliques.end(); !subLoop->cliques.done(t);) {
				Clique& slc = subLoop->cliques.get(t);
				t = subLoop->cliques.retreat(t);
			  
				slc.remove();
				*cliqueStackPointer++ = &slc;
			}

			// The last node scheduled is the last node of the current clique.
			P = currentClique->last().dfsNum;
		}
	}

	//
	// Loop scheduling
	//

	// Find the last clique in this region such that the last node N of this clique satisfies the following properties:
	//  1.There exists an edge from N to H. 
	//  2.There are no short edges E: N->A for which A is outside R. 
	//  3.N is not a switch node. 
	//  4.N is not H. 
	for (DoublyLinkedList<Clique>::iterator c = loopToSchedule.cliques.end(); !loopToSchedule.cliques.done(c); c = loopToSchedule.cliques.retreat(c)) {
		Clique& clique = loopToSchedule.cliques.get(c);
		ControlNode& lastNode = *dfsList[clique.last().dfsNum];

		bool hasEdgeToHeader = false;
		bool noShortEdgeOutsiteR = true;

		ControlEdge* limit = lastNode.getSuccessorsEnd();
		for (ControlEdge* edge = lastNode.getSuccessorsBegin(); edge != limit; edge++) {
			if (edge->getTarget().dfsNum == loopToSchedule.header)
				hasEdgeToHeader = true;
			if (!edge->longFlag && R.test(edge->getTarget().dfsNum))
				noShortEdgeOutsiteR = false;
		}

		if (hasEdgeToHeader && noShortEdgeOutsiteR && (!lastNode.hasControlKind(ckSwitch)) && (lastNode.dfsNum != loopToSchedule.header)) {
			// We found this clique. We remove the last node N of this clique and prepend it to this 
			// subloop's first clique. Now as long as this clique ends with some node N' that satisfies 
			// all of the properties below, we remove N' from the end of this clique and prepend it 
			// to this subloop's first clique.
			//
			//  1.There are no short edges E: N'->A for which A is outside R. 
			//  2.N' is not a switch node. 
			//  3.N' is not the loop's header. 
			ScheduledNode& sNode = scheduledNodes[lastNode.dfsNum];
			sNode.remove();
			loopToSchedule.cliques.first().addFirst(sNode);

			for (DoublyLinkedList<ScheduledNode>::iterator n = clique.end(); !clique.done(n);) {
				ControlNode& node = *dfsList[clique.get(n).dfsNum];
				n = clique.retreat(n);
			  
				bool noShortEdgeOutsiteR = true;
				ControlEdge* limit = node.getSuccessorsEnd();
				for (ControlEdge* edge = node.getSuccessorsBegin(); edge != limit; edge++)
					if (!edge->longFlag && R.test(edge->getTarget().dfsNum)) {
						noShortEdgeOutsiteR = false;
						break;
					}
				if (noShortEdgeOutsiteR && (!node.hasControlKind(ckSwitch)) && (node.dfsNum != loopToSchedule.header)) {
					// The condition is satisfied.
					ScheduledNode& sNode = scheduledNodes[lastNode.dfsNum];
					sNode.remove();
					loopToSchedule.cliques.first().addFirst(sNode);
				}
				else
					break;
			}

			if (clique.empty())
				clique.remove();
			break;
		}
	}

#if defined(DEBUG_SCHEDULER)
	fprintf(stdout, "done scheduled nodes: [ ");
	for (DoublyLinkedList<Clique>::iterator x = loopToSchedule.cliques.begin(); !loopToSchedule.cliques.done(x); x = loopToSchedule.cliques.advance(x)) {
		fprintf(stdout, "[ ");
		Clique& clique = loopToSchedule.cliques.get(x);
		for (DoublyLinkedList<ScheduledNode>::iterator z = clique.begin(); !clique.done(z); z = clique.advance(z))
			fprintf(stdout, "N%d ", clique.get(z).dfsNum);
		fprintf(stdout, "] ");
	}
	fprintf(stdout, "]\n");
#endif
}

//
// Set the long edge flag to the given edge in region.
//
void ControlNodeScheduler::initializeLongEdgeFlag(ControlEdge& edge, LoopHierarchyNode& inLoop, FastBitSet& RR, FastBitSet& Rlocal)
{
	Int32 A = edge.getSource().dfsNum;

	if (!RR.test(A)) { // Condition 1 - A is outside RR.
		edge.longFlag = true;
		return;
	}

	Int32 B = edge.getTarget().dfsNum;

	if (A >= B) { // Condition 3 - edge is a backward edge.
		edge.longFlag = true;
		return;
	}

	if (edge.getSource().hasControlKind(ckSwitch)) { // Condition 4 - A is a switch node.
		edge.longFlag = true;
		return;
	}

	if (!Rlocal.test(A)) { // Condition 5 - A is in a subloop.

		DoublyLinkedList<LoopHierarchyNode>& subLoops = inLoop.getSuccessors();
		for (DoublyLinkedList<LoopHierarchyNode>::iterator l = subLoops.begin(); !subLoops.done(l); l = subLoops.advance(l)) {

			LoopHierarchyNode& subLoop = subLoops.get(l);
			if (subLoop.nodes.test(A)) { // subLoop is the largest inLoop's subloop that contains A.
				for (DoublyLinkedList<Clique>::iterator c = subLoop.cliques.begin(); !subLoop.cliques.done(c); c = subLoop.cliques.advance(c))
					if (subLoop.cliques.get(c).last().dfsNum == A) { // A is the last node of this clique.
						edge.longFlag = false;
						return;
					}

				// If we get there then A is not the last node of some cliques
				edge.longFlag = true;
				return;
			}
		}
	}

	if (!Rlocal.test(B)) { // Condition 6 - B is in a subloop.

		DoublyLinkedList<LoopHierarchyNode>& subLoops = inLoop.getSuccessors();
		for (DoublyLinkedList<LoopHierarchyNode>::iterator l = subLoops.begin(); !subLoops.done(l); l = subLoops.advance(l)) {

			LoopHierarchyNode& subLoop = subLoops.get(l);
			if (subLoop.nodes.test(B)) { // subLoop is the largest inLoop's subloop that contains B.
				for (DoublyLinkedList<Clique>::iterator c = subLoop.cliques.begin(); !subLoop.cliques.done(c); c = subLoop.cliques.advance(c))
					if (subLoop.cliques.get(c).last().dfsNum == B) { // B is the last node of this clique.
						edge.longFlag = false;
						return;
					}

				// If we get there then B is not the last node of some cliques
				edge.longFlag = true;
				return;
			}
		}
	}

	// Condition 2 - edge is any exception or return edge.
	switch (edge.getSource().getControlKind()) {
		case ckCatch: case ckEnd:
			edge.longFlag = true;
			return;
		default:
			break;
	}

	switch (edge.getTarget().getControlKind()) {
		case ckCatch: case ckEnd:
			edge.longFlag = true;
			return;
		default:
			break;
	}

	edge.longFlag = false; // Condition 7.
}

//
// Fill the dominatorsMatrix with the dominators of all the nodes in this graph. 
// Each row is a bitset of the dominators' dfsNum for the ControlNode which dfsNum is the row index.
// dominatorsMatrix must have (nNodes + 1) rows & nNodes columns. (One extra row is used for
// temporary calculations).
//
void ControlNodeScheduler::findDominators()
{
	// Initially each ControlNode is dominated by all the other ControlNodes.
	dominatorsMatrix.set();

	// The Begin Node is initialized independently as it is only dominated by itself.
	dominatorsMatrix.clearRow(0);
	dominatorsMatrix.set(0, 0);

	bool changed; // loop condition.

	do {
		changed = false;

		for (Uint32 n = 1; n < nNodes; n++) {
			dominatorsMatrix.setRow(nNodes);
		  
			// The dominators of a ControlNode are the intersection of the dominators of its
			// predecessors plus itself.
			const DoublyLinkedList<ControlEdge>& predecessors = dfsList[n]->getPredecessors();
			for (DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin(); !predecessors.done(i); i = predecessors.advance(i))
				dominatorsMatrix.andRows(predecessors.get(i).getSource().dfsNum, nNodes);
			dominatorsMatrix.set(nNodes, n);

			if (!dominatorsMatrix.compareRows(nNodes, n)) {
				changed = true;
				dominatorsMatrix.copyRows(nNodes, n);
			}
		}
	} while(changed);
}

//
// Find the loop headers in the given array of ControlNodes & return the number
// of headers found.
//
Uint32 ControlNodeScheduler::findLoopHeaders()
{
	Uint32 nLoopHeaders = 0; // Number of loop headers in this graph.

	findDominators();

	for (Uint32 n = 0; n < nNodes; n++) {
		ControlNode& node = *dfsList[n];
		Uint32 nodeIndex = node.dfsNum;
	  
		const DoublyLinkedList<ControlEdge>& predecessors = node.getPredecessors();
		for (DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin(); !predecessors.done(i); i = predecessors.advance(i)) {
			Uint32 predecessorIndex = predecessors.get(i).getSource().dfsNum;

			// A loop header is a node with an incoming backward edge. This edge's source must be dominated
			// by itself (called regular backward edge).
			if ((predecessorIndex >= nodeIndex) && dominatorsMatrix.test(predecessorIndex, nodeIndex)) {
				loopHeaders.set(nodeIndex);
				nLoopHeaders++;
				break;
			}
		}
	}

	if (!loopHeaders.test(0)) {
		// The begin node is always considered to be a loop header (even if it's not).
		loopHeaders.set(0);
		nLoopHeaders++;
	}

	return nLoopHeaders;
}

//
// Build the LoopHierarchyTree, find the nodes for each loop and return the top of the tree.
//
LoopHierarchyNode& ControlNodeScheduler::buildLoopHierarchyTree()
{
	Uint32* nodeStack = new(pool) Uint32[nNodes];		// Stack of nodes to look at.

	Uint32 nLoopHeaders = findLoopHeaders();

	LoopHierarchyNode* loopNodes = new(pool) LoopHierarchyNode[nLoopHeaders];
	LoopHierarchySet loopSet(pool, loopNodes, nNodes);	// Set of the loop headers in this graph. 

	// If the BeginNode is considered to be a loop then its body 
	// contains all the nodes in this graph.
	LoopHierarchyNode& beginNode = loopSet[0];
	beginNode.header = 0;
	beginNode.nodes.sizeTo(pool, nNodes);
	beginNode.nodes.set(0, nNodes - 1);
  
	for (Int32 h = loopHeaders.nextOne(0); h != -1; h = loopHeaders.nextOne(h)) {
		//
		// Place this loop in the tree.
		//

		FastBitSet dominators(dominatorsMatrix.getRow(h), nNodes);

		Int32 parent = h;

		do // parent must be a loop header && this node must be included in the parent's loop nodes.
			parent =  dominators.previousOne(parent);
		while (!(loopHeaders.test(parent) && loopSet[parent].nodes.test(h)));

		loopSet[parent].addSuccessor(loopSet[h]);

		//
		// Find this loop's nodes.
		//

		Uint32* stackPointer = nodeStack;
		LoopHierarchyNode& loop = loopSet[h];

		// Initialize the loop header's variables.
		loop.header = h;
		loop.nodes.sizeToAndClear(pool, nNodes);
		loop.nodes.set(h);

		// Push the tails of this loop. Each tail must be dominated by the loop header.
		const DoublyLinkedList<ControlEdge>& headerPredecessors = dfsList[h]->getPredecessors();
		for (DoublyLinkedList<ControlEdge>::iterator i = headerPredecessors.begin(); !headerPredecessors.done(i); i = headerPredecessors.advance(i)) {
			Uint32 predecessorIndex = headerPredecessors.get(i).getSource().dfsNum;

			if ((predecessorIndex > Uint32(h)) && dominatorsMatrix.test(predecessorIndex, h)) {
				loop.nodes.set(predecessorIndex);
				*stackPointer++ = predecessorIndex;
			}
		}
	  
		while (stackPointer != nodeStack) {
			Uint32 n = *--stackPointer;

			const DoublyLinkedList<ControlEdge>& predecessors = dfsList[n]->getPredecessors();
			for (DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin(); !predecessors.done(i); i = predecessors.advance(i)) {
				Uint32 predecessorIndex = predecessors.get(i).getSource().dfsNum;
				if (!loop.nodes.test(predecessorIndex)) {
					loop.nodes.set(predecessorIndex);
					*stackPointer++ = predecessorIndex;
				}
			}
		}
	}

	return loopSet[0];
}

//
// Return an array of scheduled nodes. This array has nNodes elements.
//
ControlNode** ControlNodeScheduler::getScheduledNodes()
{
	// Initialize the ScheduledNodes.
	scheduledNodes = new(pool) ScheduledNode[nNodes];
	for (Uint32 i = 0; i < nNodes; i++) {
		ScheduledNode& node = scheduledNodes[i];
		node.dfsNum = i;
	}

	nodesAlreadyScheduled.sizeToAndClear(nNodes);

	//
	// Determine main nodes (called working nodes).
	//

	Uint32 generation = ControlNode::getNextGeneration();
	Uint32* nodeStack = new(pool) Uint32[nNodes]; // Stack of nodes to look at.
	Uint32* stackPointer  = nodeStack;
	*stackPointer++ = 0; // Push the beginNode.

	while (stackPointer != nodeStack) {
		Uint32 n = *--stackPointer;
		ControlNode& node = *dfsList[n];

		node.workingNode = true;

		ControlEdge* limit = node.getSuccessorsEnd();
		for (ControlEdge* edge = node.getSuccessorsBegin(); edge != limit; edge++) {
			ControlNode& successor = edge->getTarget();

			if (successor.generation != generation) {
				switch(successor.getControlKind()) {
					case ckCatch:
					case ckEnd:
						// We are not following the exception edges.
						break;

					default:
						successor.generation = generation;
						*stackPointer++ = successor.dfsNum;
						break;
				}
			}
		}
	}

	//
	// Schedule the loops from the inside out.
	//

	struct EdgeStack 
	{
		DoublyLinkedList<LoopHierarchyNode>* linkedList;
		DoublyLinkedList<LoopHierarchyNode>::iterator position;
	};

	LoopHierarchyNode& top = buildLoopHierarchyTree();

	EdgeStack *edgeStack = new(pool) EdgeStack[nNodes];
	DoublyLinkedList<LoopHierarchyNode> edgeToBeginNodeList;
	edgeToBeginNodeList.addLast(top);

	DoublyLinkedList<LoopHierarchyNode>* currentList = &edgeToBeginNodeList;
	DoublyLinkedList<LoopHierarchyNode>::iterator currentIterator = edgeToBeginNodeList.begin();
	EdgeStack *edgeStackPointer = edgeStack;

	while (true) {
		if (currentList->done(currentIterator)) {
			if (edgeStackPointer == edgeStack)
				break;

			--edgeStackPointer;
			currentList = edgeStackPointer->linkedList;
			currentIterator = edgeStackPointer->position;

			// At this point all the loops below this one in the loop hierarchy tree have
			// been scheduled. 
			LoopHierarchyNode& loopToSchedule = currentList->get(currentIterator->prev); 

			scheduleLoop(loopToSchedule);
		} else {
			LoopHierarchyNode& loop = currentList->get(currentIterator);
			currentIterator = currentList->advance(currentIterator);

			edgeStackPointer->linkedList = currentList;
			edgeStackPointer->position = currentIterator;
			edgeStackPointer++;
		  
			currentList = &loop.getSuccessors();
			currentIterator = currentList->begin();
		}
	}


	// FIXME: need to schedule the remaining nodes (exceptions).

	// Summarize the scheduling.
	ControlNode** result = new(pool) ControlNode*[nNodes];
	ControlNode** ptr = result;

	for (DoublyLinkedList<Clique>::iterator c = top.cliques.begin(); !top.cliques.done(c); c = top.cliques.advance(c)) {
		Clique& clique = top.cliques.get(c);
		for (DoublyLinkedList<ScheduledNode>::iterator s = clique.begin(); !clique.done(s); s = clique.advance(s))
			*ptr++ = dfsList[clique.get(s).dfsNum];
	}

	// Add the endNode.
	*ptr++ = dfsList[nNodes - 1];

	PR_ASSERT(ptr == &result[nNodes]);
  
	return result;
}
