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
#include "Primitives.h"
#include "ControlNodes.h"
#include "SysCalls.h"
#include "FieldOrMethod.h"
#include "DebugUtils.h"

#include "PrimitiveOperations.cpp"


const DataNode VariableOrConstant::constSources[nValueKinds] =
{
    vkVoid,
    vkInt,
    vkLong,
	vkFloat,
	vkDouble,
	vkAddr,
	vkCond,
	vkMemory,
    vkTuple
};

// ----------------------------------------------------------------------------
// VariableOrConstant


//
// Return true if the given VariableOrConstant is equal to this VariableOrConstant.
// Two VariableOrConstants are equal if either they are equal constants or
// the same Producer.
//
bool VariableOrConstant::operator==(const VariableOrConstant &poc2) const
{
	return source == poc2.source && (isVariable() || value.eq(getKind(), poc2.value));
}


//
// Return true if the VariableOrConstant is guaranteed to be nonzero.
//
bool VariableOrConstant::isAlwaysNonzero() const
{
	return isConstant() ? value.isNonzero(source->getKind()) : getVariable().isAlwaysNonzero();
}


// ----------------------------------------------------------------------------
// DataConsumer

//
// Set this DataConsumer to the variable or constant contained inside p.
// This DataConsumer should not have been previously initialized.
//
void DataConsumer::operator=(const VariableOrConstant &p)
{
	assert(!(source && source->isVariable()));
	if (p.isConstant())
		setConstant(p.getKind(), p.getConstant());
	else
		setVariable(p.getVariable());
}


//
// Destructively move the src DataConsumer (which must have been initialized)
// to this DataConsumer, which must be uninitialized.  The src DataConsumer is
// unlinked from any linked lists to which it belonged and left uninitialized.
//
void DataConsumer::move(DataConsumer &src)
{
	assert(!(source && source->isVariable()));
	source = src.source;
	node = src.node;
  #ifdef DEBUG
	src.source = 0;
	src.node = 0;
	if (isVariable())
		links.init();
  #endif
	if (isConstant())
		value = src.value;
	else
		links.substitute(src.links);
}


#ifdef DEBUG_LOG
//
// Print a reference to this DataConsumer for debugging purposes.
// Return the number of characters printed.
//
int DataConsumer::printRef(LogModuleObject &f) const
{
	if (isConstant())
		return getConstant().print(f, getKind());
	else
		return getVariable().printRef(f);
}
#endif


// ----------------------------------------------------------------------------
// DataNode


#ifdef DEBUG_LOG
//
// Print a reference to this DataNode for debugging purposes.
// Return the number of characters printed.
//
int DataNode::printRef(LogModuleObject &f) const
{
	int a = UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%c%c", constant ? 'c' : isAlwaysNonzero() ? 'w' : 'v', valueKindShortName(kind)));
	if (producerNumber != (Uint32)-1)
		return a + UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%u", producerNumber));
	else
		return a + UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%p", this));
}
#endif


// ----------------------------------------------------------------------------


Uint16 gCurLineNumber = 0;

const PrimitiveFormat categoryFormats[nPrimitiveCategories] =
{
	pfNone,					// pcNone
	pfPhi,					// pcPhi
	pfConst,				// pcConst
	pfDebug,				// pcDebug
	pfControl,				// pcIfCond
	pfControl,				// pcSwitch
	pfCatch,				// pcCatch
	pfProj,					// pcProj
	pfArg,					// pcArg
	pfResult,				// pcResult
	pfBinaryGeneric,		// pcGeneric
	pfBinaryGeneric,		// pcDivMod
	pfBinaryGeneric,		// pcShift
	pfBinaryGeneric,		// pcExt
	pfBinaryGeneric,		// pcFGeneric
	pfUnaryGeneric,			// pcConv
	pfUnaryGeneric,			// pcFConv
	pfBinaryGeneric,		// pcCmp
	pfBinaryGeneric,		// pcFCmp
	pfUnaryGeneric,			// pcCond
	pfUnaryGeneric,			// pcCond3
	pfUnaryGeneric,			// pcCheck1
	pfBinaryGeneric,		// pcCheck2
	pfLd,					// pcLd
	pfSt,					// pcSt
	pfMonitor,				// pcMonitor
	pfSync,					// pcSync
	pfSysCall,				// pcSysCall
	pfCall					// pcCall
};


