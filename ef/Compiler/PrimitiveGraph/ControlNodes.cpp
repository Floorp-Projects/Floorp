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
#include "DebugUtils.h"


const ControlKindProperties controlKindProperties[nControlKinds] =
{//  Incoming---   Outgoing----------
 //  Norml? Exc?   Norml? Exc?   One?
	{false, false, false, false, false},	// ckNone
	{false, false, true,  false, true},		// ckBegin
	{false, true,  false, false, false},	// ckEnd
	{true,  false, true,  false, true},		// ckBlock
	{true,  false, true,  false, false},	// ckIf
	{true,  false, true,  false, false},	// ckSwitch
	{true,  false, true,  true,  true},		// ckExc
	{true,  false, false, true,  false},	// ckThrow
	{true,  false, true,  true,  true},		// ckAExc
	{false, true,  true,  false, true},		// ckCatch
	{true,  false, false, false, false}		// ckReturn
};


#ifdef DEBUG_LOG
const char controlKindNames[][8] =
{
	"????",		// ckNone
	"Begin",	// ckBegin
	"End",		// ckEnd
	"Block",	// ckBlock
	"If",		// ckIf
	"Switch",	// ckSwitch
	"Exc",		// ckExc
	"Throw",	// ckThrow
	"AExc",		// ckAExc
	"Catch",	// ckCatch
	"Return"	// ckReturn
};


//
// Print the name of the ControlKind onto the output stream f.
// Return the number of characters printed.
//
int print(LogModuleObject &f, ControlKind ck)
{
	return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ((uint)ck > ckReturn ? "????" : controlKindNames[ck]));
}
#endif


// ----------------------------------------------------------------------------
// ControlNode::BeginExtra

//
// Initialize a new BeginExtra record with nArguments incoming arguments whose
// kinds are given by argumentKinds.  If the method isn't static, the this
// parameter must be listed as an incoming argument in the argumentKinds array
// as well and hasSelfArgument must be set to true; otherwise hasSelfArgument
// should be false.
// Allocate additional storage for the arguments array from the given pool.
//
ControlNode::BeginExtra::BeginExtra(ControlNode &controlNode, Pool &pool,
									uint nArguments, 
									const ValueKind *argumentKinds, 
									bool hasSelfArgument, Uint32 bci)
	: nArguments(nArguments),
	  arguments(nArguments ? new(pool) PrimArg[nArguments] : 0)
{
	initialMemory.initOutgoingEdge(vkMemory, true);
	controlNode.appendPrimitive(initialMemory);
	PrimArg *a = arguments;
	bool isThis = hasSelfArgument;
	for (uint i = 0; i != nArguments; i++) {
		// Set the bytecode index. This can't be done in the constructor
		// because initializers can't be specified for an array of objects
		// allocated through the new operator
		a->setBytecodeIndex(bci);
		a->initOutgoingEdge(*argumentKinds++, isThis);
		isThis = false;
		controlNode.appendPrimitive(*a);
		a++;
	}
}


// ----------------------------------------------------------------------------
// ControlNode::ReturnExtra

//
// Initialize a new ReturnExtra record with nResults results whose kinds are
// given in the resultKinds array.
// Allocate additional storage for the results array from the given pool.
//
ControlNode::ReturnExtra::ReturnExtra(ControlNode &controlNode, Pool &pool,
									  uint nResults, 
									  const ValueKind *resultKinds, Uint32 bci)
	: nResults(nResults), results((PrimResult * const)pool.allocate(nResults * sizeof(PrimResult)))
{
    uint i;
	PrimResult *r = results;
	for (i = 0; i != nResults; i++) {
        new (r) PrimResult(bci);
		r->setResultKind(*resultKinds++);
		controlNode.appendPrimitive(*r++);
	}
}


// ----------------------------------------------------------------------------
// ControlNode

Uint32 ControlNode::nextGeneration = 1;


#ifdef DEBUG
//
// Return true if each phi node in this ControlNode has the same number of
// inputs as this ControlNode has predecessors.
// If phisDisengaged is nonzero, always return true.
//
bool ControlNode::phisConsistent() const
{
	if (phisDisengaged)
		return true;
	Uint32 nPredecessors = predecessors.length();
	for (DoublyLinkedList<PhiNode>::iterator i = phiNodes.begin(); !phiNodes.done(i); i = phiNodes.advance(i))
		if (phiNodes.get(i).nInputs() != nPredecessors)
			return false;
	return true;
}
#endif


