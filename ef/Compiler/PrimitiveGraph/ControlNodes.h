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
#ifndef CONTROLNODES_H
#define CONTROLNODES_H

#include "Instruction.h"
#include "ClassWorld.h"
#include "Multiset.h"
#include "FastBitSet.h"
#include "CheckedUnion.h"

class ControlGraph;

class ControlEdge: public DoublyLinkedEntry<ControlEdge> // Links to other ControlEdges that go to the same target
{
	ControlNode *source;						// The node that owns this outgoing control edge  (in DEBUG versions nil if not explicitly set)
	ControlNode *target;						// The node to which control goes from this node  (in DEBUG versions nil if not explicitly set)
  public:
	bool longFlag;								// States whether the edge is a short or long edge (for ControlNodeScheduler).

  #ifdef DEBUG
	ControlEdge(): source(0), target(0) {}
  #endif

	ControlNode &getSource() const {assert(source); return *source;}
	ControlNode &getTarget() const {assert(target); return *target;}
	void setSource(ControlNode &n) {assert(!source); source = &n;}	// For use by ControlNode only.
	void setTarget(ControlNode &n) {assert(!target); target = &n;}	// For use by ControlNode only.
  	DoublyLinkedList<ControlEdge>::iterator clearTarget();
  	void substituteTarget(ControlEdge &src);
  #ifdef DEBUG
	bool hasTarget() const {return target != 0;} // Only works in DEBUG versions!
  #endif
};


enum ControlKind	// NormalIncoming?		  NormalOutgoing?		 OneNormalOutgoing?
{					//			ExceptionIncoming?		ExceptionOutgoing?
	ckNone,			//		no		   no			no		   no			no
	ckBegin,		//		no		   no			yes		   no			yes
	ckEnd,			//		no		   yes			no		   no			no
	ckBlock,		//		yes		   no			yes		   no			yes
	ckIf,			//		yes		   no			yes		   no			no
	ckSwitch,		//		yes		   no			yes		   no			no
	ckExc,			//		yes		   no			yes		   yes			yes
	ckThrow,		//		yes		   no			no		   yes			no
	ckAExc,			//		yes		   no			yes		   yes			yes
	ckCatch,		//		no		   yes			yes		   no			yes
	ckReturn		//		yes		   no			no		   no			no
};
const uint nControlKinds = ckReturn + 1;

struct ControlKindProperties
{
	bool normalInputs BOOL_8;					// Does this control node kind allow normal forward or backward incoming edges?
	bool exceptionInputs BOOL_8;				// Does this control node kind allow exception incoming edges?
	bool normalOutgoingEdges BOOL_8;			// Does this control node kind allow normal forward or backward outgoing edges?
	bool exceptionOutgoingEdges BOOL_8;			// Does this control node kind allow exception outgoing edges?
	bool oneNormalOutgoingEdge BOOL_8;			// Does this control node kind have exactly one forward or backward outgoing edge?
};

extern const ControlKindProperties controlKindProperties[nControlKinds];

inline bool hasNormalInputs(ControlKind ck) {return controlKindProperties[ck].normalInputs;}
inline bool hasExceptionInputs(ControlKind ck) {return controlKindProperties[ck].exceptionInputs;}
inline bool hasNormalOutgoingEdges(ControlKind ck) {return controlKindProperties[ck].normalOutgoingEdges;}
inline bool hasExceptionOutgoingEdges(ControlKind ck) {return controlKindProperties[ck].exceptionOutgoingEdges;}
inline bool hasOneNormalOutgoingEdge(ControlKind ck) {return controlKindProperties[ck].oneNormalOutgoingEdge;}
inline bool hasTryPrimitive(ControlKind ck) {return ck == ckExc || ck == ckThrow;}

#ifdef DEBUG_LOG
int print(LogModuleObject &f, ControlKind ck);
#endif


class ControlNode: public DoublyLinkedEntry<ControlNode> // Links to other ControlNodes in the same graph
{
  public:
	struct BeginExtra
	{
		PrimArg initialMemory;					// Initial memory value for this method
		const uint nArguments;					// Number of incoming arguments
	  private:
		PrimArg *const arguments;				// Array of nArguments incoming arguments
	  public:

