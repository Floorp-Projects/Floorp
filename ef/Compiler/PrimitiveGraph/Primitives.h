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
#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include "Value.h"
#include "DoublyLinkedList.h"
#include "VirtualRegister.h"
#include "LogModule.h"
#include "Attributes.h"

enum PrimitiveFormat
{
	pfNone,
	pfPhi,				// Phi node
	pfConst,			// Constant
	pfDebug,			// Debug or Break
	pfControl,			// If condition node or switch index node
	pfCatch,			// Catch exception producer
	pfProj,				// Projection
	pfArg,				// Argument
	pfResult,			// Result
	pfUnaryGeneric,		// General one-argument operations
	pfBinaryGeneric,	// General two-argument operations
	pfLd,				// Ld
	pfSt,				// St
	pfMonitor,			// MEnter and MExit
	pfSync,				// Sync
	pfSysCall,			// SysCall
	pfCall				// Call
};
const nPrimitiveFormats = pfCall + 1;

enum PrimitiveCategory
{
	pcNone,
	pcPhi,		// Phi node
	pcConst,	// Constant
	pcDebug,	// Debug or Break
	pcIfCond,	// If condition node
	pcSwitch,	// Switch index node
	pcCatch,	// Catch exception producer
	pcProj,		// Projection
	pcArg,		// Argument
	pcResult,	// Result
	pcGeneric,	// Two-argument integer arithmetic and logical
	pcDivMod,	// Div and Mod
	pcShift,	// Shifts
	pcExt,		// Ext
	pcFGeneric,	// Floating-point arithmetic
	pcConv,		// Conv
	pcFConv,	// FConv
	pcCmp,		// Cmp
	pcFCmp,		// FCmp
	pcCond,		// Cond
	pcCond3,	// Cond3
	pcCheck1,	// ChkNull
	pcCheck2,	// ChkCast, Limit, and LimCast
	pcLd,		// Ld
	pcSt,		// St
	pcMonitor,	// MEnter and MExit
	pcSync,		// Sync
	pcSysCall,	// SysCall
	pcCall		// Call
};
const nPrimitiveCategories = pcCall + 1;

extern const PrimitiveFormat categoryFormats[nPrimitiveCategories];

#include "PrimitiveOperations.h"

#ifdef DEBUG_LOG
int print(LogModuleObject &f, PrimitiveFormat pf);
int print(LogModuleObject &f, PrimitiveCategory pc);
int print(LogModuleObject &f, PrimitiveOperation pk);
#endif


// ----------------------------------------------------------------------------


class DataNode;
class ControlNode;
class ControlEdge;
class Instruction;

class VariableOrConstant
{
  private:
	static const DataNode constSources[nValueKinds];

  protected:									// (In DEBUG versions both variables below are nil if not explicitly set.)
	const DataNode *source;						// The referenced DataNode or (if pointing to a constant) Source
	DataNode *node;								// The node to which this DataConsumer belongs (used in DataConsumer only)
	union {
		DoublyLinkedNode links;					// Links to other consumers of a non-constant value (used in DataConsumer only)
		Value value;							// Constant
	};

  public:
  #ifdef DEBUG
	VariableOrConstant() {source = 0;}
  #endif
	bool operator==(const VariableOrConstant &poc2) const;
	bool operator!=(const VariableOrConstant &poc2) const {return !(*this == poc2);}

	bool isImmediate() const {return false;}
	bool hasKind(ValueKind kind) const;
	ValueKind getKind() const;

	bool isConstant() const;
	bool isVariable() const;
	DataNode &getVariable() const;
	const Value &getConstant() const;
	Value &getConstant();
	bool isAlwaysNonzero() const;
	Type *getDynamicType() const;

	void setConstant(Int32 v);
	void setConstant(Int64 v);
	void setConstant(Flt32 v);
	void setConstant(Flt64 v);
	void setConstant(addr v);
	void setConstant(Condition v);
	void setConstant(Memory v);
	void setConstant(ValueKind kind, const Value &v);
	void setVariable(DataNode &producer);
};


// A DataConsumer is like a VariableOrConstant except that it belongs to some (consumer) DataNode.
// Also, if the DataConsumer is a variable, it is linked into the source (producer) DataNode's
// doubly-linked list of consumers.
// If the DataConsumer currently contains a variable, do not change its source (for example by
// using either setConstant or setVariable).  Instead, call either clearVariable or destroy
// first and then call setConstant or setVariable, thus making sure that the DataNodes' lists
// of consumers are kept consistent.  In DEBUG versions asserts will enforce this rule.
class DataConsumer: public VariableOrConstant
{
  #ifdef DEBUG
  public:
	DataConsumer() {node = 0;}
  private:
	DataConsumer(const DataConsumer &);			// Copying forbidden
	void operator=(const DataConsumer &);		// Copying forbidden
  #endif
  public:
	void operator=(const VariableOrConstant &p);
	void move(DataConsumer &src);
	void destroy();

