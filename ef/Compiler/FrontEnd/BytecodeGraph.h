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
#ifndef BYTECODEGRAPH_H
#define BYTECODEGRAPH_H

#include "DoublyLinkedList.h"
#include "JavaBytecodes.h"
#include "TranslationEnv.h"
#include "VerificationEnv.h"
#include "ControlGraph.h"
#include "ClassFileSummary.h"
#include "LogModule.h"
#include "CheckedUnion.h"

//
// Invariants for a BytecodeGraph:
//
// 1.  The BytecodeGraph starts with the beginBlock BytecodeBlock.
//
// 2.  The BytecodeGraph contains exactly one EndBlock.
//     That block has no bytecodes and no successors.
//
// 3.  Every BytecodeBlock whose subkind is not skForward contains at least one bytecode.
//     A BytecodeBlock's subkind is skNormal unless specified otherwise below.
//     The EndBlock and CatchBlocks contain no bytecodes.
//
// 4.  The BytecodeGraph contains at most one BytecodeBlock that contains a return instruction
//     (return, ireturn, areturn, etc.).  The return instruction must be the only one in that
//     BytecodeBlock.  That BytecodeBlock must have exactly one successor, the EndBlock, and
//     no exceptional successors.  That BytecodeBlock's subkind must be skReturn.
//
// 5.  An instruction of kind bckIf can only occur if it is the last one in a BytecodeBlock.
//     That BytecodeBlock must have exactly two successors, the first of which is the false
//     branch and the second the true branch.
//
// 6.  An instruction of kind bckGoto or bckGoto_W may occur only if it is the last one in a
//     BytecodeBlock.  That BytecodeBlock must have exactly one successor.
//
// 7.  An instruction of kind bckTableSwitch or bckLookupSwitch may occur only if it is the
//     last one in a BytecodeBlock.  That BytecodeBlock must have n+1 successors, where n
//     is the number of cases.  The first successor is the default, while the remaining ones
//     correspond to the cases in the order they are listed in the tableswitch or lookupswitch
//     instruction.
//
// 8.  An instruction of kind bckThrow may occur only if it is the last one in a
//     BytecodeBlock.  That BytecodeBlock must have no successors.
//
// 9.  A jsr (or jsr_w) instruction must be the only one in its BytecodeBlock.  That BytecodeBlock
//     must have either one or two successors and no exceptional successors.  The first successor
//     is the first BytecodeBlock of the subroutine (also called the subroutine's header).
//     The second successor, if present, is the BytecodeBlock containing instructions after the jsr.
//     The second successor is absent if verification determines that the subroutine never returns
//     via a BytecodeBlock of kind skRet.  The jsr BytecodeBlock's subkind must be skJsr if it has
//     two successors or skJsrNoRet if it has one successor.
//
// 10. A ret (or wide ret) instruction must be the only one in its BytecodeBlock.  That BytecodeBlock
//     must have no successors and no exceptional successors.  This BytecodeBlock's subkind must be
//     skRet.
//
// 11. A BytecodeBlock with subkind skForward must not have any instructions.  That BytecodeBlock
//     must have exactly one successor and no exceptional successors and is treated as an
//     unconditional branch to that one successor.
//
// 12. No BytecodeBlock may contain an instruction of kind bckIllegal.
//
// 13. Every BytecodeBlock that ends with one of the remaining instructions (kinds bckNormal
//     or bckExc) must have exactly one successor.
//
// 14. Exceptional successors are only allowed in BytecodeBlocks that do not contain return,
//     ret, or jsr instructions.  Every exceptional successor must be either a CatchBlock or the
//     EndBlock.  
//
// 15. Non-exceptional successors may not refer to a CatchBlock.
//
// 16. The one successor of a CatchBlock must be a BytecodeBlock.  That BytecodeBlock's catchBlock
//     must point back to that CatchBlock.
//

