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

#include "ControlGraph.h"
#include "GraphUtils.h"
#if defined(DEBUG) && (defined(WIN32) || defined(USE_MESA)) && defined(IGVISUALIZE)
#include "IGVisualizer.h"
#endif
#include <math.h>

// ----------------------------------------------------------------------------
// ControlGraph


#ifdef _WIN32
 #pragma warning(disable: 4355)
#endif
//
// Create a new ControlGraph that contains only the BEGIN and END nodes.
// The graph has nArguments incoming arguments whose kinds are given by argumentKinds.
// If hasSelfArgument is true, the first argument is the 'this' argument.
// The graph initially has nMonitorSlots slots allocated for MEnter/MExit pairs.
// The graph starts with no control or data edges.
//
ControlGraph::ControlGraph(Pool &pool, uint nArguments, const ValueKind *argumentKinds, bool hasSelfArgument, Uint32 nMonitorSlots):
	pool(pool),
	nMonitorSlots(nMonitorSlots),
	beginNode(*this),
	endNode(*this),
	returnNode(0),
	recycleBuffer(0),
	dfsList(0),
	lndList(0)
{
	beginNode.setControlBegin(nArguments, argumentKinds, hasSelfArgument, 0);
	// asharma - fix this! - bci needs to be set properly here.
	endNode.setControlEnd(0);
	controlNodes.addFirst(beginNode);
	controlNodes.addLast(endNode);
}
#ifdef _WIN32
 #pragma warning(default: 4355)
#endif


//
// Create a new ControlNode in this graph and return it.  The ControlNode
// will have no ControlExtra; the caller is responsible for creating that.
//
ControlNode &ControlGraph::newControlNode()
{
	ControlNode *cn;
	if (recycleBuffer) {
		cn = recycleBuffer;
		recycleBuffer = 0;
	} else
		cn = new(pool) ControlNode(*this);
	controlNodes.addLast(*cn);
	return *cn;
}


//
// Assign a unique producerNumber to each outgoing data edge in the ControlGraph.
// The first number assigned will be base; return the last number assigned + 1.
//
Uint32 ControlGraph::assignProducerNumbers(Uint32 base)
{
	for (DoublyLinkedList<ControlNode>::iterator ci = controlNodes.begin(); !controlNodes.done(ci); ci = controlNodes.advance(ci)) {
		ControlNode &cn = controlNodes.get(ci);

		DoublyLinkedList<PhiNode> &phn = cn.getPhiNodes();
		for (DoublyLinkedList<PhiNode>::iterator phi = phn.begin(); !phn.done(phi); phi = phn.advance(phi))
			base = phn.get(phi).assignProducerNumbers(base);

		DoublyLinkedList<Primitive> &prn = cn.getPrimitives();
		for (DoublyLinkedList<Primitive>::iterator pri = prn.begin(); !prn.done(pri); pri = prn.advance(pri))
			base = prn.get(pri).assignProducerNumbers(base);
	}
	return base;
}


class ControlDFSHelper
{
  public:
	typedef ControlEdge Successor;
	typedef ControlNode *NodeRef;

	bool hasBackEdges;							// True if the graph contains a cycle
  private:
	ControlNode **dfsList;						// Alias to dfsList from the ControlGraph

  public:
	ControlDFSHelper(ControlNode **dfsList): hasBackEdges(false), dfsList(dfsList) {}

	static Successor *getSuccessorsBegin(NodeRef n) {return n->getSuccessorsBegin();}
	static Successor *getSuccessorsEnd(NodeRef n) {return n->getSuccessorsEnd();}
	static bool isNull(Successor &) {return false;}
	static NodeRef getNodeRef(const Successor& s) {return &s.getTarget();}
	static bool isMarked(NodeRef n) {return n->dfsNum != ControlNode::unmarked;}
	static bool isUnvisited(NodeRef n) {return n->dfsNum == ControlNode::unvisited;}
	static bool isNumbered(NodeRef n) {return n->dfsNum >= 0;}
	static void setMarked(NodeRef n) {n->dfsNum = ControlNode::unvisited;}
	static void setVisited(NodeRef n) {n->dfsNum = ControlNode::unnumbered;}
	void setNumbered(NodeRef n, Int32 i) {assert(i >= 0); n->dfsNum = i; dfsList[i] = n;}
	void notePredecessor(NodeRef) {}
	void noteIncomingBackwardEdge(NodeRef) {hasBackEdges = true;}
};