	DataNode &getNode() const {assert(node); return *node;}
	void setNode(DataNode &n) {node = &n;}

  #ifdef DEBUG
	void setConstant(Int32 v);
	void setConstant(Int64 v);
	void setConstant(Flt32 v);
	void setConstant(Flt64 v);
	void setConstant(addr v);
	void setConstant(Condition v);
	void setConstant(Memory v);
	void setConstant(ValueKind kind, const Value &v);
  #endif
	void setVariable(DataNode &producer);
	void clearVariable();
	void clear() {if (isVariable()) clearVariable();}

	// DoublyLinkedNode administration
	static DataConsumer &linkOwner(DoublyLinkedNode &l) {return *(DataConsumer *)((char *)&l - offsetof(DataConsumer, links));}
	DoublyLinkedNode &getLinks() {return links;}

  #ifdef DEBUG_LOG
	int printRef(LogModuleObject &f) const;
  #endif
};

inline bool isIfCond(PrimitiveOperation op) {return op >= poIfLt && op <= poIfUGe;}
inline bool isCond2(PrimitiveOperation op) {return op >= poLt_I && op <= poUGe_I;}
inline bool isCond3(PrimitiveOperation op) {return op >= poCatL_I && op <= poCatCG_I;}
inline Condition2 ifCondToCondition2(PrimitiveOperation op) {assert(isIfCond(op)); return (Condition2)(op - poIfLt + condLt);}
inline Condition2 cond2ToCondition2(PrimitiveOperation op) {assert(isCond2(op)); return (Condition2)(op - poLt_I + condLt);}
inline Condition3 cond3ToCondition3(PrimitiveOperation op) {assert(isCond3(op)); return (Condition3)(op - poCatL_I);}
inline PrimitiveOperation condition2ToIfCond(Condition2 cond2) {return (PrimitiveOperation)(cond2 - condLt + poIfLt);}
inline PrimitiveOperation condition2ToCond2(Condition2 cond2) {return (PrimitiveOperation)(cond2 - condLt + poLt_I);}
inline PrimitiveOperation condition3ToCond3(Condition3 cond3) {return (PrimitiveOperation)(cond3 + poCatL_I);}

DataNode *partialComputeComparison(Condition2 cond2, VariableOrConstant c, bool &result);


// ----------------------------------------------------------------------------


extern Uint16 gCurLineNumber;					// debug get line number info

class DataNode
{
    ValueKind kind ENUM_8;		// Kind of the constant or the data node's outgoing data edge
    bool constant BOOL_8;		// True if this value is a constant (held inside the DataConsumer)

	enum DataNodeFlag
	{
		dnIsReal,				// This primitive can be created (as opposed to, say, poNone, which cannot)
		dnCanRaiseException,	// This primitive can raise an exception
		dnIsRoot				// This primitive feeds a control node, so it should't be deleted just because its data
	};							//   output is dead.

	struct Template
	{
		PrimitiveOperation operation ENUM_16;	// Minor kind of primitive
		PrimitiveCategory category ENUM_8;		// Major kind of primitive
		ValueKind kind ENUM_8;					// Kind of the constant or the data node's outgoing data edge
		Uint16 flags;							// DataNodeFlags
	};

	static const Template templates[nPrimitiveOperations];

  #ifdef DEBUG
  public:
	enum Origin {aoVariable, aoConstant, aoEither};

	struct InputConstraint
	{
		ValueKind kind ENUM_8;					// Kinds of an input.  vkVoid means "any kind".
		Origin origin ENUM_8;					// Can the input be a variable, a constant, or either?
	};
  private:

	struct InputConstraintPattern
	{
		const InputConstraint *inputConstraints;// Constraints on the inputs.
		Uint8 nInputConstraints;				// Number of inputs (repeat input counts here as one input)
		bool repeat BOOL_8;						// If true, the last input can be repeated zero or more times
	};

	static const InputConstraintPattern inputConstraintPatterns[nPrimitiveOperations];
  #endif

