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
#ifndef BYTECODETRANSLATOR_H
#define BYTECODETRANSLATOR_H

#include "BytecodeGraph.h"

class BytecodeTranslator: public TranslationCommonEnv
{
	// Helper for translating a BytecodeBlock
	class BlockTranslator
	{
		const BytecodeTranslator &bt;			// Backpointer to the current BytecodeTranslator
		Pool &envPool;							// The pool for allocating translation environment parts (disappears after primitives are generated)
		Pool &primitivePool;					// The pool for allocating new parts of the primitive graph
		ClassFileSummary &classFileSummary;		// The BytecodeGraph's ClassFileSummary
		TranslationEnv *translationEnv;			// The current state of local and stack variables; never nil
		BytecodeBlock &block;					// The block which we're currently translating
		ControlNode *cn;						// The control node currently being generated or nil if we are done

	  public:
		BlockTranslator(const BytecodeTranslator &bt, TranslationEnv &env, BytecodeBlock &block, ControlNode *cn);

		TranslationEnv &getEnv() const {return *translationEnv;}
		void setEnv(TranslationEnv &env) {translationEnv = &env;}
		ControlGraph &getControlGraph() const {return bt.controlGraph;}

	  private:
		void appendControlNode(ControlEdge &e);
		void genException(ControlKind ck, Primitive *tryPrimitive, const Class &exceptionClass, bool canOwnEnv) const;
		void genThrowCommon(const VariableOrConstant &exception, const Class &exceptionClass, bool canOwnEnv);
		void genSimpleThrow(cobj exceptionObject, bool canOwnEnv);
		void genPossibleThrow(Primitive &prim, const Class &exceptionClass);
		void createBasicBlock(BasicBlock &successor);
		void createIf(Condition2 cond2, bool reverseCond2, 
					  VariableOrConstant &c, BasicBlock &successorFalse, 
					  BasicBlock &successorTrue, Uint32 bci);
		void createContinuedIf(Condition2 cond2, bool reverseCond2,
							   VariableOrConstant &c, 
							   BasicBlock &successorTrue, Uint32 bci);
		void genNullGuard(const VariableOrConstant &arg, Uint32 bci);
		void genTableSwitch(const bytecode *bc, Uint32 bci);
		void genLookupSwitch(const bytecode *bc, Uint32 bci);
		void genReadObjectType(const VariableOrConstant &object,
							   VariableOrConstant &type, Uint32 bci) const; 
		void genGetStatic(ConstantPoolIndex cpi, Uint32 bci) const;
		void genPutStatic(ConstantPoolIndex cpi, Uint32 bci) const;
		void genGetField(ConstantPoolIndex cpi, Uint32 bci);
		void genPutField(ConstantPoolIndex cpi, Uint32 bci);
		void genReadArrayLength(const VariableOrConstant &arrayAddr, VariableOrConstant &arrayLength, Uint32 bci);
		void genArrayCastGuard(const VariableOrConstant &array, const VariableOrConstant &object);
		void genArrayEltAccess(TypeKind tk, bool write, Uint32 bci);
	    void genCheckInterfaceAssignability(VariableOrConstant &type, const Type &targetInterface, VariableOrConstant *results, Uint32 bci) const;
		void genCheckCastNonNull(const Type &t, const VariableOrConstant &arg, Uint32 bci);
		void genCheckCast(ConstantPoolIndex cpi, const VariableOrConstant &arg, Uint32 bci);
	    void genInstanceOfNonNull(const Type &t, const VariableOrConstant &arg, VariableOrConstant &result, ControlNode *cnFalse, Uint32 bci);
		void genInstanceOf(ConstantPoolIndex cpi, const VariableOrConstant &arg, VariableOrConstant &result, Uint32 bci);
		void genNewCommon(const SysCall &sysCall, uint nArgs, const VariableOrConstant *args, Uint32 bci);
		void genTempArrayOfDim(Int32 dim, Uint32 bci);
		Primitive &genMonitor(PrimitiveOperation op, const VariableOrConstant &arg, Uint32 bci) const;
		void genThrow(const VariableOrConstant &exception);
		void genReceiverNullCheck(const Signature &sig, VariableOrConstant &receiver, Uint32 bci);
		void genVirtualLookup(const VariableOrConstant &receiver, Uint32 vIndex, VariableOrConstant &functionAddr, Uint32 bci) const;
		void genInterfaceLookup(const VariableOrConstant &receiver, Uint32 interfaceNumber, Uint32 vIndex, const Method *method,
	                            VariableOrConstant &functionAddr, Uint32 bci) const;
		void genInvoke(bytecode opcode, ConstantPoolIndex cpi, uint nArgs, Uint32 bci);
	  public:
		void genLivePrimitives();
		void genCycleHeader();