//
// Initialize the dfsNum field of each ControlNode in this ControlGraph
// to the ControlNode's serial number.  Also set up dfsList so that
// if n is ControlNode c's dfsNum, then dfsList[n] = &c.
//
#include "NativeFormatter.h"
void ControlGraph::dfsSearch()
{
	// Initialize the ControlNodes' dfsNum fields.
	Uint32 maxNControlNodes = 0;
	for (DoublyLinkedList<ControlNode>::iterator i = controlNodes.begin(); !controlNodes.done(i); i = controlNodes.advance(i)) {
		controlNodes.get(i).dfsNum = ControlNode::unmarked;
		maxNControlNodes++;
	}

	// Count the ControlNodes reachable from beginNode.
	dfsList = new(pool) ControlNode *[maxNControlNodes];
	ControlDFSHelper dfsHelper(dfsList);
	ControlEdge beginEdge;
	ControlEdge endEdge;
	beginEdge.setTarget(beginNode);
	endEdge.setTarget(endNode);
	SearchStackEntry<ControlEdge> *searchStack = new SearchStackEntry<ControlEdge>[maxNControlNodes];
	nNodes = graphSearchWithEnd(dfsHelper, beginEdge, endEdge, maxNControlNodes, searchStack);

	// Now do the actual depth-first search.
	depthFirstSearchWithEnd(dfsHelper, beginEdge, endEdge, nNodes, searchStack);
	hasBackEdges = dfsHelper.hasBackEdges;
#ifndef WIN32 // ***** Visual C++ has a bug in the code for delete[].
	delete[] searchStack;
#endif
}


#include "CodeGenerator.h"

struct LoopSearchDepthInfo
{
  Uint32 begin;
  Uint32 count;
};

void ControlGraph::lndSearch()
{
  Uint32 i;

  if (!dfsListIsValid())
	dfsSearch();

  lndList = new(pool) ControlNode *[nNodes];
  
  // Initialize the control node's depth info.
  for (i = 0; i < nNodes; i++)
	{
	  dfsList[i]->loopId = 0;
	  dfsList[i]->loopDepth = 0;
	  lndList[i] = dfsList[i];
	}

  if (!hasBackEdges)
	return;

  LoopSearchDepthInfo *depthInfo = new LoopSearchDepthInfo[nNodes + 1];
  for (i = 1; i < nNodes + 1; i++) depthInfo[i].count = 0;
  depthInfo[0].begin = 0;
  depthInfo[0].count = nNodes;
  
  ControlNode** stack = new ControlNode *[nNodes];
#ifdef DEBUG
  ControlNode** stackEnd = &stack[nNodes];
#endif
  Uint32 loopId = 1;

  for (Uint32 n = 0; n < nNodes; n++)
	{
	  ControlNode* node = dfsList[n];
	  ControlEdge* successors_limit = node->getSuccessorsEnd();

	  for (ControlEdge* successor = node->getSuccessorsBegin(); successor != successors_limit; successor++)
		if (successor->getTarget().dfsNum < node->dfsNum)    // retreating edge.
		  {
			ControlNode** sp = stack;
			depthInfo[successor->getTarget().loopDepth++].count--;
			depthInfo[successor->getTarget().loopDepth].count++;
			successor->getTarget().loopId = ++loopId;
			
			depthInfo[node->loopDepth++].count--;
			depthInfo[node->loopDepth].count++;
			node->loopId = loopId;
			
			*sp++ = node;                                // push the first element on the stack.
			while (sp != stack)
			  {
				const DoublyLinkedList<ControlEdge>& predecessors = (*--sp)->getPredecessors();
				for (DoublyLinkedList<ControlEdge>::iterator predecessor = predecessors.begin();
					 !predecessors.done(predecessor); 
					 predecessor = predecessors.advance(predecessor))
				  {
					ControlNode &prev_node = predecessors.get(predecessor).getSource();
					if (prev_node.loopId != loopId)
					  {
						depthInfo[prev_node.loopDepth].count--;
						prev_node.loopDepth++;
						depthInfo[prev_node.loopDepth].count++;
						prev_node.loopId = loopId;
						*sp++ = &prev_node;
#ifdef DEBUG
						assert(sp < stackEnd);
#endif
					  }
				  }
			  }
		  }
	}

  // summarize 
  for(Uint32 d = 1; d < nNodes + 1; d++) 
	  depthInfo[d].begin = depthInfo[d - 1].begin + depthInfo[d - 1].count;

  for(Int32 j = nNodes - 1; j >= 0; j--)
	{
	  ControlNode* node = dfsList[j];
	  lndList[depthInfo[node->loopDepth].begin++] = node;
	  node->nVisited = (float) pow(10.0, (int) node->loopDepth);
	}

  delete[] stack;
  delete[] depthInfo;
}