#ifdef DEBUG_LOG
static const char primitiveFormatNames[nPrimitiveFormats][16] = 
{
	"None",				// pfNone
	"Phi",				// pfPhi
	"Const",			// pfConst
	"Debug",			// pfDebug
	"Control",			// pfControl
	"Catch",			// pfCatch
	"Proj",				// pfProj
	"Arg",				// pfArg
	"Result",			// pfResult
	"UnaryGeneric",		// pfUnaryGeneric
	"BinaryGeneric",	// pfBinaryGeneric
	"Ld",				// pfLd
	"St",				// pfSt
	"Monitor",			// pfMonitor
	"Sync",				// pfSync
	"SysCall",			// pfSysCall
	"Call"				// pfCall
};


static const char primitiveCategoryNames[nPrimitiveCategories][16] = 
{
	"None",			// pcNone
	"Phi",			// pcPhi
	"Const",		// pcConst
	"Debug",		// pcDebug
	"IfCond",		// pcIfCond
	"Switch",		// pcSwitch
	"Catch",		// pcCatch
	"Proj",			// pcProj
	"Arg",			// pcArg
	"Result",		// pcResult
	"Generic",		// pcGeneric
	"DivMod",		// pcDivMod
	"Shift",		// pcShift
	"Ext",			// pcExt
	"FGeneric",		// pcFGeneric
	"Conv",			// pcConv
	"FConv",		// pcFConv
	"Cmp",			// pcCmp
	"FCmp",			// pcFCmp
	"Cond",			// pcCond
	"Cond3",		// pcCond3
	"Check1",		// pcCheck1
	"Check2",		// pcCheck2
	"Ld",			// pcLd
	"St",			// pcSt
	"Monitor",		// pcMonitor
	"Sync",			// pcSync
	"SysCall",		// pcSysCall
	"Call"			// pcCall
};


//
// Print the name of the PrimitiveFormat onto the output stream f.
// Return the number of characters printed.
//
int print(LogModuleObject &f, PrimitiveFormat pf)
{
	return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ((uint)pf >= nPrimitiveFormats ? "????" : primitiveFormatNames[pf]));
}


//
// Print the name of the PrimitiveCategory onto the output stream f.
// Return the number of characters printed.
//
int print(LogModuleObject &f, PrimitiveCategory pc)
{
	return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ((uint)pc >= nPrimitiveCategories ? "????" : primitiveCategoryNames[pc]));
}


//
// Print the name of the PrimitiveOperation onto the output stream f.
// Return the number of characters printed.
//
int print(LogModuleObject &f, PrimitiveOperation pk)
{
	return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ((uint)pk >= nPrimitiveOperations ? "????" : primitiveOperationNames[pk]));
}
#endif


//
// If the comparison always returns a constant, return nil and store the (true or
// false) result of the comparison in result.  Otherwise, return the DataNode
// obtained from c.
//
DataNode *partialComputeComparison(Condition2 cond2, VariableOrConstant c, bool &result)
{
	assert(c.hasKind(vkCond));

	if (c.isConstant()) {
		result = applyCondition(cond2, c.getConstant().c);
		return 0;
	}

	// Optimize the cases in which we know that cond cannot be cEq.
	DataNode &cp = c.getVariable();
	if (cp.isAlwaysNonzero())
		switch (cond2) {
			case condEq:
				result = false;
				return 0;
			case condNe:
				result = true;
				return 0;
			default:;
		}
	return &cp;
}


// ----------------------------------------------------------------------------
// DataNode