class BytecodeGraph;
struct BytecodeBlock;
struct BasicBlock: DoublyLinkedEntry<BasicBlock> // Links to other BasicBlocks in the same graph
{
	// Persistent fields
	enum Kind {bbEnd, bbBytecode, bbCatch};
	enum StackNormalization {snNoChange, sn0, sn1, sn2}; // Number of words to leave on the stack when entering a BasicBlock.
												// snNoChange means to leave the stack as is (the normal case);
												// sn0, sn1, and sn2 mean to leave only the *top* 0, 1, or 2 words on the stack.
												// These are needed so we can combine all return bytecodes into one block.

	const Kind kind ENUM_8;						// Kind of this BasicBlock (EndBlock, BytecodeBlock, or CatchBlock)
	const StackNormalization stackNormalization ENUM_8; // Number of words to leave on the stack when entering this BasicBlock
	BasicBlock **successorsBegin;				// Array of pointers to successors			(nil if this is the end block)
	BasicBlock **successorsEnd;					// Last normal successor pointer + 1;		(nil if this is the end block)
												//   also first exceptional successor pointer
	BasicBlock **handlersEnd;					// Last exceptional successor pointer + 1	(nil if this is the end block;
  #ifdef DEBUG									//   same as successorsEnd if this is a catch block)
	BasicBlock **successorsPhysicalEnd;			// End of storage allocated for this successors array
  #endif

	// Fields computed by depthFirstSearch
	enum {unmarked = -3, unvisited = -2, unnumbered = -1};
	Int32 dfsNum;								// Serial number assigned by depth-first search (-3 is unmarked, -2 is unvisited, -1 is unnumbered)
	Uint32 nPredecessors;						// Number of BasicBlock successor pointers that refer to this BasicBlock
												//   (often different from the number of ControlNodes that jump to this
												//   BasicBlock's ControlNodes because some BasicBlocks may be discovered
												//   to be dead and hence generate no ControlNodes, while others may generate
												//   code that has multiple branches to their successors.)
												//   The beginBlock pointer is also considered to be a successor pointer (so
												//   nPredecessors of the first block is always at least one).
												//   The implicit exception edges going to the EndBlock are not counted
  #ifdef DEBUG									//   in the end BasicBlock's nPredecessors.
	bool hasIncomingBackEdges;					// True if this block has incoming backward edges
  #endif

  private:
	// Temporary fields for verification
	struct VerificationTemps
	{
		Uint32 generation;						// Number of last dataflow pass to have changed verificationEnvIn; 0 if none yet
		VerificationEnv verificationEnvIn;		// State of locals and stack at beginning of block
		BytecodeBlock *subroutineHeader;		// If this is a ret block, points to the entry point (header) of the subroutine;
												//   nil if not known or this is not a ret block.
		BytecodeBlock *subroutineRet;			// If this is a subroutine header, points to the ret block of that subroutine;
												//   nil if the ret block hasn't been found yet or this is not a subroutine header.
		union {
			BasicBlock *clone;					// The copy of this BasicBlock, temporarily stored here while copying a subgraph of this graph
			bool recompute;						// True if verificationEnvIn has changed since it was last propagated to successors
		};

		VerificationTemps(VerificationEnv::Common &c):
			generation(0), verificationEnvIn(c), subroutineHeader(0), subroutineRet(0), recompute(false) {}
		VerificationTemps(const VerificationEnv &env, Function1<BytecodeBlock *, BytecodeBlock *> &translator,
						  BytecodeBlock *subroutineHeader, BytecodeBlock *subroutineRet):
			generation(0), verificationEnvIn(env, translator), subroutineHeader(subroutineHeader), subroutineRet(subroutineRet) {}
	};