	PrimitiveOperation operation ENUM_16;		// Minor kind of primitive
	PrimitiveCategory category ENUM_8;			// Major kind of primitive
	bool alwaysNonzero BOOL_8;					// If true, the value of this DataNode is known to always be nonzero
	Uint16 flags;								// DataNodeFlags
	DoublyLinkedList<DataConsumer> consumers;	// List of consumers of data produced by this DataNode
  protected:
	ControlNode *container;						// The ControlNode that contains this DataNode or nil if none

	DataConsumer *inputsBegin;					// Array of incoming data flow edges
	DataConsumer *inputsEnd;					//   Last element + 1
	Uint16 lineNumber;							// Java source line number
  public:
	Uint32 producerNumber;						// Serial number assigned and used by the register allocator and pretty-printer

	IntervalMapping *mapping;					// The localVariable
												// corresponding to the
												// mapping
  private:
	struct _annotation
	{
		VirtualRegisterPtr lowVReg;
		VirtualRegisterPtr highVReg;
		Instruction *insn;
	} annotation;


  #ifdef DEBUG
	// make sure we do not incorrectly use annotations
	enum AnnotationKind	{anNone, anInsn, anVr};
	AnnotationKind annotationKind;
  #endif

  protected:
	explicit DataNode(PrimitiveOperation op);

	void destroy();

  public:
    DataNode(ValueKind kind) : kind(kind), constant(true) {};

	// Use these instead of expressions like "getFormat() == pfLd" because the latter doesn't do
	// type checking -- the compiler will happily let you make errors like "getFormat() == pcLd".
    inline bool isConstant() const {return constant;}
    inline bool isVariable() const {return !constant;}

    bool hasKind(ValueKind k) const {return k == kind;}
	bool hasFormat(PrimitiveFormat f) const {return f == categoryFormats[category];}
	bool hasCategory(PrimitiveCategory c) const {return c == category;}
	bool hasOperation(PrimitiveOperation op) const {return op == operation;}
	ValueKind getKind() const {return kind;}
	PrimitiveFormat getFormat() const {return categoryFormats[category];}
	PrimitiveCategory getCategory() const {return category;}
	PrimitiveOperation getOperation() const {return operation;}
	void setOperation(PrimitiveOperation op);

	// Flag queries
	bool isReal() const {return flags>>dnIsReal & 1;}
	bool canRaiseException() const {return flags>>dnCanRaiseException & 1;}
	bool isRoot() const {return flags>>dnIsRoot & 1;}

	bool producesVariable() const {return !isVoidKind(kind);}
	bool isAlwaysNonzero() const {return alwaysNonzero;}
	void setAlwaysNonzero(bool nz) {alwaysNonzero = nz;}
	bool hasConsumers() const {return !consumers.empty();}
	const DoublyLinkedList<DataConsumer> &getConsumers() const {return consumers;}
	void addConsumer(DataConsumer &consumer) {consumers.addLast(consumer);}

	ControlNode *getContainer() const {return container;}
	void setContainer(ControlNode *c) {assert(!container); container = c;} // For use by ControlNode only.
	// see also changeContainer in PhiNode and Primitive.

	Uint32 nInputs() const {return inputsEnd - inputsBegin;}
	DataConsumer *getInputsBegin() const {return inputsBegin;}
	DataConsumer *getInputsEnd() const {return inputsEnd;}
	DataConsumer &nthInput(Uint32 n) const {assert(n < nInputs()); return inputsBegin[n];}
	bool nthInputIsConstant(Uint32 n) const {return nthInput(n).isConstant();}
	DataNode &nthInputVariable(Uint32 n) const;
	bool isSignedCompare() const;
	Value &nthInputConstant(Uint32 n) const {return nthInput(n).getConstant();}

	Uint16 getLineNumber() const {return lineNumber;}
	void setLineNumber(Uint16 lineNumber) {DataNode::lineNumber = lineNumber;}

	inline void annotate(Instruction& insn);
	inline void annotate(VirtualRegister& vReg, VRClass cl);
	inline void annotateLow(VirtualRegister& vReg, VRClass cl);
	inline void annotateHigh(VirtualRegister& vReg, VRClass cl);
	inline void annotate(VirtualRegister& lowVReg, VirtualRegister& highVReg, VRClass cl);
	inline VirtualRegister* getVirtualRegisterAnnotation();
	inline VirtualRegister* getLowVirtualRegisterAnnotation();
	inline VirtualRegister* getHighVirtualRegisterAnnotation();
	inline VirtualRegisterPtr& getVirtualRegisterPtrAnnotation();
	inline VirtualRegisterPtr& getLowVirtualRegisterPtrAnnotation();
	inline VirtualRegisterPtr& getHighVirtualRegisterPtrAnnotation();
	inline Instruction* getInstructionAnnotation();