		BeginExtra(ControlNode &controlNode, Pool &pool, uint nArguments,
				   const ValueKind *argumentKinds, bool hasSelfArgument,
				   Uint32 bci);

		PrimArg &operator[](uint n) const {assert(n < nArguments); return arguments[n];}
	};

	struct EndExtra
	{
		PrimResult finalMemory;					// Final memory value for this method
		EndExtra(Uint32 bci) : finalMemory(bci) {};
	};

	struct ExceptionExtra
	{
		Uint32 nHandlers;						// The last nHandlers successors go to exception handlers.  nHandlers is always
												//   greater than zero, and every exception must be handled (possibly just by
												//   jumping to the control End node if no other handler applies).
		const Class **handlerFilters;			// Array of nHandlers exception classes that are handled by the exception handlers
												//   guarding this control node.  These are in the same order as the last nHandlers
												//   successors.  An exception thrown inside this ControlNode will be passed to
	};											//   the first matching handler.

	struct TryExtra: ExceptionExtra
	{
		Primitive *tryPrimitive;				// The primitive in this control node that can cause exceptions

		explicit TryExtra(Primitive *tryPrimitive): tryPrimitive(tryPrimitive) {}
	};

	struct CatchExtra
	{
		PrimCatch cause;						// Exception that was thrown here
		CatchExtra(Uint32 bci) : cause(bci) {};
	};

	struct ReturnExtra
	{
		const uint nResults;					// Number of results
	  private:
		PrimResult *const results;				// Array of nResults variables containing the results
	  public:

		ReturnExtra(ControlNode &controlNode, Pool &pool, uint nResults,
					const ValueKind *resultKinds, Uint32 bci);
		PrimResult &operator[](uint n) const {assert(n < nResults); return results[n];}
	};


  private:
	DoublyLinkedList<ControlEdge> predecessors;	// List of possible predecessors of this control node
	ControlEdge *successorsBegin;				// Array of edges to successors (in DEBUG versions nil if not explicitly set)
	ControlEdge *successorsEnd;					//   Last element + 1			(in DEBUG versions nil if not explicitly set)
	DoublyLinkedList<PhiNode> phiNodes;			// List of phi nodes inside this control node
	DoublyLinkedList<Primitive> primitives;		// List of primitives inside this control node
	DoublyLinkedList<Instruction> instructions; // List of scheduled instructions inside this control node

	ControlKind controlKind ENUM_8;				// The kind of this control node (in DEBUG or DEBUG_LOG versions ckNone if not explicitly set)
	bool noRecycle BOOL_8;						// True if this node should not be recycled

  #ifdef DEBUG									// If zero, the phi nodes correspond to the predecessors list.  If nonzero,
	Uint32 phisDisengaged;						//   set to number of nested disengagePhis calls while working on the predecessor
  #endif										//   or phi lists.

	union {										// Kind-specific data for:
		BeginExtra *beginExtra;					//   ckBegin
		EndExtra *endExtra;						//   ckEnd
		PrimControl *controlPrimExtra;			//   ckIf, ckSwitch
		ReturnExtra *returnExtra;				//   ckReturn
		ExceptionExtra *exceptionExtra;			//   ckExc (TryExtra *), ckThrow (TryExtra *), ckAExc
		CatchExtra *catchExtra;					//   ckCatch
	};

  public:
	ControlGraph &controlGraph;					// Graph that contains this control node
	Uint32 generation;							// General-purpose variable used for marking nodes while traversing the graph
  private:
	static Uint32 nextGeneration;				// Generation value to be used for next graph traversal; each graph traversal
												// should increment this by one.
  public:
	enum {unmarked = -3, unvisited = -2, unnumbered = -1};
	Int32 dfsNum;								// Control number assigned by depth-first search
	Uint32 loopDepth;							// Loop nesting depth
	Uint32 loopId;								// Temp variable for loop search.

	// ********* Fields below will be moved to RegisterAllocationTemps
    Uint32 schedulePos;
	Flt32 nVisited;								// Approx. the number of time this node is visited (10^loopDepth)
	FastBitSet liveAtBegin;						// Virtual registers live at begin
	FastBitSet liveAtEnd;						// Virtual registers live at end
	bool hasNewLiveAtEnd;						// if true get the new interferences.
	bool liveAtBeginIsValid;					// if true liveAtBegin has been updated.
	// ********* Fields above will be moved to RegisterAllocationTemps