//
// Return the number of successor edges that are not exception or return edges.
// Multiple edges to the same node are counted multiple times.
//
Uint32 ControlNode::nNormalSuccessors() const
{
	switch (controlKind) {
		case ckEnd:
		case ckThrow:
		case ckReturn:
			return 0;
		case ckBegin:
		case ckBlock:
		case ckExc:
		case ckAExc:
		case ckCatch:
			return 1;
		case ckIf:
			return 2;
		case ckSwitch:
			return nSuccessors();
		case ckNone:;
	}
	trespass("Bad control node");
	return 0;
}


//
// Allocate exactly one successor for this ControlNode.
//
void ControlNode::setOneSuccessor()
{
	ControlEdge *successor = new(getPrimitivePool()) ControlEdge;
	successor->setSource(*this);
	setSuccessors(successor, successor + 1);
}


//
// Allocate n successors for this ControlNode.
//
void ControlNode::setNSuccessors(Uint32 n)
{
	ControlEdge *successors = new(getPrimitivePool()) ControlEdge[n];
	ControlEdge *successors2 = successors;
	while (n--)
		successors2++->setSource(*this);
	setSuccessors(successors, successors2);
}


//
// Use n preallocated successors for this ControlNode.
// Make the successors's sources point to this ControlNode.
//
inline void ControlNode::setNSuccessors(Uint32 n, ControlEdge *successors)
{
	setSuccessors(successors, successors + n);
	while (n--)
		successors++->setSource(*this);
}


//
// Unset the control node's kind so that the node can get a new kind.
// The primitives and phi nodes remain attached to this control node, but
// the control kind-specific data disappears.
// The successor ControlEdges' targets must have been initialized prior
// to calling this method; this method clears them.
//
void ControlNode::clearControlKind()
{
  #if defined(DEBUG) || defined(DEBUG_LOG)
	controlKind = ckNone;
  #endif
	for (ControlEdge *e = successorsBegin; e != successorsEnd; e++)
		e->clearTarget();
  #ifdef DEBUG
	successorsBegin = 0;
	successorsEnd = 0;
  #endif
}


//
// Unset the control node's kind so that the node can get a new kind.
// The primitives and phi nodes remain attached to this control node, but
// the control kind-specific data disappears.
// The successor ControlEdges' targets must have been initialized prior
// to calling this method; this method clears them.
//
// Return a location suitable for relinking another edge to the control
// node's normal outgoing edge's target in place of the existing normal
// outgoing edge without disturbing the order of that target's
// predecessors (changing that order would disrupt the target's phi nodes).
// This control node must have exactly one normal outgoing edge.
//
DoublyLinkedList<ControlEdge>::iterator ControlNode::clearControlKindOne()
{
	assert(hasOneNormalOutgoingEdge(getControlKind()) && successorsBegin);
  #if defined(DEBUG) || defined(DEBUG_LOG)
	controlKind = ckNone;
  #endif
	ControlEdge *e = successorsBegin;
	DoublyLinkedList<ControlEdge>::iterator where = e->clearTarget();
	while (++e != successorsEnd)
		e->clearTarget();
  #ifdef DEBUG
	successorsBegin = 0;
	successorsEnd = 0;
  #endif
	return where;
}


//
// Designate the ControlNode to be a Begin node with nArguments incoming arguments
// whose kinds are given by argumentKinds.  If the method isn't static, the this
// parameter must be listed as an incoming argument in the argumentKinds array
// as well and hasSelfArgument must be set to true; otherwise hasSelfArgument
// should be false.
// Afterwards the caller must initialize the successor ControlEdge's target.
//
void ControlNode::setControlBegin(uint nArguments, 
								  const ValueKind *argumentKinds, 
								  bool hasSelfArgument, Uint32 bci)
{
	assert(controlKind == ckNone);

	setOneSuccessor();
	Pool &pool = getPrimitivePool();
	beginExtra = new(pool) BeginExtra(*this, pool, nArguments, argumentKinds,
									  hasSelfArgument, bci);
	controlKind = ckBegin;
}