#ifdef DEBUG_LOG
//
// Print a ControlGraph for debugging purposes.
//
void ControlGraph::print(LogModuleObject &f)
{
	dfsSearch();

	Uint32 nProducers = assignProducerNumbers(1) - 1;
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("ControlGraph %p (%d producers)\n", this, nProducers));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("beginNode: "));
	beginNode.printRef(f);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\nendNode: "));
	endNode.printRef(f);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\nreturnNode: "));
	if (returnNode)
		returnNode->printRef(f);
	else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("none"));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\nControl nodes:\n\n"));
	for (Uint32 i = 0; i != nNodes; i++)
		dfsList[i]->printPretty(f, 4);

	bool printedHeader = false;
	for (DoublyLinkedList<ControlNode>::iterator j = controlNodes.begin(); !controlNodes.done(j); j = controlNodes.advance(j)) {
		ControlNode &cn = controlNodes.get(j);
		if (cn.dfsNum < 0) {
			if (!printedHeader) {
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Unreachable control nodes:\n\n"));
				printedHeader = true;
			}
			cn.printPretty(f, 4);
		}
	}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("End ControlGraph\n\n"));
}
#endif


// ----------------------------------------------------------------------------
// Control flow builders and utilities


//
// Return input coerced to a variable.  If input is already a variable, return
// its variable.  If it is a constant, allocate a PrimConst node in the given
// control node with the constant's value and return that PrimConst.
//
DataNode &makeVariable(const VariableOrConstant &input, ControlNode &cn,
					   Uint32 bci)
{
	if (input.isConstant()) {
		PrimConst &primConst = *new(cn.getPrimitivePool())
			PrimConst(input.getKind(), input.getConstant(), bci); 
		cn.appendPrimitive(primConst);
		return primConst;
	} else
		return input.getVariable();
}


//
// This method follows a variable's value backwards through a phi node.
//
// Edge e is one of the incoming edges of node cn.  input is a variable or
// constant available in cn.  If input is a constant, return it unchanged;
// if it is a variable, return the VariableOrConstant representing its value
// in e's source.  The result may be different from input when node cn
// contains a phi node that outputs input.
//
// Note that the returned value is a reference and may change if input or
// one of the phi node's inputs changes.
//
const VariableOrConstant &followVariableBack(const VariableOrConstant &input, ControlNode &cn, ControlEdge &e)
{
	assert(&e.getTarget() == &cn);
	if (input.isVariable()) {
		DataNode &inputVar = input.getVariable();
		if (inputVar.hasCategory(pcPhi) && inputVar.getContainer() == &cn)
			return inputVar.nthInput(cn.getPredecessors().index(e));
	}
	return input;
}


//
// Make a control flow merge of nPredecessors predecessor control nodes into a
// new control node. Return the new control node, which may be the incoming
// control node if nPredecessors is 1.
// The caller is responsible for creating any phi nodes in the new control node;
// joinDataFlows is a convenient way of creating such phi nodes.
//
ControlNode &joinControlFlows(uint nPredecessors, ControlNode *const*predecessors, ControlGraph &cg)
{
  #ifdef DEBUG
	assert(nPredecessors != 0);
	// Make sure that none of the predecessors is null.
	for (uint i = 0; i != nPredecessors; i++)
		assert(predecessors[i]);
  #endif

	if (nPredecessors == 1)
		return *predecessors[0];

	ControlNode &cn = cg.newControlNode();
	while (nPredecessors--) {
		ControlNode &pred = **predecessors++;
		pred.setControlBlock();
		cn.addPredecessor(pred.getNormalSuccessor());
	}
	return cn;
}