	Uint32 assignProducerNumbers(Uint32 base);
	virtual void setInstructionRoot(Instruction* /*inInstructionRoot*/) {}
	virtual Instruction* getInstructionRoot() {return NULL;}

#ifdef DEBUG
    virtual void validate();        // perform self-consistency checks
    InputConstraintPattern getInputConstraintPattern() { return inputConstraintPatterns[getOperation()]; }
    bool mark BOOL_8;               // marks this primitive as having been seen during an ordered search
#endif

  #ifdef DEBUG_LOG
  protected:
	virtual void printOutgoing(LogModuleObject &f) const;
	virtual void printIncoming(LogModuleObject &f) const;
  public:
	int printRef(LogModuleObject &f) const;
	void printPretty(LogModuleObject &f, int margin) const;
  #endif

  #if 1
	// HACK! These are temporary stubs and will go away soon.
	DataNode *getOutgoingEdgesBegin() {return this;}
	DataNode *getOutgoingEdgesEnd() {return this + producesVariable();}
  #endif
};


class PhiNode: public DataNode, public DoublyLinkedEntry<PhiNode> // Links to other PhiNodes in the same ControlNode
{
	DataConsumer *inputsLimit;					// Pointer past last entry allocated in the inputsBegin array
												//   (may include extra slop after inputsEnd)

  public:
	PhiNode(Uint32 nExpectedInputs, ValueKind kind, Pool &pool);
	void destroy();

	static PhiNode &cast(DataNode &node)
		{assert(node.getFormat() == pfPhi); return *static_cast<PhiNode *>(&node);}

	void clearContainer() {assert(container); remove(); container = 0;}
	void changeContainer(ControlNode *c) {assert(container); remove(); container = c;} // For use by ControlNode only.

	void addInput(DataNode &producer, Pool &pool);
	void removeSomeInputs(Function2<bool, DataConsumer &, ControlEdge &> &f);
};


class Primitive: public DataNode, public DoublyLinkedEntry<Primitive> // Links to other Primitives in the same ControlNode
{
  protected:
  	Instruction		*instructionRoot;			// Instructions which creates this primitive if no outputs
	Uint16			burgState;					// BURG state given to this node
	Uint16			lineNumber;					// corresponds to src line#		
	Uint32			bytecodeIndex;				// Bytecode Index
	Uint16			debugLineNumber;			// contains value >= lineNumber of all children (during InstructionScheduling)

 public:
	explicit Primitive(PrimitiveOperation op, Uint32 bci);
 	void destroy();

	static Primitive &cast(DataNode &node)
		{assert(node.getFormat() >= pfConst); return *static_cast<Primitive *>(&node);}

	void clearContainer() {assert(container); remove(); container = 0;}
	void changeContainer(ControlNode *c) {assert(container); remove(); container = c;} // For use by ControlNode only.

	Uint16 getBurgState() const { return burgState;}
	Uint16 setBurgState(Uint16 newState) {burgState = newState; return newState;}

	Uint16 getLineNumber() {return lineNumber;}
	Uint16 setLineNumber(Uint16 newLineNumber) {lineNumber = newLineNumber; return lineNumber;}

	Uint16 getDebugLineNumber() {return debugLineNumber;}
	Uint16 setDebugLineNumber(Uint16 newDebugLineNumber) {debugLineNumber = newDebugLineNumber; return debugLineNumber;}

	Uint32 getBytecodeIndex() {return bytecodeIndex;}
	void setBytecodeIndex(Uint32 bci) {bytecodeIndex = bci;};

	void setInstructionRoot(Instruction* inInstructionRoot) {instructionRoot = inInstructionRoot;}
	Instruction* getInstructionRoot() {return instructionRoot;}
};


// --- PRIVATE ----------------------------------------------------------------


// A primitive that has no incoming edges
class Primitive0: public Primitive
{
  protected:
	explicit Primitive0(PrimitiveOperation op, Uint32 bci);
};


// A primitive that has one incoming edge
class Primitive1: public Primitive
{
  protected:
	DataConsumer input;							// Incoming data edge

	explicit Primitive1(PrimitiveOperation op, Uint32 bci);

  public:
	DataConsumer &getInput() {return input;}
};


// A primitive that has two incoming edges
class Primitive2: public Primitive
{
  protected:
	DataConsumer inputs[2];						// Incoming data edges