	// Temporary fields for translation into a primitive graph
	struct TranslationTemps
	{
		Uint32 nSeenPredecessors;				// Number of already seen BasicBlock successor pointers that refer to this BasicBlock
		TranslationEnv translationEnvIn;		// State of locals and stack at beginning of block
												//   For efficiency, the end block's envIn contains information only about memory bindings.
		bool envInInitialized BOOL_8;			// True if envIn has already been initialized; if false, no ControlNodes point to
	  #ifdef DEBUG								//   firstControlNode yet.
		bool generatedControlNodes BOOL_8;		// True if ControlNodes have already been generated or optimized out for this BasicBlock
	  #endif
		mutable ControlNode *firstControlNode;	// First ControlNode generated for this BasicBlock or nil if none yet or optimized out
		DoublyLinkedList<ControlEdge> predecessors; // List of predecessors of this block's first generated control node
												//   (Moved into firstControlNode when the ControlNodes for this BasicBlock are created.)
		TranslationTemps(TranslationCommonEnv &commonEnv);
	};

	CheckedUnion2(VerificationTemps, TranslationTemps) temps;
  public:

	BasicBlock(BytecodeGraph &bytecodeGraph, Kind kind, StackNormalization stackNormalization);

	bool hasKind(Kind k) const {return k == kind;}
	Uint32 nSuccessors() const {return successorsEnd - successorsBegin;}
	BytecodeBlock &getSuccessor(Uint32 n) const;

	// Accessors for temporary fields for verification
	void initVerification(VerificationEnv::Common &c);
	void initVerification(BasicBlock &src, Function1<BytecodeBlock *, BytecodeBlock *> &translator);
	Uint32 &getGeneration() {return temps.getVerificationTemps().generation;}
	VerificationEnv &getVerificationEnvIn() {return temps.getVerificationTemps().verificationEnvIn;}
	BytecodeBlock *&getSubroutineHeader() {return temps.getVerificationTemps().subroutineHeader;}
	BytecodeBlock *&getSubroutineRet() {return temps.getVerificationTemps().subroutineRet;}
	BasicBlock *&getClone() {return temps.getVerificationTemps().clone;}
	bool &getRecompute() {return temps.getVerificationTemps().recompute;}

	// Accessors for temporary fields for primitive graph translation
	void initTranslation(TranslationCommonEnv &commonEnv);
	Uint32 &getNSeenPredecessors() {return temps.getTranslationTemps().nSeenPredecessors;}
	bool &getEnvInInitialized() {return temps.getTranslationTemps().envInInitialized;}
  #ifdef DEBUG
	bool getGeneratedControlNodes() const {return temps.getTranslationTemps().generatedControlNodes;}
	bool &getGeneratedControlNodes() {return temps.getTranslationTemps().generatedControlNodes;}
  #endif
	ControlNode *&getFirstControlNode() const {return temps.getTranslationTemps().firstControlNode;}
	DoublyLinkedList<ControlEdge> &getPredecessors() {return temps.getTranslationTemps().predecessors;}
  public:
	TranslationEnv &getTranslationEnvIn() {return temps.getTranslationTemps().translationEnvIn;}
	const TranslationEnv &getTranslationEnvIn() const {return temps.getTranslationTemps().translationEnvIn;}

	// Debugging
  #ifdef DEBUG_LOG
	int printRef(LogModuleObject &f, const BytecodeGraph &bg) const;
	void printPretty(LogModuleObject &f, const BytecodeGraph &bg, const ConstantPool *c, int margin) const;
  #endif
};


struct EndBlock: BasicBlock
{
	EndBlock(BytecodeGraph &bytecodeGraph);
};


struct CatchBlock: BasicBlock
{
  private:
	BasicBlock *successor;						// The handler of exceptions caught here
  public:

	CatchBlock(BytecodeGraph &bytecodeGraph, BytecodeBlock &handler);
	CatchBlock(BytecodeGraph &bytecodeGraph, const CatchBlock &src);

	BytecodeBlock &getHandler() const;
};