//
// Merge a set of variables (inputs) into a single variable (output) on a
// control flow merge entering node cn.  This usually generates a phi node
// with the given inputs and output inside cn, but it might just return one
// of the inputs if all of the inputs are the same.
// The inputs must be given in the same order as the predecessors were added
// to cn; the i-th input must be generated by the i-th predecessor.
//
// nPredecessors==1 is a special case to match the optimization inside joinControlFlows:
// if nPredecessors is 1, then the input is simply copied to the output and
// it is guaranteed that no phi nodes will be created in this case.
//
void joinDataFlows(uint nPredecessors, ControlNode &cn, const VariableOrConstant *inputs, VariableOrConstant &output)
{
	assert(nPredecessors != 0);
	if (nPredecessors != 1) {
		assert(cn.getPredecessors().lengthIs(nPredecessors));
		const VariableOrConstant *inputsLimit = inputs + nPredecessors;
		ValueKind kind = inputs->getKind();

	  #ifdef DEBUG
		// Assert that all inputs have the same kind.
		for (const VariableOrConstant *i = inputs + 1; i != inputsLimit; i++)
			assert(i->hasKind(kind));
	  #endif

		// Are all of the inputs the same?
		for (const VariableOrConstant *input = inputs + 1; input != inputsLimit; input++)
			if (*input != *inputs) {

				// At least one input is different.  Generate a phi node.
				Pool &pool = cn.getPrimitivePool();
				PhiNode &phiNode = *new(pool) PhiNode(nPredecessors, kind, pool);
				DoublyLinkedList<ControlEdge>::iterator pred = cn.getPredecessors().begin();
				for (input = inputs; input != inputsLimit; input++) {
					// asharma - fix me
					phiNode.addInput(makeVariable(*input, cn.getPredecessors().get(pred).getSource(), 0), pool);
					pred = cn.getPredecessors().advance(pred);
				}
				cn.addPhiNode(phiNode);
				output.setVariable(phiNode);
				return;
			}
	}

	// All of the inputs are the same.  Just return the first one.
	output = inputs[0];
}


//
// cn is a control node that recently acquired an additional predecessor p via
// addPredecessor; that predecessor is now cn's last predecessor.
// consumer is a DataConsumer in cn or one of its control descendants that
// consumes a variable live at the entry to cn.
//
// This method adds or modifies the phi nodes in cn as appropriate so that:
//    if control comes into cn via one of its original predecessors, consumer
//       will have its original value;
//    if control comes into cn from its last predecessor p, consumer will have
//       the value given by newSource (which must originate in p or one of its
//       control ancestors).
//
// consumer may or may not have a phi node in cn; if it does, then that phi
// node should not have an input corresponding to predecessor p; this method
// will add such an input.
//
// addDataFlow or a similar method should be called on every variable live on
// entry to an existing control node that just acquired a new predecessor.
//
void addDataFlow(ControlNode &cn, DataConsumer &consumer, const VariableOrConstant &newSource)
{
	assert(newSource.hasKind(consumer.getKind()));

	// If consumer is a phi node inside cn, set dn to that phi node;
	// if not, set dn to nil.
	DataNode *dn = 0;
	if (consumer.isVariable()) {
		dn = &consumer.getVariable();
		if (!(dn->hasCategory(pcPhi) && dn->getContainer() == &cn))
			dn = 0;
	}

	if (dn || consumer != newSource) {
		Pool &pool = cn.getPrimitivePool();
		// asharma - fix me
		DataNode &newSourceVar = makeVariable(newSource, cn.getPredecessors().last().getSource(), 0);

		if (dn)
			// We already have a phi node in cn for this variable.  Add that last input.
			PhiNode::cast(*dn).addInput(newSourceVar, pool);
		else {
			// We don't have a phi node in cn for this variable.
			// We shall create one.
			Uint32 nPredecessors = cn.getPredecessors().length();
			PhiNode &phiNode = *new(pool) PhiNode(nPredecessors, consumer.getKind(), pool);
			DoublyLinkedList<ControlEdge>::iterator pred = cn.getPredecessors().begin();
			while (--nPredecessors) {
				// asharma - fix me
				phiNode.addInput(makeVariable(consumer, cn.getPredecessors().get(pred).getSource(), 0), pool);
				pred = cn.getPredecessors().advance(pred);
			}
			phiNode.addInput(newSourceVar, pool);
			cn.addPhiNode(phiNode);
			consumer.clear();
			consumer.setVariable(phiNode);
		}
	}
}