	explicit Primitive2(PrimitiveOperation op, Uint32 bci);

  public:
	DataConsumer *getInputs() {return inputs;}
};


// --- PUBLIC -----------------------------------------------------------------


// Lone constants
class PrimConst: public Primitive0
{
  public:
	const Value value;

	PrimConst(ValueKind vk, const Value &value, Uint32 bci);

	static PrimConst &cast(DataNode &node)
		{assert(node.hasFormat(pfConst)); return *static_cast<PrimConst *>(&node);}

#ifdef DEBUG
    void validate();        // perform self-consistency checks
#endif

  #ifdef DEBUG_LOG
  protected:
	virtual void printIncoming(LogModuleObject &f) const;
  #endif
};


enum TupleComponent
{
	tcMemory = 0,		// Outgoing memory edge
	tcValue = 1,		// Value returned from a volatile load
	tcFirstResult = 1,	// Component number of first result from a call or system call; subsequent results, if any,
						//   have TupleComponent numbers starting from 2.
	tcMAX = 0x80000000	// (dummy value to force TupleComponent to be 32 bits wide)
};

// Projections of tuple values into their components
class PrimProj: public Primitive1
{
  public:
	const TupleComponent component;				// Number that indicates which component we're getting from the tuple

	PrimProj(PrimitiveOperation op, TupleComponent component, 
			 bool alwaysNonzero, Uint32 bci);
	PrimProj(ValueKind vk, TupleComponent component, bool alwaysNonzero,
			 Uint32 bci);

	static PrimProj &cast(DataNode &node)
		{assert(node.hasFormat(pfProj)); return *static_cast<PrimProj *>(&node);}

  #ifdef DEBUG_LOG
  protected:
	virtual void printIncoming(LogModuleObject &f) const;
  #endif
};


// Incoming function arguments
class PrimArg: public Primitive0
{
  public:
	PrimArg();
	void initOutgoingEdge(ValueKind vk, bool alwaysNonzero);

	static PrimArg &cast(DataNode &node)
		{assert(node.hasFormat(pfArg)); return *static_cast<PrimArg *>(&node);}

  #ifdef DEBUG_LOG
  protected:
	virtual void printIncoming(LogModuleObject &f) const;
  #endif
};


// Outgoing function results
class PrimResult: public Primitive1
{
  public:
	explicit PrimResult(Uint32 bci, ValueKind vk = vkMemory);
	void setResultKind(ValueKind vk);

	static PrimResult &cast(DataNode &node)
		{assert(node.hasFormat(pfResult)); return *static_cast<PrimResult *>(&node);}

  #ifdef DEBUG_LOG
  protected:
	virtual void printOutgoing(LogModuleObject &f) const;
  #endif
};


// Anchor primitives for linking if and switch inputs to their control nodes;
class PrimControl: public Primitive1
{
  public:
	explicit PrimControl(PrimitiveOperation op, Uint32 bci);

	static PrimControl &cast(DataNode &node)
		{assert(node.hasFormat(pfControl)); return *static_cast<PrimControl *>(&node);}
};


// Caught exceptions
class PrimCatch: public Primitive0
{
  public:
	PrimCatch(Uint32 bci);

	static PrimCatch &cast(DataNode &node)
		{assert(node.hasFormat(pfCatch)); return *static_cast<PrimCatch *>(&node);}
};


// Unary arithmetic and logical primitives
class PrimUnaryGeneric: public Primitive1
{
  public:
	explicit PrimUnaryGeneric(PrimitiveOperation op, Uint32 bci,
							  bool alwaysNonzero = false);

	static PrimUnaryGeneric &cast(DataNode &node)
		{assert(node.hasFormat(pfUnaryGeneric)); return *static_cast<PrimUnaryGeneric *>(&node);}
};


// Binary arithmetic and logical primitives, including Div and Mod
class PrimBinaryGeneric: public Primitive2
{
  public:
	explicit PrimBinaryGeneric(PrimitiveOperation op, Uint32 bci,
							   bool alwaysNonzero = false);

	static PrimBinaryGeneric &cast(DataNode &node)
		{assert(node.hasFormat(pfBinaryGeneric)); return *static_cast<PrimBinaryGeneric *>(&node);}
};


// Memory read primitives
class PrimLoad: public Primitive2
{
  protected:
	bool hasOutgoingMemory;						// True if this is a volatile load

  public:
	PrimLoad(PrimitiveOperation op, bool hasOutgoingMemory, Uint32 bci);