	// ********* Fields below will be moved to FormatterTemps
	bool workingNode; 							// True if this node is seen as a working node for the ControlNodeScheduler.
	Uint32 nativeOffset;
	// ********* Fields above will be moved to FormatterTemps

  private:
	// Temporary fields for matching monitorenters with monitorexits
	struct MonitorAnalysisTemps
	{
		SimpleMultiset<DataConsumer *> *monitorsHeld; // Multiset of objects whose monitors are currently held; nil if not known yet
	};

	// Temporary fields for register allocation
	struct RegisterAllocationTemps
	{
	    Uint32 schedulePos;
		Flt32 nVisited;							// Approx. the number of time this node is visited (10^loopDepth)
		FastBitSet liveAtBegin;					// Virtual registers live at begin
		FastBitSet liveAtEnd;					// Virtual registers live at end
		bool hasNewLiveAtEnd;					// if true get the new interferences.
		bool liveAtBeginIsValid;				// if true liveAtBegin has been updated.
	};

	// Temporary fields for scheduling and formatting code
	struct FormatterTemps
	{
		bool workingNode; 						// True if this node is seen as a working node for the ControlNodeScheduler.
		Uint32 nativeOffset;
	};

	CheckedUnion3(MonitorAnalysisTemps, RegisterAllocationTemps, FormatterTemps) temps;


	explicit ControlNode(ControlGraph &cg);	// Call ControlGraph::newControlNode instead to create a ControlNode
	void recycle();
  public:

	Pool &getPrimitivePool() const;

  #ifdef DEBUG
	bool getPhisDisengaged() const {return phisDisengaged || phiNodes.empty();}
	bool phisConsistent() const;
  #endif
	void disengagePhis() {assert(phisConsistent()); DEBUG_ONLY(phisDisengaged++);}
	void reengagePhis() {assert(phisDisengaged); DEBUG_ONLY(phisDisengaged--); assert(phisConsistent());}

	const DoublyLinkedList<ControlEdge> &getPredecessors() const {return predecessors;}
	void addPredecessor(ControlEdge &e) {assert(phisDisengaged || phiNodes.empty()); e.setTarget(*this); predecessors.addLast(e);}
	void addPredecessor(ControlEdge &e, DoublyLinkedList<ControlEdge>::iterator where);
	void movePredecessors(DoublyLinkedList<ControlEdge> &src);

	ControlEdge *getSuccessorsBegin() const {assert(successorsBegin); return successorsBegin;}
	ControlEdge *getSuccessorsEnd() const {assert(successorsEnd); return successorsEnd;}
	Uint32 nSuccessors() const {assert(successorsBegin && successorsEnd); return successorsEnd - successorsBegin;}
	Uint32 nNormalSuccessors() const;
	ControlEdge &nthSuccessor(Uint32 n) const {assert(n < nSuccessors()); return successorsBegin[n];}
	ControlEdge &getNormalSuccessor() const;
	ControlEdge &getFalseSuccessor() const;
	ControlEdge &getTrueSuccessor() const;
	ControlEdge *getSwitchSuccessors() const;
	ControlEdge &getReturnSuccessor() const;
	void setSuccessors(ControlEdge *newSuccessorsBegin, ControlEdge *newSuccessorsEnd);
  private:
	void setOneSuccessor();
	void setNSuccessors(Uint32 n);
	void setNSuccessors(Uint32 n, ControlEdge *successors);
  public:

	DoublyLinkedList<PhiNode> &getPhiNodes() {assert(!phisDisengaged); return phiNodes;}
	void addPhiNode(PhiNode &p) {assert(phisDisengaged || p.nInputs() == predecessors.length()); p.setContainer(this); phiNodes.addLast(p);}
	void movePhiNode(PhiNode &p);

	DoublyLinkedList<Primitive> &getPrimitives() {return primitives;}
	void appendPrimitive(Primitive &p) {p.setContainer(this); primitives.addLast(p);}
	void prependPrimitive(Primitive &p) {p.setContainer(this); primitives.addFirst(p);}
	void movePrimitiveToBack(Primitive &p);
	void movePrimitiveToFront(Primitive &p);