//
// cn is a control node with an incoming edge e.  consumer is a DataConsumer
// in cn or one of its control descendants that consumes a variable live at the
// entry to cn.
//
// This method adds or modifies the phi nodes in cn as appropriate so that:
//    if control comes into cn via an edge other than e, consumer will have
//       the same value it does now;
//    if control comes into cn via edge e, consumer will have the value given
//       by newSource.
//
// consumer may or may not have a phi node in cn; if it does, then that phi
// node should have an input corresponding to edge e.
//
void changeDataFlow(ControlNode &cn, ControlEdge &e, DataConsumer &consumer, const VariableOrConstant &newSource)
{
	assert(&e.getTarget() == &cn && newSource.hasKind(consumer.getKind()));

	// If consumer is a phi node inside cn, set dn to that phi node;
	// if not, set dn to nil.
	DataNode *dn = 0;
	if (consumer.isVariable()) {
		dn = &consumer.getVariable();
		if (!(dn->hasCategory(pcPhi) && dn->getContainer() == &cn))
			dn = 0;
	}

	if (dn || consumer != newSource) {
		// asharma - fix me
		DataNode &newSourceVar = makeVariable(newSource, e.getSource(), 0);
		Uint32 eIndex = cn.getPredecessors().index(e);

		if (dn) {
			// We already have a phi node in cn for this variable.
			DataConsumer &input = PhiNode::cast(*dn).nthInput(eIndex);
			input.clear();
			input = newSource;
		} else {
			// We don't have a phi node in cn for this variable.
			// We shall create one.
			Pool &pool = cn.getPrimitivePool();
			Uint32 nPredecessors = cn.getPredecessors().length();
			assert(eIndex < nPredecessors);
			PhiNode &phiNode = *new(pool) PhiNode(nPredecessors, consumer.getKind(), pool);
			DoublyLinkedList<ControlEdge>::iterator pred = cn.getPredecessors().begin();
			for (Uint32 i = 0; i != nPredecessors; i++) {
				// asharma - fix me
				phiNode.addInput(i == eIndex ? newSourceVar : makeVariable(consumer, cn.getPredecessors().get(pred).getSource(), 0), pool);
				pred = cn.getPredecessors().advance(pred);
			}
			cn.addPhiNode(phiNode);
			consumer.clear();
			consumer.setVariable(phiNode);
		}
	}
}


//
// Return a ControlNode appropriate for placing new DataNodes that should be
// executed immediately after all of this ControlNode's DataNodes if this
// ControlNode's execution leaves along the successor edge with the given number.
// The returned ControlNode may be a new ControlNode spliced into the control
// graph between this ControlNode and its given successor, or it may be an
// existing ControlNode -- either this one or its successor -- if that existing
// ControlNode can be legally used to hold the extra DataNodes.
//
// The edge with the given successorNumber should not be an exception or return edge.
//
ControlNode &obtainSuccessorSite(ControlNode &cn, Uint32 successorNumber)
{
	assert(successorNumber < cn.nNormalSuccessors());
	switch (cn.getControlKind()) {
		case ckBlock:

			return cn;	// We can add more DataNodes to the end of this node.
		case ckBegin:
		case ckIf:
		case ckSwitch:
		case ckExc:
		case ckAExc:
		case ckCatch:
			{
				ControlEdge &successorEdge = cn.nthSuccessor(successorNumber);
				ControlNode &successor = successorEdge.getTarget();
				assert(hasNormalInputs(successor.getControlKind()));
				if (successor.getPredecessors().lengthIs(1))
					// The successor has only one input, so we can add more DataNodes to its beginning.
					return successor;
				else {
					// Make an intermediate block node and insert it between cn and
					// its designated successor.
					ControlGraph &cg = cn.controlGraph;
					ControlNode &intermediate = cg.newControlNode();
					intermediate.setControlBlock();
					intermediate.getNormalSuccessor().substituteTarget(successorEdge);
					intermediate.addPredecessor(successorEdge);
					return intermediate;
				}
			}
		case ckNone:
		case ckEnd:
		case ckThrow:
		case ckReturn:
			break; // These nodes have no normal successors.
	}
	trespass("Bad control node");
	return cn;
}