	static PrimLoad &cast(DataNode &node)
		{assert(node.hasFormat(pfLd)); return *static_cast<PrimLoad *>(&node);}

	DataConsumer &getIncomingMemory() {return inputs[0];}
	DataConsumer &getAddress() {return inputs[1];}
};


// Memory write primitives
class PrimStore: public Primitive
{
  protected:
	DataConsumer inputs[3];						// Incoming data edges (memory, address, and data)

  public:
	explicit PrimStore(PrimitiveOperation op, Uint32 bci);

	static PrimStore &cast(DataNode &node)
		{assert(node.hasFormat(pfSt)); return *static_cast<PrimStore *>(&node);}

	DataConsumer &getIncomingMemory() {return inputs[0];}
	DataConsumer &getAddress() {return inputs[1];}
	DataConsumer &getData() {return inputs[2];}
};


// Monitor access primitives
class PrimMonitor: public Primitive2
{
	Uint32 slot;								// Index of stack slot for the saved subheader

  public:
	enum {unknownSlot = (Uint32)-1};			// Value to store in slot if not known yet

	PrimMonitor(PrimitiveOperation op, Uint32 slot, Uint32 bci);

	static PrimMonitor &cast(DataNode &node)
		{assert(node.hasFormat(pfMonitor)); return *static_cast<PrimMonitor *>(&node);}

	DataConsumer &getIncomingMemory() {return inputs[0];}
	DataConsumer &getMonitor() {return inputs[1];}
	Uint32 getSlot() const {assert(slot != unknownSlot); return slot;}
};


// Memory synchronization point primitives
class PrimSync: public Primitive0
{
  public:
	PrimSync(Uint32 bci);

	static PrimSync &cast(DataNode &node)
		{assert(node.hasFormat(pfSync)); return *static_cast<PrimSync *>(&node);}
};


struct SysCall;

// System call primitives
class PrimSysCall: public Primitive
{
  public:
	const SysCall &sysCall;						// The type of system call represented here

	PrimSysCall(const SysCall &sysCall, Pool &pool, Uint32 bci);

	static PrimSysCall &cast(DataNode &node)
		{assert(node.hasFormat(pfSysCall)); return *static_cast<PrimSysCall *>(&node);}

  #ifdef DEBUG_LOG
  protected:
	virtual void printIncoming(LogModuleObject &f) const;
  #endif
};


// Function call primitives
class PrimCall: public Primitive
{
  public:
	const uint nArguments;						// Number of arguments in this call (does not include the function or memory edges)
	const uint nResults;						// Number of results in this call (does not include the outgoing memory edge)
	const Method *const method;					// The Method being called or nil if not known

	PrimCall(PrimitiveOperation op, const Method *method, uint nArguments,
			 uint nResults, Pool &pool, Uint32 bci);

	static PrimCall &cast(DataNode &node)
		{assert(node.hasFormat(pfCall)); return *static_cast<PrimCall *>(&node);}

	DataConsumer &getIncomingMemory() {return inputsBegin[0];}
	DataConsumer &getFunction() {return inputsBegin[1];}
	DataConsumer *getArguments() {return inputsBegin + 2;}

  #ifdef DEBUG_LOG
  protected:
	virtual void printIncoming(LogModuleObject &f) const;
  #endif
};


// --- INLINES ----------------------------------------------------------------


inline bool VariableOrConstant::hasKind(ValueKind kind) const {assert(source); return source->hasKind(kind);}
inline ValueKind VariableOrConstant::getKind() const {assert(source); return source->getKind();}

inline bool VariableOrConstant::isConstant() const {assert(source); return source->isConstant();}
inline bool VariableOrConstant::isVariable() const {assert(source); return source->isVariable();}

inline const Value &VariableOrConstant::getConstant() const {assert(source && source->isConstant()); return value;}
inline Value &VariableOrConstant::getConstant() {assert(source && source->isConstant()); return value;}

inline void VariableOrConstant::setConstant(Int32 v) {source = constSources + vkInt; value.i = v;}
inline void VariableOrConstant::setConstant(Int64 v) {source = constSources + vkLong; value.l = v;}
inline void VariableOrConstant::setConstant(Flt32 v) {source = constSources + vkFloat; value.f = v;}
inline void VariableOrConstant::setConstant(Flt64 v) {source = constSources + vkDouble; value.d = v;}
inline void VariableOrConstant::setConstant(addr v) {source = constSources + vkAddr; value.a = v;}
inline void VariableOrConstant::setConstant(Condition v) {source = constSources + vkCond; value.c = v;}
inline void VariableOrConstant::setConstant(Memory v) {source = constSources + vkMemory; value.m = v;}
inline void VariableOrConstant::setConstant(ValueKind kind, const Value &v) {source = constSources + kind; value = v;}
    