//
// Destroy this DataNode, which must have been initialized.  This DataNode's
// consumers are destroyed as well.
// Don't call this directly; call PhiNode::destroy or Primitive::destroy instead.
//
void DataNode::destroy()
{
	DataConsumer *begin = inputsBegin;
	DataConsumer *end = inputsEnd;
	while (begin != end)
		begin++->destroy();

  #ifdef DEBUG
	kind = vkVoid;
	constant = false;
	operation = poNone;
	category = pcNone;
	alwaysNonzero = false;
	flags = templates[poNone].flags;
	container = 0;
	inputsBegin = 0;
	inputsEnd = (DataConsumer *)(-1);
  #endif
}


//
// Returns true if data node is a signed compare, false if unsigned compare
// NB must be a compare (checked by assertion)
// (Used by x86 emitter)
//
bool DataNode::isSignedCompare() const
{
	bool result;
	switch(getOperation())
	{
	case poCmp_I:		// comparison is signed
		result = true;
		break;
	
	case poCmpU_I:		// comparison is unsigned
	case poCmpU_A:
		result = false;
		break;

	default:
		assert(false);
	}
	return result;
}


//
// Assign a unique producerNumber to this DataNode if it produces a value.
// The first number assigned will be base; return the last number assigned + 1.
//
Uint32 DataNode::assignProducerNumbers(Uint32 base)
{
	if (producesVariable())
		producerNumber = base++;
	return base;
}


#ifdef DEBUG_LOG
//
// Print the outgoing edge, if any, of a DataNode along with the := sign.
//
void DataNode::printOutgoing(LogModuleObject &f) const
{
	if (producesVariable()) {
		printRef(f);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("  := "));
	}
}


//
// Print the incoming edges of a DataNode preceded by some white space.
//
void DataNode::printIncoming(LogModuleObject &f) const
{
	if (inputsBegin != inputsEnd) {
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("  "));
		bool printSeparator = false;
		for (DataConsumer *dc = inputsBegin; dc != inputsEnd; dc++) {
			if (printSeparator)
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
			printSeparator = true;
			assert(&dc->getNode() == this);
			dc->printRef(f);
		}
	}
}


//
// Print a DataNode for debugging purposes.
// f should be at the beginning of a line.
//
void DataNode::printPretty(LogModuleObject &f, int margin) const
{
	printMargin(f, margin);
	ControlNode *c = getContainer();
	if (c && c->hasControlKind() && hasTryPrimitive(c->getControlKind()))
		if (this == c->getTryExtra().tryPrimitive)
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Exc  "));
	printOutgoing(f);
	print(f, getOperation());
	printIncoming(f);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
}
#endif


// ----------------------------------------------------------------------------
// PhiNode

//
// Construct a new phi node.  nExpectedInputs is an estimate of the number
// of incoming edges that the phi node will have.  The estimate does not have to
// be accurate, but performance will suffer if it is too low.
// The phi node starts with no incoming edges.  The caller must explicitly add
// each incoming edge in the same order as the order in which the predecessors of
// the PhiNode's container are listed.
//
PhiNode::PhiNode(Uint32 nExpectedInputs, ValueKind kind, Pool &pool):
	DataNode((PrimitiveOperation)(poPhi_I + kind - vkInt))
{
	assert(hasFormat(pfPhi));
	DataConsumer *edges = new(pool) DataConsumer[nExpectedInputs];
	inputsBegin = edges;
	inputsEnd = edges;
	inputsLimit = edges + nExpectedInputs;
}


//
// Destroy this PhiNode, which must have been initialized.  This PhiNode's
// consumers are destroyed and the PhiNode is removed from its container.
//
void PhiNode::destroy()
{
	remove();
	DataNode::destroy();
}