struct BytecodeBlock: BasicBlock
{
	enum Subkind
	{
		skNormal,	// A BytecodeBlock that does not contain any return, ret, or jsr instructions
		skReturn,	// A BytecodeBlock consisting entirely of one of the return instructions
		skJsr,		// A BytecodeBlock consisting entirely of a jsr or jsr_w instruction whose subroutine may or may not return
		skJsrNoRet,	// A BytecodeBlock consisting entirely of a jsr or jsr_w instruction whose subroutine is known never to return
		skRet,		// A BytecodeBlock consisting entirely of a ret or wide ret instruction
		skForward	// A formerly skRet BytecodeBlock that is known to unconditionally branch to its one successor
	};

	const bytecode *const bytecodesBegin;		// Pointer to first bytecode in this block
	const bytecode *bytecodesEnd;				// End of last bytecode + 1
	CatchBlock *catchBlock;						// If a catch handler begins at bytecodesBegin, a CatchBlock that points back to
												//   this block; nil otherwise.
	const Class **exceptionClasses;				// Array of exception classes; an exception matching the nth exception class
	const Class **exceptionClassesEnd;			//   would transfer control to the nth exceptional successor
	Subkind subkind ENUM_8;						// Description of contents of this BytecodeBlock

	BytecodeBlock(BytecodeGraph &bytecodeGraph, const bytecode *bytecodesBegin, StackNormalization stackNormalization);
	BytecodeBlock(BytecodeGraph &bytecodeGraph, const BytecodeBlock &src);

	bool hasSubkind(Subkind sk) const {return sk == subkind;}
	bool handlersForbidden() const;

	void initSuccessors(BasicBlock **successors, Uint32 nSuccessors, Int32 nActiveHandlers, Pool &bytecodeGraphPool);
	BasicBlock **transformToJsrNoRet();
	void transformToForward(BasicBlock **successor);
};


class BytecodeGraph
{
  public:
	// Persistent fields
	Pool &bytecodeGraphPool;					// Pool for allocating this BytecodeGraph's nodes (blocks)
	ClassFileSummary &classFileSummary;			// Java class file descriptor
	Method& method;								// The method this
												// BytecodeGraph represents
	const Uint32 nLocals;						// Number of words of local variables
	const Uint32 stackSize;						// Number of words of local stack space
	const bool isInstanceMethod BOOL_8;			// True if this method has a "this" argument
	const bool isSynchronized BOOL_8;			// True if this method is synchronized
	const uint nArguments;						// Number of incoming arguments (including "this")
	const ValueKind *const argumentKinds;		// Kinds of incoming arguments (including "this")
	const ValueKind resultKind;					// Kind of result
	const bytecode *const bytecodesBegin;		// Pointer to first bytecode in this function (must be word-aligned)
	const bytecode *const bytecodesEnd;			// End of last bytecode in this function + 1
	const Uint32 bytecodesSize;					// bytecodesEnd - bytecodesBegin
	const Uint32 nExceptionEntries;				// Number of exception table entries
	const ExceptionItem *const exceptionEntries;// Exception table
	BytecodeBlock *beginBlock;					// Starting basic block in function
	EndBlock *endBlock;							// The end BasicBlock
	BytecodeBlock *returnBlock;					// The return BytecodeBlock or nil if none
  private:
	DoublyLinkedList<BasicBlock> blocks;		// List of basic blocks (may include unreachable blocks)
	Uint32 nBlocks;								// Number of reachable basic blocks

	// Fields computed by depthFirstSearch
	BasicBlock **dfsList;						// Array of reachable basic blocks ordered by depth-first search;
												//   each block's dfsNum is an index into this array.
	const bytecode returnBytecode;				// The kind of return bytecode that corresponds to resultKind
	bool hasBackEdges BOOL_8;					// True if this graph contains a cycle
	bool hasJSRs BOOL_8;						// True if this graph contains a jsr or jsr_w instruction

  public:
	BytecodeGraph(ClassFileSummary &cfs, Pool &bytecodeGraphPool, bool isInstanceMethod, bool isSynchronized,
				  uint nArguments, const ValueKind *argumentKinds, ValueKind resultKind, Uint32 nLocals, Uint32 stackSize,
				  const bytecode *bytecodesBegin, Uint32 bytecodesSize, Uint32 nExceptionEntries,
				  const ExceptionItem *exceptionEntries, Method& method);
  private:
	BytecodeGraph(const BytecodeGraph &);		// Copying forbidden
	void operator=(const BytecodeGraph &);		// Copying forbidden
  public:

	BasicBlock **getDFSList() const {assert(dfsList); return dfsList;}
	Uint32 getDFSListLength() const {assert(dfsList); return nBlocks;}
	void invalidateDFSList() {DEBUG_ONLY(dfsList = 0);}
	void addBlock(BasicBlock &block) {blocks.addLast(block); invalidateDFSList();}

	static const bytecode *switchAlign(const bytecode *bc);

  private:
	BytecodeBlock &followGotos(Uint32 initialOffset, Int32 displacement, BytecodeBlock **blocks);
	BytecodeBlock &noteBlockBoundary(Uint32 offset, BytecodeBlock **blocks);
	void noteExceptionBoundaries(BytecodeBlock **blocks, Int32 *activeHandlerDeltas);
	const bytecode *noteTableSwitchTargets(const bytecode *bc, BytecodeBlock **blocks);
	const bytecode *noteLookupSwitchTargets(const bytecode *bc, BytecodeBlock **blocks);
	const bytecode *recordTableSwitchTargets(const bytecode *bc, BytecodeBlock **blocks, BasicBlock **&successors,
											 Uint32 &nSuccessors, Int32 nActiveHandlers);
	const bytecode *recordLookupSwitchTargets(const bytecode *bc, BytecodeBlock **blocks, BasicBlock **&successors,
											  Uint32 &nSuccessors, Int32 nActiveHandlers);
	void recordHandlerTargets(BytecodeBlock **blocks);
	void verifyNoBoundariesBetween(const bytecode *bc1, const bytecode *bc2, BytecodeBlock **blocks) const;
	void createBlocks(Pool &tempPool);
	bool depthFirstSearch(Pool &tempPool);
	void splitCatchNodes();
  public:
	void divideIntoBlocks(Pool &tempPool);

  #ifdef DEBUG_LOG
	bool dfsBlockNumbersValid() const {return dfsList != 0;}
	void print(LogModuleObject &f, const ConstantPool *c) const;
  #endif
};


// --- INLINES ----------------------------------------------------------------


//
// Initialize BasicBlock fields used for translation into a primitive graph.
//
inline BasicBlock::TranslationTemps::TranslationTemps(TranslationCommonEnv &commonEnv):
	nSeenPredecessors(0),
	translationEnvIn(commonEnv),
	envInInitialized(false),
	firstControlNode(0)
{
  #ifdef DEBUG
	generatedControlNodes = false;
  #endif
}


//
// Initialize this BasicBlock contained in the given BytecodeGraph.
//
inline BasicBlock::BasicBlock(BytecodeGraph &bytecodeGraph, Kind kind, StackNormalization stackNormalization):
	kind(kind),
	stackNormalization(stackNormalization)
{
	bytecodeGraph.addBlock(*this);
}


//
// Return the nth regular (non-exceptional) successor.
// Don't call this to get the successor of the return node, because it is not
// a BytecodeBlock.
//
inline BytecodeBlock &BasicBlock::getSuccessor(Uint32 n) const
{
	assert(n < nSuccessors() && successorsBegin[n]->hasKind(bbBytecode));
	return *static_cast<BytecodeBlock *>(successorsBegin[n]);
}


//
// Prepare this BasicBlock for verification.
//
inline void BasicBlock::initVerification(VerificationEnv::Common &c)
{
	new(temps.initVerificationTemps()) VerificationTemps(c);
}


//
// Prepare this BasicBlock for verification using a copy of src's env, subroutineHeader,
// and subroutineRet fields.  Translate any BasicBlock pointers using the given
// translator.  Set the generation to zero.
//
inline void BasicBlock::initVerification(BasicBlock &src, Function1<BytecodeBlock *, BytecodeBlock *> &translator)
{
	new(temps.initVerificationTemps()) VerificationTemps(src.getVerificationEnvIn(), translator,
														 translator(src.getSubroutineHeader()), translator(src.getSubroutineRet()));
}