//
// Return the producer linked to this VariableOrConstant or DataConsumer.
//
inline DataNode &VariableOrConstant::getVariable() const
{
	assert(source && source->isVariable());
	return *const_cast<DataNode *>(source);
}


//
// If known, return the actual type of the object produced by this VariableOrConstant.
// If not known, return nil.
// The returned type is the most specific type of the object, not a conservative statically
// computed bound.
// Currently this method works only on VariableOrConstants of kind vkAddr.
//
inline Type *VariableOrConstant::getDynamicType() const
{
	assert(hasKind(vkAddr));
	// For now, don't infer types of non-constants.
	return isConstant() ? objectAddressType(value.a) : 0;
}


//
// Set this VariableOrConstant to represent the given producer.
//
inline void VariableOrConstant::setVariable(DataNode &producer)
{
	source = &producer;
	links.init();
}


// ----------------------------------------------------------------------------



#ifdef DEBUG
inline void DataConsumer::setConstant(Int32 v) {assert(!(source && source->isVariable())); VariableOrConstant::setConstant(v);}
inline void DataConsumer::setConstant(Int64 v) {assert(!(source && source->isVariable())); VariableOrConstant::setConstant(v);}
inline void DataConsumer::setConstant(Flt32 v) {assert(!(source && source->isVariable())); VariableOrConstant::setConstant(v);}
inline void DataConsumer::setConstant(Flt64 v) {assert(!(source && source->isVariable())); VariableOrConstant::setConstant(v);}
inline void DataConsumer::setConstant(addr v)  {assert(!(source && source->isVariable())); VariableOrConstant::setConstant(v);}
inline void DataConsumer::setConstant(Condition v) {assert(!(source && source->isVariable())); VariableOrConstant::setConstant(v);}
inline void DataConsumer::setConstant(Memory v) {assert(!(source && source->isVariable())); VariableOrConstant::setConstant(v);}
inline void DataConsumer::setConstant(ValueKind kind, const Value &v) {assert(!(source && source->isVariable())); VariableOrConstant::setConstant(kind, v);}
#endif

inline void DataConsumer::clearVariable() {assert(source && source->isVariable()); links.remove(); DEBUG_ONLY(source = 0);}
   
//
// Destroy this DataConsumer, which must have been initialized.  This DataConsumer
// is unlinked from any linked lists to which it belonged and left uninitialized.
//
inline void DataConsumer::destroy()
{
	if (isVariable())
		links.remove();
  #ifdef DEBUG
	source = 0;
	node = 0;
  #endif
}


//
// Link the DataConsumer located to the given, non-constant source.
//
inline void DataConsumer::setVariable(DataNode &producer)
{
	assert(!(source && source->isVariable()));
	VariableOrConstant::setVariable(producer);
	producer.addConsumer(*this);
}


// ----------------------------------------------------------------------------


//
// Initialize this DataNode, which will contain a primitive or phi node with
// the given operation.
//
inline DataNode::DataNode(PrimitiveOperation op)
{
	const Template &t = templates[op];
	kind = t.kind;
	constant = false;
	operation = t.operation;
	category = t.category;
	alwaysNonzero = false;
	flags = t.flags;
	container = 0;
	mapping = 0;

	assert(isReal());

  #ifdef DEBUG
	producerNumber = (Uint32)-1;
	annotationKind = anNone;
  #endif
	annotation.insn = NULL;
	lineNumber = gCurLineNumber++;
}


//
// Change the operation of a DataNode to the given operation.
// A DataNode's operation can only be changed within the same category.
//
inline void DataNode::setOperation(PrimitiveOperation op)
{
	const Template &t = templates[op];
	assert(hasCategory(t.category));
	kind = t.kind;
	operation = t.operation;
	alwaysNonzero = false;
	flags = t.flags;
}


//
// Return the nth input, which must be a variable.
//
inline DataNode &DataNode::nthInputVariable(Uint32 n) const
{
	return nthInput(n).getVariable();
}


inline void DataNode::annotate(Instruction& insn) 
{
	annotation.insn = &insn; 
	DEBUG_ONLY(annotationKind = anInsn); 
}