//
// Add a new input to the phi node.  The inputs must be added in the same
// order as the order of the predecessors of the PhiNode's container.
// Allocate edges from the given pool.
//
void PhiNode::addInput(DataNode &producer, Pool &pool)
{
	DataConsumer *input = inputsEnd;
	if (input == inputsLimit) {
		Uint32 newNEdges = (input - inputsBegin) * 2;
		if (newNEdges < 4)
			newNEdges = 4;
		DataConsumer *newInputsBegin = new(pool) DataConsumer[newNEdges];
		input = move(inputsBegin, input, newInputsBegin);
		inputsBegin = newInputsBegin;
		inputsLimit = newInputsBegin + newNEdges;
	}
	input->setNode(*this);
	input->setVariable(producer);
	inputsEnd = input + 1;
}


//
// Call f once on each of this phi node's inputs in order.  Remove from this phi
// node the inputs for which f returns true.  This phi node remains a phi node
// (is not optimized out) even if all or all but one of its inputs are removed.
// The function f also gets each input's corresponding predecessor.
//
// CAUTION:  Function f should not cache the DataConsumer reference it obtains
// because that DataConsumer can be destroyed.  If it wants to cache the DataConsumer,
// it should make a copy.
//
void PhiNode::removeSomeInputs(Function2<bool, DataConsumer &, ControlEdge &> &f)
{
	const DoublyLinkedList<ControlEdge> &predecessors = getContainer()->getPredecessors();
	assert(predecessors.lengthIs(nInputs()));
	DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin();
	DataConsumer *srcInput = inputsBegin;
	DataConsumer *dstInput = srcInput;
	DataConsumer *end = inputsEnd;
	while (srcInput != end) {
		if (f(*srcInput, predecessors.get(i)))
			srcInput->destroy();
		else {
			if (srcInput != dstInput)
				dstInput->move(*srcInput);
			dstInput++;
		}
		srcInput++;
		i = predecessors.advance(i);
	}
	inputsEnd = dstInput;
}


// ----------------------------------------------------------------------------
// Primitive


//
// Destroy this Primitive, which must have been initialized.  This Primitive's
// consumers are destroyed and the Primitive is removed from its container.
//
void Primitive::destroy()
{
	remove();
	DataNode::destroy();
}


// ----------------------------------------------------------------------------
// PrimConst

//
// Create a PrimConst with the given value.
//
PrimConst::PrimConst(ValueKind vk, const Value &value, Uint32 bci):
	Primitive0((PrimitiveOperation)(poConst_I + vk - vkInt), bci),
	value(value)
{
	assert(isRegOrMemKind(vk) && hasFormat(pfConst));
	setAlwaysNonzero(value.isNonzero(vk));
}


#ifdef DEBUG_LOG
void PrimConst::printIncoming(LogModuleObject &f) const
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("  "));
	value.print(f, getKind());
}
#endif


// ----------------------------------------------------------------------------
// PrimProj

//
// Create a PrimProj with the given operation.
// If alwaysNonzero is true, the output is known to never be zero.
// The caller then has to connect the input to the data node that produces the tuple.
//
PrimProj::PrimProj(PrimitiveOperation op, TupleComponent component, bool
				   alwaysNonzero, Uint32 bci):
	Primitive1(op, bci),
	component(component)
{
	assert(hasFormat(pfProj));
	setAlwaysNonzero(alwaysNonzero);
}


//
// Create a PrimProj with the given value kind, which must be a storable ValueKind.
// If alwaysNonzero is true, the output is known to never be zero.
// The caller then has to connect the input to the data node that produces the tuple.
//
PrimProj::PrimProj(ValueKind vk, TupleComponent component, bool
				   alwaysNonzero, Uint32 bci):
	Primitive1((PrimitiveOperation)(poProj_I + vk - vkInt), bci),
	component(component)
{
	assert(isStorableKind(vk));
	setAlwaysNonzero(alwaysNonzero);
}


#ifdef DEBUG_LOG
void PrimProj::printIncoming(LogModuleObject &f) const
{
	Primitive1::printIncoming(f);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", component%d", component));
}
#endif


// ----------------------------------------------------------------------------
// PrimArg