//
// Designate the ControlNode to be an End node.
// The caller must subsequently initialize the finalMemory's incoming edge.
//
void ControlNode::setControlEnd(Uint32 bci)
{
	assert(controlKind == ckNone);

	ControlEdge e;
	setSuccessors(&e, &e);	// No successors.
	endExtra = new(getPrimitivePool()) EndExtra(bci);
	appendPrimitive(endExtra->finalMemory);
	controlKind = ckEnd;
}


//
// Designate the ControlNode to be a Block node.
// Afterwards the caller must initialize the successor ControlEdge's target.
//
void ControlNode::setControlBlock()
{
	assert(controlKind == ckNone);

	setOneSuccessor();
	controlKind = ckBlock;
}

//
// Designate the ControlNode to be an If node that tests the given
// conditional.  condition must be a DataNode that generates a condition
// value.  Afterwards the caller must initialize the two successor
// ControlEdges' targets.
//
void 
ControlNode::setControlIf(Condition2 conditional, DataNode &condition,
						  Uint32 bci)
{
	assert(controlKind == ckNone && condition.hasKind(vkCond));

	setNSuccessors(2);
	PrimControl *pc = new(getPrimitivePool()) 
		PrimControl(condition2ToIfCond(conditional), bci);
	pc->getInput().setVariable(condition);
	appendPrimitive(*pc);
	controlPrimExtra = pc;
	controlKind = ckIf;
}


//
// Designate the ControlNode to be a Switch node with nCases cases.  selector
// must be a DataNode that generates an int value.  Afterwards the caller
// must initialize all the successor ControlEdges' targets.
//
void 
ControlNode::setControlSwitch(DataNode &selector, Uint32 nCases, Uint32 bci) 
{
	assert(controlKind == ckNone && selector.hasKind(vkInt) && nCases > 0);

	setNSuccessors(nCases);
	PrimControl *pc = new(getPrimitivePool()) PrimControl(poSwitch, bci);
	pc->getInput().setVariable(selector);
	appendPrimitive(*pc);
	controlPrimExtra = pc;
	controlKind = ckSwitch;
}


//
// Designate the ControlNode to be an Exc, Throw, or AExc node, depending on
// the given ControlKind.  The node will have nHandlers exception handlers
// with that many filter classes listed in the handlerFilters array.  The
// exception classes are considered in order when dispatching an exception,
// so their order is significant.  If the ControlNode will be an Exc or Throw
// node, tryPrimitive must be a primitive located inside this node that can
// throw an exception; if the ControlNode will be an AExc node, tryPrimitive
// must be nil.  successors must be a preallocated array of nHandlers or
// nHandlers+1 successor edges, including the edge referring to the normal
// successor for Exc and AExc nodes.
//
// The caller must initialize all the successor ControlEdges' targets either
// before or after calling this method.
//
void 
ControlNode::setControlException(ControlKind ck, Uint32 nHandlers, 
								 const Class **handlerFilters, 
								 Primitive *tryPrimitive, 
								 ControlEdge *successors)
{
	assert(controlKind == ckNone &&
		   hasExceptionOutgoingEdges(ck) && nHandlers > 0 &&
		   (tryPrimitive == 0) == (ck == ckAExc));

	bool hasNormalExit = ck != ckThrow;
	setNSuccessors(hasNormalExit + nHandlers, successors);

	ExceptionExtra *ee;
	if (tryPrimitive)
		ee = new(getPrimitivePool()) TryExtra(tryPrimitive);
	else
		ee = new(getPrimitivePool()) ExceptionExtra;
	ee->nHandlers = nHandlers;
	ee->handlerFilters = handlerFilters;
	exceptionExtra = ee;
	controlKind = ck;
}


//
// Designate the ControlNode to be a Catch node.  Return the node's PrimCatch
// primitive.  Afterwards the caller must initialize the successor
// ControlEdge's target.
//
PrimCatch &
ControlNode::setControlCatch(Uint32 bci)
{
	assert(controlKind == ckNone);

	setOneSuccessor();
	catchExtra = new(getPrimitivePool()) CatchExtra(bci);
	PrimCatch &cause = catchExtra->cause;
	prependPrimitive(cause);
	controlKind = ckCatch;
	return cause;
}