inline void DataNode::annotateLow(VirtualRegister& vReg, VRClass cl)
{
	annotation.lowVReg.initialize(vReg, cl); 
	DEBUG_ONLY(annotationKind = anVr); 
#ifdef DEBUG
	vReg.isAnnotated = true;
	if (vReg.isPreColored())
		trespass("This is not a temporary register. It cannot be preColored. !!!");
#endif
}

inline void DataNode::annotateHigh(VirtualRegister& vReg, VRClass cl)
{
	annotation.highVReg.initialize(vReg, cl); 
	DEBUG_ONLY(annotationKind = anVr); 
#ifdef DEBUG
	vReg.isAnnotated = true;
	if (vReg.isPreColored())
		trespass("This is not a temporary register. It cannot be preColored. !!!");
#endif
}

inline void DataNode::annotate(VirtualRegister& vReg, VRClass cl) {annotateLow(vReg, cl);}

inline void DataNode::annotate(VirtualRegister& lowVReg, VirtualRegister& highVReg, VRClass cl)
{
	annotation.lowVReg.initialize(lowVReg, cl); 
	annotation.highVReg.initialize(highVReg, cl); 
	DEBUG_ONLY(annotationKind = anVr); 
#ifdef DEBUG
	lowVReg.isAnnotated = true;
	highVReg.isAnnotated = true;
	if (lowVReg.isPreColored() || highVReg.isPreColored())
		trespass("This is not a temporary register. It cannot be preColored. !!!");
#endif
}
	
inline VirtualRegister* DataNode::getLowVirtualRegisterAnnotation()
{
	assert(annotationKind == anNone || annotationKind == anVr);
	VirtualRegisterPtr& vRegPtr = annotation.lowVReg;
	if (vRegPtr.isInitialized())
		return &vRegPtr.getVirtualRegister();
	else
		return 0;
}

inline VirtualRegister* DataNode::getVirtualRegisterAnnotation() {return getLowVirtualRegisterAnnotation();}

inline VirtualRegister* DataNode::getHighVirtualRegisterAnnotation()
{
	assert(annotationKind == anNone || annotationKind == anVr);
	VirtualRegisterPtr& vRegPtr = annotation.highVReg;
	if (vRegPtr.isInitialized())
		return &vRegPtr.getVirtualRegister();
	else
		return 0;
}

inline VirtualRegisterPtr& DataNode::getLowVirtualRegisterPtrAnnotation() 
{ 
	assert(annotationKind == anNone || annotationKind == anVr);
	return annotation.lowVReg; 
}

inline VirtualRegisterPtr& DataNode::getHighVirtualRegisterPtrAnnotation() 
{ 
	assert(annotationKind == anNone || annotationKind == anVr);
	return annotation.highVReg; 
}

inline VirtualRegisterPtr& DataNode::getVirtualRegisterPtrAnnotation() {return getLowVirtualRegisterPtrAnnotation();}

inline Instruction* DataNode::getInstructionAnnotation() 
{ 
	assert(annotationKind == anNone || annotationKind == anInsn); 
	return (annotation.insn);
}


// ----------------------------------------------------------------------------


//
// Initialize a primitive with the given operation.
// The caller then has to connect the inputs to other data flow nodes.
//
inline Primitive::Primitive(PrimitiveOperation op, Uint32 bci):
	DataNode(op),
	instructionRoot(0),
	bytecodeIndex(bci),
	debugLineNumber(0)
{}


//
// Initialize a no-input primitive with the given operation.
//
inline Primitive0::Primitive0(PrimitiveOperation op, Uint32 bci)
	: Primitive(op, bci)
{
	inputsBegin = 0;
	inputsEnd = 0;
}


//
// Initialize a one-input primitive with the given operation.
// The caller then has to connect the input to another data flow node.
//
inline Primitive1::Primitive1(PrimitiveOperation op, Uint32 bci)
	: Primitive(op, bci)
{
	inputsBegin = &input;
	inputsEnd = &input + 1;
	input.setNode(*this);
}


//
// Initialize a two-input primitive with the given operation.
// The caller then has to connect the inputs to other data flow nodes.
//
inline Primitive2::Primitive2(PrimitiveOperation op, Uint32 bci)
	: Primitive(op, bci)
{
	inputsBegin = inputs;
	inputsEnd = inputs + 2;
	inputs[0].setNode(*this);
	inputs[1].setNode(*this);
}


//
// Change the value kind of the PrimResult.
//
inline void PrimResult::setResultKind(ValueKind vk)
{
	assert(isStorableKind(vk) || isMemoryKind(vk));
	setOperation((PrimitiveOperation)(poResult_I + vk - vkInt));
}

#endif