	bool hasControlKind(ControlKind ck) const {assert(controlKind != ckNone); return controlKind == ck;}
	ControlKind getControlKind() const {assert(controlKind != ckNone); return controlKind;}
	void unsetControlKind();
	void clearControlKind();
	DoublyLinkedList<ControlEdge>::iterator clearControlKindOne();
	void setControlBegin(uint nArguments, const ValueKind *argumentKinds,
						 bool hasSelfArgument, Uint32 bci);
	void setControlEnd(Uint32 bci);
	void setControlBlock();
	void setControlIf(Condition2 conditional, DataNode &condition, Uint32 bci);
	void setControlSwitch(DataNode &selector, Uint32 nCases, Uint32 bci);
	void setControlException(ControlKind ck, Uint32 nHandlers, const Class **handlerFilters, Primitive *tryPrimitive,
		 					 ControlEdge *successors);
	PrimCatch &setControlCatch(Uint32 bci);
	void setControlReturn(uint nResults, const ValueKind *resultKinds, 
						  Uint32 bci);

	BeginExtra &getBeginExtra() const {assert(hasControlKind(ckBegin)); return *beginExtra;}
	EndExtra &getEndExtra() const {assert(hasControlKind(ckEnd)); return *endExtra;}
	PrimControl &getControlPrimExtra() const {assert(hasControlKind(ckIf) || hasControlKind(ckSwitch)); return *controlPrimExtra;}
	ExceptionExtra &getExceptionExtra() const {assert(hasExceptionOutgoingEdges(getControlKind())); return *exceptionExtra;}
	TryExtra &getTryExtra() const {assert(hasTryPrimitive(getControlKind())); return *static_cast<TryExtra *>(exceptionExtra);}
	CatchExtra &getCatchExtra() const {assert(hasControlKind(ckCatch)); return *catchExtra;}
	ReturnExtra &getReturnExtra() const {assert(hasControlKind(ckReturn)); return *returnExtra;}

	bool empty() const {assert(!phisDisengaged); return !noRecycle && primitives.empty() && phiNodes.empty();}
	void inhibitRecycling() {noRecycle = true;}

	InstructionList &getInstructions() {return instructions;}
	void addScheduledInstruction(Instruction& i) {instructions.addLast(i);}
	bool haveScheduledInstruction(Instruction& i) {return instructions.exists(i);}
	void setNativeOffset(Uint32 offset) { nativeOffset = offset; }
	Uint32 getNativeOffset() { return nativeOffset; }
	static Uint32 getNextGeneration() {return nextGeneration++;}

  #ifdef DEBUG
	void printScheduledInstructions(LogModuleObject &f);
    void validate(FastBitSet **reachingSet);                // perform consistency checks on self
 #endif
  #ifdef DEBUG_LOG
	bool hasControlKind() const {return controlKind != ckNone;} // Only use for debug logging code! Doesn't work in release builds.
	int printRef(LogModuleObject &f) const;
	void printPretty(LogModuleObject &f, int margin) const;
  #endif

	friend class ControlGraph;	// ControlGraph can call the ControlNode constructor and recycle
};


// --- INLINES ----------------------------------------------------------------


//
// Unlink this edge from the target to which it is linked.
// Return a location suitable for relinking another edge to the target
// in place of this edge without disturbing the order of the target's
// predecessors (changing that order would disrupt the target's phi nodes).
//
inline DoublyLinkedList<ControlEdge>::iterator ControlEdge::clearTarget()
{
	assert(target && target->getPhisDisengaged());
  #ifdef DEBUG
	target = 0;
  #endif
	DoublyLinkedList<ControlEdge>::iterator where = prev;
	remove();
	return where;
}


//
// Unlink src from the target to which it is linked and link this edge to that target
// in src's place.  This edge must not currently have a target.
//
// This is essentially equivalent to, but faster than:
//
//   ControlNode &target = src.getTarget();
//   target.disengagePhis();
//   DoublyLinkedList<ControlEdge>::iterator where = src.clearTarget();
//   target.addPredecessor(*this, where);
//   target.reengagePhis();
//
inline void ControlEdge::substituteTarget(ControlEdge &src)
{
	assert(!target && src.target);
	substitute(src);
	target = src.target;
  #ifdef DEBUG
	src.target = 0;
  #endif
}