//
// Create a PrimArg.  The caller must subsequently call initOutgoingEdge on it.
//
PrimArg::PrimArg(): Primitive0(poArg_M, 0)	// initOutgoingEdge will change the operation as appropriate.
{
	assert(hasFormat(pfArg));
}


//
// Set the PrimArg's kind and flag that states whether the PrimArg might
// ever be zero or null.
//
void PrimArg::initOutgoingEdge(ValueKind vk, bool alwaysNonzero)
{
	assert(isStorableKind(vk) || isMemoryKind(vk));
	setOperation((PrimitiveOperation)(poArg_I + vk - vkInt));
	setAlwaysNonzero(alwaysNonzero);
}


#ifdef DEBUG_LOG
void PrimArg::printIncoming(LogModuleObject &f) const
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("  arg"));
	ControlNode *container = getContainer();
	if (container && container->hasControlKind()) {
		ControlNode::BeginExtra &beginExtra = container->getBeginExtra();
		if (this == &beginExtra.initialMemory)
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("MEM"));
		else {
			for (uint n = 0; n != beginExtra.nArguments; n++)
				if (this == &beginExtra[n]) {
					UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%d", n));
					return;
				}
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("??"));
		}
	}
}
#endif


// ----------------------------------------------------------------------------
// PrimResult

//
// Create a PrimResult.
//
PrimResult::PrimResult(Uint32 bci, ValueKind vk)
	: Primitive1((PrimitiveOperation)(poResult_I + vk - vkInt), bci)
{
	assert((isStorableKind(vk) || isMemoryKind(vk)) && hasFormat(pfResult));
}


#ifdef DEBUG_LOG
void PrimResult::printOutgoing(LogModuleObject &f) const
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("res"));
	ControlNode *container = getContainer();
	if (container && container->hasControlKind())
		if (container->hasControlKind(ckEnd) && this == &container->getEndExtra().finalMemory)
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("MEM"));
		else {
			ControlNode::ReturnExtra &returnExtra = container->getReturnExtra();
			uint n;
			for (n = 0; n != returnExtra.nResults; n++)
				if (this == &returnExtra[n]) {
					UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%d", n));
					break;
				}
			if (n == returnExtra.nResults)
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("??"));
		}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("  := "));
}
#endif


// ----------------------------------------------------------------------------
// PrimControl

//
// Create a PrimControl node.  op should be poSwitch or one of the poIf... kinds.
// The caller then has to connect the input to another data flow node.
//
PrimControl::PrimControl(PrimitiveOperation op, Uint32 bci)
	: Primitive1(op, bci)
{
	assert(hasFormat(pfControl));
}


// ----------------------------------------------------------------------------
// PrimCatch

//
// Create a PrimCatch.
//
PrimCatch::PrimCatch(Uint32 bci): Primitive0(poCatch, bci)
{
	assert(hasFormat(pfCatch));
	setAlwaysNonzero(true);
}


// ----------------------------------------------------------------------------
// PrimUnaryGeneric

//
// Create a generic one-argument, one-result primitive with the given PrimitiveOperation.
// If alwaysNonzero is true, the output is known to never be zero.
// The caller then has to connect the input to another data flow node.
//
PrimUnaryGeneric::PrimUnaryGeneric(PrimitiveOperation op, Uint32 bci, bool alwaysNonzero)
	: Primitive1(op, bci)
{
	assert(hasFormat(pfUnaryGeneric));
	setAlwaysNonzero(alwaysNonzero);
}


// ----------------------------------------------------------------------------
// PrimBinaryGeneric

//
// Create a generic two-argument, one-result primitive with the given PrimitiveOperation.
// If alwaysNonzero is true, the output is known to never be zero.
// The caller then has to connect the inputs to other data flow nodes.
//
PrimBinaryGeneric::PrimBinaryGeneric(PrimitiveOperation op, Uint32 bci,
									 bool alwaysNonzero) 
									 
	: Primitive2(op, bci)
{
	assert(hasFormat(pfBinaryGeneric));
	setAlwaysNonzero(alwaysNonzero);
}