//
// Prepare this BasicBlock for translation into a primitive graph.
//
inline void BasicBlock::initTranslation(TranslationCommonEnv &commonEnv)
{
	new(temps.initTranslationTemps()) TranslationTemps(commonEnv);
}


//
// Initialize this CatchBlock contained in the given BytecodeGraph.
// The CatchBlock's successor is given.
//
inline CatchBlock::CatchBlock(BytecodeGraph &bytecodeGraph, BytecodeBlock &handler):
	BasicBlock(bytecodeGraph, bbCatch, sn0),
	successor(&handler)
{
	successorsBegin = &successor;
	successorsEnd = &successor + 1;
	handlersEnd = successorsEnd;
  #ifdef DEBUG
	successorsPhysicalEnd = handlersEnd;
  #endif
}


//
// Make a copy of the src CatchBlock pointing to the same handler as src and add it
// to the given graph.  However, do not copy or initialize the VerificationTemps or
// TranslationTemps in the copy.
//
inline CatchBlock::CatchBlock(BytecodeGraph &bytecodeGraph, const CatchBlock &src):
	BasicBlock(bytecodeGraph, bbCatch, sn0),
	successor(src.successor)
{
	successorsBegin = &successor;
	successorsEnd = &successor + 1;
	handlersEnd = successorsEnd;
  #ifdef DEBUG
	successorsPhysicalEnd = handlersEnd;
  #endif
}


//
// Return the handler passed to this CatchBlock's constructor.
//
inline BytecodeBlock &CatchBlock::getHandler() const
{
	return *static_cast<BytecodeBlock *>(successor);
}


//
// Initialize this BytecodeBlock contained in the given BytecodeGraph.
// The caller should later call initSuccessors to set up this block's successors.
//
inline BytecodeBlock::BytecodeBlock(BytecodeGraph &bytecodeGraph, const bytecode *bytecodesBegin, StackNormalization stackNormalization):
	BasicBlock(bytecodeGraph, bbBytecode, stackNormalization),
	bytecodesBegin(bytecodesBegin),
	catchBlock(0),
	subkind(skNormal)
{}


//
// Return true if this block must not have any handlers and createBlocks should not
// allocates any room for handlers.
//
inline bool BytecodeBlock::handlersForbidden() const
{
	return !hasSubkind(skNormal);
}


//
// Change this skJsr block to a skJsrNoRet block.  Return a pointer suitable as an
// argument to transformToForward.
//
inline BasicBlock **BytecodeBlock::transformToJsrNoRet()
{
	assert(hasSubkind(skJsr));
	subkind = skJsrNoRet;
	BasicBlock **e = successorsEnd - 1;
	successorsEnd = e;
	handlersEnd = e;
  #ifdef DEBUG
	successorsPhysicalEnd = e;
  #endif
	return e;
}


//
// Change this skRet block to a skForward block.  successor must point to a one-word
// block of memory allocated from this bytecode graph's pool that points to this block's
// unique successor.
//
inline void BytecodeBlock::transformToForward(BasicBlock **successor)
{
	assert(hasSubkind(skRet));
	subkind = skForward;
	successorsBegin = successor;
	BasicBlock **e = successor + 1;
	successorsEnd = e;
	handlersEnd = e;
  #ifdef DEBUG
	successorsPhysicalEnd = e;
  #endif
	bytecodesEnd = bytecodesBegin;
}


//
// Return bc word-aligned as needed for a tableswitch or lookupswitch bytecode.
//
inline const bytecode *BytecodeGraph::switchAlign(const bytecode *bc)
{
	// Assumes that bytecodesBegin is word-aligned.
	return (const bytecode *)((size_t)bc + 3 & -4);
}


#endif