	  #ifdef DEBUG
		bool translatorDone() const {return !cn;}
	  #endif
	};

	BytecodeGraph &bytecodeGraph;				// The BytecodeGraph from which we're translating
	ControlGraph &controlGraph;					// The primitive graph to which we're translating

	BytecodeTranslator(BytecodeGraph &bytecodeGraph, ControlGraph &controlGraph, Pool &envPool);

	static void normalizeEnv(TranslationEnv &env, BasicBlock::StackNormalization stackNormalization);
	static void linkPredecessor(BasicBlock &block, ControlEdge &controlEdge, TranslationEnv &predecessorEnv, bool canOwnEnv);
	static void finishedOnePredecessor(BasicBlock &block);
	static void finishedOutgoingEdges(BasicBlock &block);
	ControlNode *createControlNode(BasicBlock &block, bool mergeBlock) const;

	void genPrimitives(EndBlock &block) const;
	void genPrimitives(CatchBlock &block) const;
	void genPrimitives(BytecodeBlock &block) const;
	void genRawPrimitiveGraph() const;
	void addSynchronization(VariableOrConstant &syncHolder, Uint32 bci) const;
  public:
	static ControlGraph *genPrimitiveGraph(BytecodeGraph &bytecodeGraph, Pool &primitivePool, Pool &tempPool);

	friend class BytecodeTranslator::BlockTranslator;	// BlockTranslator calls the private methods above
};


// --- INLINES ----------------------------------------------------------------


//
// Initialize a translator that will translate a single BytecodeBlock.
//
inline BytecodeTranslator::BlockTranslator::BlockTranslator(const BytecodeTranslator &bt, TranslationEnv &env, BytecodeBlock &block,
															ControlNode *cn):
	bt(bt),
	envPool(bt.envPool),
	primitivePool(bt.primitivePool),
	classFileSummary(bt.bytecodeGraph.classFileSummary),
	translationEnv(&env),
	block(block),
	cn(cn)
{}


//
// Initialize a translator that will construct the control graph out of the given
// bytecode graph.
// envPool will be used for temporary allocation of TranslationEnvs and the data
// structures they contain; all of these can be deallocated when the BytecodeTranslator
// is done.
//
inline BytecodeTranslator::BytecodeTranslator(BytecodeGraph &bytecodeGraph, ControlGraph &controlGraph, Pool &envPool):
	TranslationCommonEnv(envPool, controlGraph.pool, bytecodeGraph.nLocals, bytecodeGraph.stackSize),
	bytecodeGraph(bytecodeGraph),
	controlGraph(controlGraph)
{}


//
// Take note that we just completely generated code for a BasicBlock b
// that is one of the predecessors of BasicBlock block.  If b jumps to
// BasicBlock block, then BasicBlock block's environment should have been
// adjusted accordingly by now.
// This method lets BasicBlock block keep track of how many other
// BasicBlocks remain that can jump to BasicBlock block and whose
// environments have not yet been merged into BasicBlock block.
//
inline void BytecodeTranslator::finishedOnePredecessor(BasicBlock &block)
{
	assert(block.getNSeenPredecessors() != block.nPredecessors);
	block.getNSeenPredecessors()++;
}

#endif
