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
#ifndef CONTROLGRAPH_H
#define CONTROLGRAPH_H

#include "ControlNodes.h"
#include "LogModule.h"

struct MethodDescriptor;

class ControlGraph
{
  public:
	Pool &pool;									// Memory pool from which this graph is allocated
	DoublyLinkedList<ControlNode> controlNodes;	// List of control nodes in this graph
	Uint32 nMonitorSlots;						// Number of slots needed for MEnter/MExit primitives in this graph

  private:
	ControlNode beginNode;						// The Begin node
	ControlNode endNode;						// The End node
	ControlNode *returnNode;					// The graph's Return node or nil if none
	ControlNode *recycleBuffer;					// A free ControlNode ready for reuse or nil if none

  public:
	ControlNode** dfsList;						// List of control nodes in depth first search order.
	Uint32 nNodes;
	bool hasBackEdges;
	ControlNode** lndList;						// List of control nodes in loop nesting depth order.

	ControlGraph(Pool &pool, uint nArguments, const ValueKind *argumentKinds, bool hasSelfArgument, Uint32 nMonitorSlots);
  private:
	ControlGraph(const ControlGraph &);			// Copying forbidden
	void operator=(const ControlGraph &);		// Copying forbidden
  public:

	ControlNode &newControlNode();
	void recycle(ControlNode &cn);

	ControlNode &getBeginNode() {return beginNode;}
	ControlNode &getEndNode() {return endNode;}
	ControlNode *getReturnNode() const {return returnNode;}
	void setReturnNode(ControlNode &cr) {assert(!returnNode && cr.hasControlKind(ckReturn)); returnNode = &cr;}
	Uint32 assignProducerNumbers(Uint32 base);

	bool dfsListIsValid() const {return dfsList != 0;}
	void dfsSearch();
	bool lndListIsValid() const {return lndList != 0;}
	void lndSearch();

  #ifdef DEBUG_LOG
	void print(LogModuleObject &f);
  #endif
    
#ifdef DEBUG
    void validate();                        // verifies that this control graph is 'correct'
#endif

};


// ----------------------------------------------------------------------------
// Control flow builders and utilities


DataNode &makeVariable(const VariableOrConstant &input, ControlNode &cn, Uint32 bci);
const VariableOrConstant &followVariableBack(const VariableOrConstant &input, ControlNode &cn, ControlEdge &e);

ControlNode &joinControlFlows(uint nPredecessors, ControlNode *const*predecessors, ControlGraph &cg);
void joinDataFlows(uint nPredecessors, ControlNode &cn, const VariableOrConstant *inputs, VariableOrConstant &output);
void addDataFlow(ControlNode &cn, DataConsumer &consumer, const VariableOrConstant &newSource);
void changeDataFlow(ControlNode &cn, ControlEdge &e, DataConsumer &consumer, const VariableOrConstant &newSource);

ControlNode &obtainSuccessorSite(ControlNode &cn, Uint32 successorNumber = 0);
ControlNode &insertControlNodeAfter(ControlNode &cn, ControlNode *&successor, DoublyLinkedList<ControlEdge>::iterator &where,
									Uint32 successorNumber = 0);
ControlNode &insertControlNodeBefore(ControlNode &cn);
ControlNode *insertControlNodeBefore(ControlNode &cn, Function1<bool, ControlEdge &> &f);


// --- INLINES ----------------------------------------------------------------

//
// Initialize a ControlNode.
// Don't call this directly; call ControlGraph::newControlNode instead.
//
inline ControlNode::ControlNode(ControlGraph &cg):
	noRecycle(false),
	controlGraph(cg),
	generation(0),
	liveAtBegin(cg.pool),
	liveAtEnd(cg.pool),
	workingNode(false)
{
  #if defined(DEBUG) || defined(DEBUG_LOG)
	controlKind = ckNone;
  #endif
  #ifdef DEBUG
	successorsBegin = 0;
	successorsEnd = 0;
	phisDisengaged = 0;
  #endif
}


//
// Return the pool for allocating primitives in this control node.
//
inline Pool &ControlNode::getPrimitivePool() const
{
	return controlGraph.pool;
}


//
// Recycle the ControlNode so that it can be reused by newControlNode.
// The given ControlNode must satisfy the empty() method; have no predecessors,
// instructions, extra, or generation; and belong to this ControlGraph.
//
inline void ControlGraph::recycle(ControlNode &cn)
{
	assert(&cn.controlGraph == this);
	cn.remove();
	cn.recycle();
	recycleBuffer = &cn;
}
#endif