//
// Bring the ControlNode back to the state it's in immediately after it's constructed.
// The given ControlNode must satisfy the empty() method and have no predecessors,
// instructions, control kind, or generation.
//
inline void ControlNode::recycle()
{
	assert(!noRecycle && controlKind == ckNone && predecessors.empty() && empty() && instructions.empty() && generation == 0);
}


//
// Return the normal outgoing edge of this node.  This node must have exactly one
// normal outgoing edge.
//
inline ControlEdge &ControlNode::getNormalSuccessor() const
{
	assert(hasOneNormalOutgoingEdge(getControlKind()) && successorsBegin);
	return successorsBegin[0];
}


//
// Return the false outgoing edge of this if node.
//
inline ControlEdge &ControlNode::getFalseSuccessor() const
{
	assert(hasControlKind(ckIf) && successorsBegin);
	return successorsBegin[0];
}


//
// Return the true outgoing edge of this if node.
//
inline ControlEdge &ControlNode::getTrueSuccessor() const
{
	assert(hasControlKind(ckIf) && successorsBegin);
	return successorsBegin[1];
}


//
// Return the array of nSuccessors() outgoing edges of this switch node.
//
inline ControlEdge *ControlNode::getSwitchSuccessors() const
{
	assert(hasControlKind(ckSwitch) && successorsBegin);
	return successorsBegin;
}


//
// Return the outgoing edge of this return node.
//
inline ControlEdge &ControlNode::getReturnSuccessor() const
{
	assert(hasControlKind(ckReturn) && successorsBegin);
	return successorsBegin[0];
}


//
// Add a new predecessor to this control node in the specified location.
// The node will be added to this control node's list of predecessors after the
// where predecessor, which should be the result of a clearTarget or
// clearControlKindOne call on this node.  The where predecessor can be this
// node's root, in which case the new control node will become the first predecessor.
//
inline void ControlNode::addPredecessor(ControlEdge &e, DoublyLinkedList<ControlEdge>::iterator where)
{
	assert(phisDisengaged || phiNodes.empty());
	e.setTarget(*this);
	predecessors.insertAfter(e, where);
}


//
// Designate the successors array for this control node.  This can be done only
// once afer the control node is created and once after it each time it is
// reset with clearControlKind or one of its cousins.
//
inline void ControlNode::setSuccessors(ControlEdge *newSuccessorsBegin, ControlEdge *newSuccessorsEnd)
{
	assert(!successorsBegin && !successorsEnd && newSuccessorsBegin && newSuccessorsEnd &&
		   newSuccessorsEnd >= newSuccessorsBegin);
	successorsBegin = newSuccessorsBegin;
	successorsEnd = newSuccessorsEnd;
}


//
// Move the phi node so that it becomes the last phi node in this control node.
// The phi node must have been part of some control node (not necessarily this one).
//
inline void ControlNode::movePhiNode(PhiNode &p)
{
	assert(phisDisengaged || p.nInputs() == predecessors.length());
	p.changeContainer(this);
	phiNodes.addLast(p);
}


//
// Move the primitive so that it becomes the last primitive in this control node.
// The primitive must have been part of some control node (not necessarily this one).
//
inline void ControlNode::movePrimitiveToBack(Primitive &p)
{
	p.changeContainer(this);
	primitives.addLast(p);
}


//
// Move the primitive so that it becomes the first primitive in this control node.
// The primitive must have been part of some control node (not necessarily this one).
//
inline void ControlNode::movePrimitiveToFront(Primitive &p)
{
	p.changeContainer(this);
	primitives.addFirst(p);
}


//
// Unset the control node's kind so that the node can get a new kind.
// The primitives and phi nodes remain attached to this control node, but
// the control kind-specific data disappears.
// Unlike clearControlKind, the successor ControlEdges' targets must not
// have been initialized prior to calling this method.
//
inline void ControlNode::unsetControlKind()
{
  #if defined(DEBUG) || defined(DEBUG_LOG)
	controlKind = ckNone;
  #endif
  #ifdef DEBUG
	for (ControlEdge *e = successorsBegin; e != successorsEnd; e++)
		assert(!e->hasTarget());
	successorsBegin = 0;
	successorsEnd = 0;
  #endif
}

#endif