// ----------------------------------------------------------------------------
// PrimLoad

//
// Create a PrimLoad with the given PrimitiveOperation.
// The caller then has to connect the inputs to other data flow nodes.
//
PrimLoad::PrimLoad(PrimitiveOperation op, bool hasOutgoingMemory, Uint32 bci)
	: Primitive2(op, bci), hasOutgoingMemory(hasOutgoingMemory)
{
	assert(hasFormat(pfLd));
}


// ----------------------------------------------------------------------------
// PrimStore

//
// Create a PrimStore with the given PrimitiveOperation.
// The caller then has to connect the inputs to other data flow nodes.
//
PrimStore::PrimStore(PrimitiveOperation op, Uint32 bci) : Primitive(op, bci)
{
	assert(hasFormat(pfSt));
	inputsBegin = inputs;
	inputsEnd = inputs + 3;
	inputs[0].setNode(*this);
	inputs[1].setNode(*this);
	inputs[2].setNode(*this);

	setAlwaysNonzero(true);
}


// ----------------------------------------------------------------------------
// PrimMonitor

//
// Create a monitor primitive with the given PrimitiveOperation (which should be
// poMEnter or poMExit).  slot is the index of the stack slot for the saved subheader.
//
PrimMonitor::PrimMonitor(PrimitiveOperation op, Uint32 slot, Uint32 bci)
	: Primitive2(op, bci), slot(slot)
{
	assert(hasFormat(pfMonitor));
	setAlwaysNonzero(true);
}


// ----------------------------------------------------------------------------
// PrimSync

//
// Create a PrimSync.
//
PrimSync::PrimSync(Uint32 bci): Primitive0(poSync, bci)
{
	assert(hasFormat(pfSync));
	setAlwaysNonzero(true);
}

// ----------------------------------------------------------------------------
// PrimSysCall

//
// Create a system call primitive for the given system call.  The incoming edges
// get allocated from the given pool and connected.
// The caller then has to connect the inputs to other data flow nodes.
//
PrimSysCall::PrimSysCall(const SysCall &sysCall, Pool &pool, Uint32 bci)
	: Primitive(sysCall.operation, bci), sysCall(sysCall)
{
	assert(hasFormat(pfSysCall));
	uint i = sysCall.nInputs;
	DataConsumer *inputs = new(pool) DataConsumer[i];
	inputsBegin = inputs;
	while (i--)
		inputs++->setNode(*this);
	inputsEnd = inputs;
}


#ifdef DEBUG_LOG
void PrimSysCall::printIncoming(LogModuleObject &f) const
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" %s", sysCall.name));
	Primitive::printIncoming(f);
}
#endif


// ----------------------------------------------------------------------------
// PrimCall

//
// Create a call primitive for the given call.  The incoming edges
// get allocated from the given pool and connected.
// The caller then has to connect the inputs to other data flow nodes.
// nArguments and nResults do not include the implicit memory edges or the
// edge carrying the address of the called function.
//
// If the Method object is known for the function being called, method should point to it;
// if not, method must be nil.
//
PrimCall::PrimCall(PrimitiveOperation op, const Method *method, 
				   uint nArguments, uint nResults, Pool &pool, Uint32 bci)
	: Primitive(op, bci), nArguments(nArguments), nResults(nResults), method(method)
{
	assert(hasFormat(pfCall));
	nArguments += 2;
	DataConsumer *inputs = new(pool) DataConsumer[nArguments];
	inputsBegin = inputs;
	while (nArguments--)
		inputs++->setNode(*this);
	inputsEnd = inputs;
}


#ifdef DEBUG_LOG
void PrimCall::printIncoming(LogModuleObject &f) const
{
	if (method) {
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("  ["));
		method->printRef(f);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("]"));
	}
	Primitive::printIncoming(f);
}
#endif