//
// Return a ControlNode appropriate for placing new DataNodes that should be
// executed immediately after all of this ControlNode's DataNodes if this
// ControlNode's execution leaves along the successor edge with the given number.
// Unlike obtainSuccessorSite, the returned ControlNode has no kind (although it may
// have existing phi nodes and primitives).  The caller should set that
// ControlNode's kind and set its normal successor to successor by calling
// successor->addPredecessor(..., *where) using the where value returned from
// this method.
//
// This method disengages successor's phi nodes; the caller should reengage them
// after calling addPredecessor as above.
//
// The returned ControlNode may be a new ControlNode spliced into the control
// graph between this ControlNode and its given successor, or it may be the
// given ControlNode if it can be legally used.
//
ControlNode &insertControlNodeAfter(ControlNode &cn, ControlNode *&successor, DoublyLinkedList<ControlEdge>::iterator &where,
									Uint32 successorNumber)
{
	assert(successorNumber < cn.nNormalSuccessors());
	ControlEdge &successorEdge = cn.nthSuccessor(successorNumber);
	successor = &successorEdge.getTarget();
	successor->disengagePhis();

	switch (cn.getControlKind()) {
		case ckBlock:
			where = cn.clearControlKindOne();
			return cn;	// We can add more DataNodes to the end of this node.
		case ckBegin:
		case ckIf:
		case ckSwitch:
		case ckExc:
		case ckAExc:
		case ckCatch:
			{
				// Make an intermediate block node and insert it between cn and
				// its designated successor.
				ControlNode &intermediate = cn.controlGraph.newControlNode();
				where = successorEdge.clearTarget();
				intermediate.addPredecessor(successorEdge);
				return intermediate;
			}
		case ckNone:
		case ckEnd:
		case ckThrow:
		case ckReturn:
			break; // These nodes have no normal successors.
	}
	trespass("Bad control node");
	return cn;
}


//
// Insert a new ControlNode immediately before ControlNode cn and return the new
// ControlNode.  The returned ControlNode has no kind (although it may
// have existing phi nodes, which are moved there from ControlNode cn).
// The caller should set that ControlNode's kind and optionally set its normal
// successor(s) to point to ControlNode cn.  The modified cn is guaranteed to
// have no phi nodes, so the caller can call addPredecessor on cn with impunity.
//
// Any incoming control edges pointing to cn are moved to the new control node.
// It is the caller's responsibility to ensure that that new control node can
// accept these edges (i.e., if cn is an aexc node and the new control node cannot
// accept backward edges, the control graph may become inconsistent).
//
ControlNode &insertControlNodeBefore(ControlNode &cn)
{
	ControlNode &newNode = cn.controlGraph.newControlNode();
	DoublyLinkedList<PhiNode> &phiNodes = cn.getPhiNodes();
	cn.disengagePhis();
	newNode.disengagePhis();

	// Move all predecessors from cn to the newNode.
	const DoublyLinkedList<ControlEdge> &predecessors = cn.getPredecessors();
	DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin();
	while (!predecessors.done(i)) {
		ControlEdge &e = predecessors.get(i);
		i = predecessors.advance(i);
		e.clearTarget();
		newNode.addPredecessor(e);
	}

	// Move all phi nodes from cn to the newNode.
	DoublyLinkedList<PhiNode>::iterator j = phiNodes.begin();
	while (!phiNodes.done(j)) {
		PhiNode &phi = phiNodes.get(j);
		j = phiNodes.advance(j);
		newNode.movePhiNode(phi);
	}

	newNode.reengagePhis();
	cn.reengagePhis();
	return newNode;
}


class InsertControlNodeBeforeIteratee: public Function2<bool, DataConsumer &, ControlEdge &>
{
	Function1<bool, ControlEdge &> &f;			// Test function passed into insertControlNodeBefore
	ControlNode &newNode;						// New control node being created by insertControlNodeBefore
	const Uint32 nTrueEdges;					// Number of true edges
	VariableOrConstant uniqueInput;				// If all inputs for which f is true seen so far are the same, the value of that input
	Uint32 nUniqueInputs;						// Number of times uniqueInput has been seen.
	PhiNode *builtPhi;							// Phi node being build if inputs for which f is true are not all the same

  public:
	InsertControlNodeBeforeIteratee(Function1<bool, ControlEdge &> &f, ControlNode &newNode, Uint32 nTrueEdges):
		f(f), newNode(newNode), nTrueEdges(nTrueEdges), nUniqueInputs(0), builtPhi(0) {}

	bool operator()(DataConsumer &input, ControlEdge &e);
	DataNode &getInput();
};