//
// Designate the ControlNode to be a Return node with nResults results whose
// kinds are given in the resultKinds array.  Afterwards the caller must
// initialize the successor ControlEdge's target to point to the End node and
// call getReturnExtra()[...].getInput() = ... for each result
//
void ControlNode::setControlReturn(uint nResults, 
								   const ValueKind *resultKinds, Uint32 bci)
{
	assert(controlKind == ckNone);

	setOneSuccessor();
	Pool &pool = getPrimitivePool();
	returnExtra = new(pool) ReturnExtra(*this, pool, nResults, resultKinds,
										bci);
	controlKind = ckReturn;
	controlGraph.setReturnNode(*this);
}


//
// Move the predecessors from the given list to this ControlNode.
// This ControlNode must have no current predecessors.
// On return, the src list will be made empty because the ControlEdges can
// be linked into only one DoublyLinkedList at a time.
//
void ControlNode::movePredecessors(DoublyLinkedList<ControlEdge> &src)
{
	assert(phisDisengaged || phiNodes.empty());
	predecessors.move(src);
	for (DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin();
		 !predecessors.done(i);
		 i = predecessors.advance(i))
		predecessors.get(i).setTarget(*this);
}



#ifdef DEBUG
//
// Temporary routine to print out the list of instructions which are valid after scheduling.
//
void ControlNode::printScheduledInstructions(LogModuleObject &f)
{
	for(InstructionList::iterator i = instructions.begin(); !instructions.done(i); i = instructions.advance(i))
	{
		instructions.get(i).getPrimitive()->printPretty(f, 2);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\t\t"));
		instructions.get(i).printDebug(f);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
	}
}
#endif


#ifdef DEBUG_LOG
//
// Print a reference to this ControlNode for debugging purposes.
// Return the number of characters printed.
//
int ControlNode::printRef(LogModuleObject &f) const
{
	if (controlGraph.dfsListIsValid() && dfsNum >= 0)
		return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("N%d", dfsNum));
	else
		return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("N%p", this));
}


//
// Print a ControlNode for debugging purposes.
// f should be at the beginning of a line.
//
void ControlNode::printPretty(LogModuleObject &f, int margin) const
{
	printMargin(f, margin);
	if (controlKind != ckNone) {
		print(f, controlKind);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" "));
	}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Node "));
	printRef(f);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" (%p):\n", this));

	printMargin(f, margin);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Predecessors: "));
	bool printSeparator = false;
	for (DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin(); !predecessors.done(i); i = predecessors.advance(i)) {
		ControlEdge &e = predecessors.get(i);
		if (printSeparator)
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
		printSeparator = true;
		assert(&e.getTarget() == this);
		e.getSource().printRef(f);
	}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));

	if (!phiNodes.empty()) {
		printMargin(f, margin);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Phi nodes:\n"));
		for (DoublyLinkedList<PhiNode>::iterator i = phiNodes.begin(); !phiNodes.done(i); i = phiNodes.advance(i)) {
			PhiNode &n = phiNodes.get(i);
			assert(n.getContainer() == this);
			n.printPretty(f, margin + 4);
		}
	}

	if (!primitives.empty()) {
		printMargin(f, margin);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Primitives:\n"));
		for (DoublyLinkedList<Primitive>::iterator i = primitives.begin(); !primitives.done(i); i = primitives.advance(i)) {
			Primitive &n = primitives.get(i);
			assert(n.getContainer() == this);
			n.printPretty(f, margin + 4);
		}
	}

	if (successorsBegin && successorsEnd) {
		ControlEdge *e = successorsBegin;
		ControlEdge *eEnd = e + nNormalSuccessors() + hasControlKind(ckReturn);
		if (e != eEnd) {
			printMargin(f, margin);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Successors: "));
			bool printSeparator = false;
			while (e != eEnd) {
				if (printSeparator)
					UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
				printSeparator = true;
				assert(&e->getSource() == this);
				e->getTarget().printRef(f);
				e++;
			}
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
		}
		if (e != successorsEnd) {
			ExceptionExtra *ee = &getExceptionExtra();
			assert((Uint32)(successorsEnd - e) == ee->nHandlers);
			printMargin(f, margin);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Exception handlers: "));
			bool printSeparator = false;
			const Class **exceptionClass = ee->handlerFilters;
			while (e != successorsEnd) {
				if (printSeparator)
					printMargin(f, margin + 20);
				printSeparator = true;
				assert(&e->getSource() == this);
				(*exceptionClass++)->printRef(f);
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" -> "));
				e->getTarget().printRef(f);
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
				e++;
			}
		}
	}

	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
}
#endif