//
// Return true if this input to the phi node should be removed.
// As a side effect keep track of the removed inputs and build a new
// phi node for the new control node that insertControlNodeBefore is creating.
//
bool InsertControlNodeBeforeIteratee::operator()(DataConsumer &input, ControlEdge &e)
{
	if (!f(e))
		return false;

	Pool &pool = newNode.getPrimitivePool();
	if (!builtPhi) {
		if (nUniqueInputs == 0) {
			uniqueInput = input;
			nUniqueInputs = 1;
			return true;
		}
		if (uniqueInput == input) {
			nUniqueInputs++;
			return true;
		}
		assert(nUniqueInputs > 0 && nUniqueInputs < nTrueEdges);
		builtPhi = new(pool) PhiNode(nTrueEdges, input.getKind(), pool);
		while (nUniqueInputs--)
			builtPhi->addInput(uniqueInput.getVariable(), pool);
		newNode.addPhiNode(*builtPhi);
	}

	builtPhi->addInput(input.getVariable(), pool);
	return true;
}


//
// Return the phi node or the unique input encountered by calls to this iterator.
//
inline DataNode &InsertControlNodeBeforeIteratee::getInput()
{
	if (builtPhi)
		return *builtPhi;
	assert(nUniqueInputs);
	return uniqueInput.getVariable();
}


//
// Insert a new ControlNode immediately before some incoming edges into ControlNode cn
// and return the new ControlNode.  This function calls f on each edge coming into cn
// and only relocates the edges for which f returns true.  If f returns false for all
// edges coming into cn, this function does nothing and returns nil; if f returns true
// for at least one edge, this function creates and returns one new ControlNode shared
// among all the edges for which f returned true.  The returned ControlNode has no kind
// (although it may have existing phi nodes if cn had phi nodes).
//
// The caller should set that ControlNode's kind and set one of its successors to
// point to ControlNode cn by calling addPredecessor once on cn.  The caller should
// then call reengagePhis on cn (but not if this function returned nil).  This function
// may construct phi nodes inside cn that assume that the edge from the new ControlNode
// to cn will be the last edge, so it is important to call addPredecessor once on cn to
// add that edge before making other predecessor modifications on cn.
//
// Any incoming control edges pointing to cn for which f returned true are moved to the
// new control node.  It is the caller's responsibility to ensure that that new control
// node can accept these edges (i.e., if cn is an aexc node and the new control node
// cannot accept backward edges, the control graph may become inconsistent).
//
// Function f should have no side effects and the order in which it is called is not
// guaranteed.
//
ControlNode *insertControlNodeBefore(ControlNode &cn, Function1<bool, ControlEdge &> &f)
{
	const DoublyLinkedList<ControlEdge> &predecessors = cn.getPredecessors();
	for (DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin(); !predecessors.done(i); i = predecessors.advance(i))
		if (f(predecessors.get(i))) {
			// We have at least one edge for which f returned true.
			// Disengage cn's phis and create the new control node.
			ControlNode &newNode = cn.controlGraph.newControlNode();
			DoublyLinkedList<PhiNode> &phiNodes = cn.getPhiNodes();
			cn.disengagePhis();
			newNode.disengagePhis();

			// Count the edges for which f returned true.
			DoublyLinkedList<ControlEdge>::iterator k = i;
			Uint32 nTrueEdges = 0;
			do {
				if (f(predecessors.get(k)))
					nTrueEdges++;
				k = predecessors.advance(k);
			} while (!predecessors.done(k));

			// Split all phi nodes between cn and the newNode.
			DoublyLinkedList<PhiNode>::iterator j = phiNodes.begin();
			while (!phiNodes.done(j)) {
				PhiNode &phi = phiNodes.get(j);
				j = phiNodes.advance(j);
				InsertControlNodeBeforeIteratee iter(f, newNode, nTrueEdges);
				phi.removeSomeInputs(iter);
				phi.addInput(iter.getInput(), cn.getPrimitivePool());
			}

			// Move the predecessors for which f returns true from cn to the newNode.
			do {
				ControlEdge &e = predecessors.get(i);
				i = predecessors.advance(i);
				if (f(e)) {
					e.clearTarget();
					newNode.addPredecessor(e);
				}
			} while (!predecessors.done(i));

			newNode.reengagePhis();
			return &newNode;
		}
	return 0;
}
