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
#include "BytecodeVerifier.h"
#include "MemoryAccess.h"
#include "GraphUtils.h"
#include "ErrorHandling.h"
#include "DebugUtils.h"


// ----------------------------------------------------------------------------
// BasicBlock


#ifdef DEBUG_LOG
//
// Print a reference to this BasicBlock for debugging purposes.
// Return the number of characters printed.
//
int BasicBlock::printRef(LogModuleObject &f, const BytecodeGraph &bg) const
{
	if (bg.dfsBlockNumbersValid() && dfsNum >= 0)
		return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("B%d", dfsNum));
	else
		return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("B%p", this));
}


//
// Print this BasicBlock for debugging purposes.
// f should be at the beginning of a line.
// If c is non-nil, disassemble constant pool indices symbolically using the information
// in c.
//
void BasicBlock::printPretty(LogModuleObject &f, const BytecodeGraph &bg, const ConstantPool *c, int margin) const
{
	printMargin(f, margin);
	printRef(f, bg);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" (%p): %d predecessors", this, nPredecessors));
  #ifdef DEBUG
	if (hasIncomingBackEdges)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" (cycle header)"));
  #endif
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));

	switch (kind) {
		case bbEnd:
			printMargin(f, margin + 4);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("End block\n"));
			break;
		case bbBytecode:
			{
				const BytecodeBlock *bb = static_cast<const BytecodeBlock *>(this);
				if (!bb->hasSubkind(BytecodeBlock::skNormal)) {
					const char *subkindName;
					switch (bb->subkind) {
						case BytecodeBlock::skReturn:
							subkindName = "Return";
							break;
						case BytecodeBlock::skJsr:
							subkindName = "Jsr";
							break;
						case BytecodeBlock::skJsrNoRet:
							subkindName = "JsrNoRet";
							break;
						case BytecodeBlock::skRet:
							subkindName = "Ret";
							break;
						case BytecodeBlock::skForward:
							subkindName = "Forward";
							break;
						default:
							subkindName = "????";
					}
					printMargin(f, margin + 4);
					UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%s bytecode block\n", subkindName));
				}
				disassembleBytecodes(f, bb->bytecodesBegin, bb->bytecodesEnd, bg.bytecodesBegin, c, margin + 4);
			}
			break;
		case bbCatch:
			printMargin(f, margin + 4);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Catch block\n"));
			break;
	}

	bool printSeparator;
	if (successorsBegin != successorsEnd) {
		printMargin(f, margin);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Successors: "));
		printSeparator = false;
		for (BasicBlock **s = successorsBegin; s != successorsEnd; s++) {
			if (printSeparator)
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
			printSeparator = true;
			(*s)->printRef(f, bg);
		}
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
	}
	if (successorsEnd != handlersEnd) {
		printMargin(f, margin);
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Exception handlers: "));
		printSeparator = false;
		assert(kind == bbBytecode);
		const Class **exceptionClass = static_cast<const BytecodeBlock *>(this)->exceptionClasses;
		for (BasicBlock **s = successorsEnd; s != handlersEnd; s++) {
			if (printSeparator)
				printMargin(f, margin + 20);
			printSeparator = true;
			(*exceptionClass++)->printRef(f);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" -> "));
			(*s)->printRef(f, bg);
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
		}
		assert(exceptionClass == static_cast<const BytecodeBlock *>(this)->exceptionClassesEnd);
	}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
}
#endif


// ----------------------------------------------------------------------------
// EndBlock


//
// Initialize this EndBlock contained in the given BytecodeGraph.
//
inline EndBlock::EndBlock(BytecodeGraph &bytecodeGraph):
	BasicBlock(bytecodeGraph, bbEnd, sn0)
{
	successorsBegin = 0;
	successorsEnd = 0;
	handlersEnd = 0;
  #ifdef DEBUG
	successorsPhysicalEnd = 0;
  #endif
}


// ----------------------------------------------------------------------------
// BytecodeBlock


//
// Make a copy of the src BytecodeBlock pointing to the same successors and CatchBlock
// as src and add it to the given graph.  However, do not copy or initialize the
// VerificationTemps or TranslationTemps in the copy.
//
BytecodeBlock::BytecodeBlock(BytecodeGraph &bytecodeGraph, const BytecodeBlock &src):
	BasicBlock(bytecodeGraph, bbBytecode, src.stackNormalization),
	bytecodesBegin(src.bytecodesBegin),
	bytecodesEnd(src.bytecodesEnd),
	catchBlock(src.catchBlock),
	subkind(src.subkind)
{
	Uint32 nSuccessors = src.nSuccessors();
	Uint32 nSuccessorsAndHandlers = src.handlersEnd - src.successorsBegin;
	BasicBlock **successorsAndHandlers = nSuccessorsAndHandlers ? new(bytecodeGraph.bytecodeGraphPool) BasicBlock *[nSuccessorsAndHandlers] : 0;
	successorsBegin = successorsAndHandlers;
	handlersEnd = copy(src.successorsBegin, src.handlersEnd, successorsAndHandlers);
	successorsEnd = successorsAndHandlers + nSuccessors;
  #ifdef DEBUG
	successorsPhysicalEnd = handlersEnd;
  #endif

	Uint32 nHandlers = nSuccessorsAndHandlers - nSuccessors;
	exceptionClasses = nHandlers ? new(bytecodeGraph.bytecodeGraphPool) const Class *[nHandlers] : 0;
	exceptionClassesEnd = copy(src.exceptionClasses, src.exceptionClassesEnd, exceptionClasses);
}


//
// Set up the successors and handlers arrays for this BytecodeBlock.  The caller should
// have allocated the successors array whose first nSuccessors entries are the normal
// successors and next nActiveHandlers entries are the exceptional successors.  The
// caller should have initialized the normal successors already.  nActiveHandlers must
// be no less than the number of exception handlers active for this BytecodeBlock.
// bytecodeGraphPool is used to allocate the exceptionClasses array.
//
inline void BytecodeBlock::initSuccessors(BasicBlock **successors, Uint32 nSuccessors, Int32 nActiveHandlers, Pool &bytecodeGraphPool)
{
	successorsBegin = successors;
	successorsEnd = successors + nSuccessors;
	handlersEnd = successorsEnd;
	exceptionClasses = nActiveHandlers ? new(bytecodeGraphPool) const Class *[nActiveHandlers] : 0;
	exceptionClassesEnd = exceptionClasses;
  #ifdef DEBUG
	successorsPhysicalEnd = handlersEnd + nActiveHandlers;
  #endif
}


// ----------------------------------------------------------------------------
// BytecodeGraph

static const bytecode returnBytecodes[nValueKinds] = {
	bcReturn,	// vkVoid
	bcIReturn,	// vkInt
	bcLReturn,	// vkLong
	bcFReturn,	// vkFloat
	bcDReturn,	// vkDouble
	bcAReturn,	// vkAddr
	bcNop,		// vkCond
	bcNop,		// vkMemory
	bcNop		// vkTuple
};

static const BasicBlock::StackNormalization returnNormalizations[nValueKinds] = {
	BasicBlock::sn0,		// vkVoid
	BasicBlock::sn1,		// vkInt
	BasicBlock::sn2,		// vkLong
	BasicBlock::sn1,		// vkFloat
	BasicBlock::sn2,		// vkDouble
	BasicBlock::sn1,		// vkAddr
	BasicBlock::snNoChange,	// vkCond
	BasicBlock::snNoChange,	// vkMemory
	BasicBlock::snNoChange	// vkTuple
};


//
// Create a new BytecodeGraph for the given function using the given pool.
// bytecodesBegin must be word-aligned.
//
BytecodeGraph::BytecodeGraph(ClassFileSummary &cfs, Pool &bytecodeGraphPool, bool isInstanceMethod,
		bool isSynchronized, uint nArguments, const ValueKind *argumentKinds, ValueKind resultKind,
		Uint32 nLocals, Uint32 stackSize, const bytecode *bytecodesBegin, Uint32 bytecodesSize, Uint32 nExceptionEntries,
		const ExceptionItem *exceptionEntries, Method& _method):
	bytecodeGraphPool(bytecodeGraphPool),
	classFileSummary(cfs),
	method(_method),
	nLocals(nLocals),
	stackSize(stackSize),
	isInstanceMethod(isInstanceMethod),
	isSynchronized(isSynchronized),
	nArguments(nArguments),
	argumentKinds(argumentKinds),
	resultKind(resultKind),
	bytecodesBegin(bytecodesBegin),
	bytecodesEnd(bytecodesBegin + bytecodesSize),
	bytecodesSize(bytecodesSize),
	nExceptionEntries(nExceptionEntries),
	exceptionEntries(exceptionEntries),
	returnBlock(0),
	nBlocks(0),
	dfsList(0),
	returnBytecode(returnBytecodes[resultKind])
{
	// Make sure that the bytecodes are word-aligned.  If not, the switchAlign function
	// and its clients must be changed to handle unaligned word reads.
	assert(((size_t)bytecodesBegin & 3) == 0);
	assert(returnBytecode != bcNop);
}



//
// followGotos returns the ultimate target of a goto or goto_w and changes the
// blocks array to make the source of the first and every intermediate goto/goto_w
// into an alias of the ultimate target.
//
// initialOffset is the offset of a goto or goto_w bytecode whose displacement is
// also given.  initialOffset is guaranteed to be less than bytecodesSize, but the
// caller does not make any assertions about displacement.
//
// If the first goto/goto_w points to a chain of zero or more intermediate goto/goto_ws
// that ends at bytecode X that is something other than a goto/goto_w, then the
// BytecodeBlock corresponding to bytecode X is the ultimate target.
// If the first goto/goto_w points to a chain of zero or more intermediate goto/goto_ws
// that completes a cycle of goto/goto_ws, then the first goto/goto_w whose target is
// either the first goto/goto_w or is a member of the chain becomes the ultimate target.
//
BytecodeBlock &BytecodeGraph::followGotos(Uint32 initialOffset, Int32 displacement, BytecodeBlock **blocks)
{
	// Create a unique marker to mark the locations of the gotos as we follow them.
	// If we ever encounter the same marker as a target of one of our gotos, we know
	// that the gotos form a cycle.
	BytecodeBlock *marker = reinterpret_cast<BytecodeBlock *>(endBlock);
	assert(marker);

	BytecodeBlock *finalBlock;
	Uint32 prevOffset = initialOffset;
	Uint32 offset = initialOffset + displacement;
	while (true) {
		assert(!blocks[prevOffset]);
		blocks[prevOffset] = marker;	// Mark the sources of the goto/goto_ws we've already seen.
		if (offset >= bytecodesSize)
			verifyError(VerifyError::badBytecodeOffset);
		BytecodeBlock *b = blocks[offset];
		if (b) {
			if (b == marker) {
				finalBlock = new(bytecodeGraphPool) BytecodeBlock(*this, bytecodesBegin + prevOffset, BasicBlock::snNoChange);
				blocks[prevOffset] = finalBlock;
			} else
				finalBlock = b;
			break;
		}
		prevOffset = offset;
		const bytecode *bc = bytecodesBegin + offset;
		bytecode e = *bc;
		if (e == bcGoto)
			// We don't bother to check that offset <= bytecodesSize-3 here because we'll check
			// that inside createBlocks; the worst that can happen here is that we'll read a few
			// bytes past the end of the method's bytecodes.
			offset += readBigSHalfwordUnaligned(bc+1);
		else if (e == bcGoto_W)
			// We don't bother to check that offset <= bytecodesSize-5 here because we'll check
			// that inside createBlocks; the worst that can happen here is that we'll read a few
			// bytes past the end of the method's bytecodes.
			offset += readBigSWordUnaligned(bc+1);
		else {
			finalBlock = &noteBlockBoundary(offset, blocks);
			break;
		}
	}

	// Convert all remaining markers into aliases of the ultimate target.
	offset = initialOffset;
	while (blocks[offset] == marker) {
		blocks[offset] = finalBlock;
		const bytecode *bc = bytecodesBegin + offset;
		bytecode e = *bc;
		if (e == bcGoto)
			offset += readBigSHalfwordUnaligned(bc+1);
		else {
			assert(e == bcGoto_W);
			offset += readBigSWordUnaligned(bc+1);
		}
	}
	assert(blocks[offset] == finalBlock);
	return *finalBlock;
}


//
// If the blocks array doesn't already contain a BytecodeBlock corresponding to the
// given bytecode offset, create a new BytecodeBlock at that offset and add it to the blocks array.
// Verify that the offset is less than bytecodesSize.
// Return the existing or created BytecodeBlock.
//
// After returnBlock is created any subsequent blocks starting with a return instruction are aliased
// to the existing returnBlock.  Gotos are aliased out (i.e. the source of a goto gets the same
// BytecodeBlock as the target of that goto); as a consequence no BytecodeBlock can contain a goto
// except in the case of a cycle consisting entirely of gotos.  A goto_w is considered to be a goto.
//
BytecodeBlock &BytecodeGraph::noteBlockBoundary(Uint32 offset, BytecodeBlock **blocks)
{
	if (offset >= bytecodesSize)
		verifyError(VerifyError::badBytecodeOffset);
	BytecodeBlock *&bb = blocks[offset];
	BytecodeBlock *b = bb;
	if (!b) {
		const bytecode *bc = bytecodesBegin + offset;
		bytecode e = *bc;
		if (e == bcGoto)
			// Make the goto's source BytecodeBlock to be an alias of its target BytecodeBlock.
			// We don't bother to check that offset <= bytecodesSize-3 here because we'll check
			// that inside createBlocks; the worst that can happen here is that we'll read a few
			// bytes past the end of the method's bytecodes.
			b = &followGotos(offset, readBigSHalfwordUnaligned(bc+1), blocks);

		else if (e == bcGoto_W)
			// Make the goto_w's source BytecodeBlock to be an alias of its target BytecodeBlock.
			// We don't bother to check that offset <= bytecodesSize-5 here because we'll check
			// that inside createBlocks; the worst that can happen here is that we'll read a few
			// bytes past the end of the method's bytecodes.
			b = &followGotos(offset, readBigSWordUnaligned(bc+1), blocks);

		else if (e == returnBytecode) {
			b = returnBlock;
			if (!b) {
				b = new(bytecodeGraphPool) BytecodeBlock(*this, bc, returnNormalizations[resultKind]);
				returnBlock = b;
			}
			bb = b;
		} else {
			b = new(bytecodeGraphPool) BytecodeBlock(*this, bc, BasicBlock::snNoChange);
			bb = b;
		}
	}
	return *b;
}


//
// Call noteBlockBoundary on the start_pc, end_pc, and handler_pc value of
// every exception handler in this function.  Create a CatchBlock for each handler.
// Add 1 to each entry in the activeHandlerDeltas array for each exception handler whose
// start_pc is at that entry's offset, and subtract 1 from each entry in the activeHandlerDeltas
// array for each exception handler whose end_pc is at that entry's offset.
//
void BytecodeGraph::noteExceptionBoundaries(BytecodeBlock **blocks, Int32 *activeHandlerDeltas)
{
	const ExceptionItem *e = exceptionEntries;
	const ExceptionItem *eEnd = e + nExceptionEntries;
	while (e != eEnd) {
		Uint32 startPC = e->startPc;
		Uint32 endPC = e->endPc;
		if (startPC >= endPC)
			verifyError(VerifyError::badBytecodeOffset);
		noteBlockBoundary(startPC, blocks);
		activeHandlerDeltas[startPC]++;
		if (endPC != bytecodesSize)
			noteBlockBoundary(endPC, blocks);
		activeHandlerDeltas[endPC]--;
		BytecodeBlock &handler = noteBlockBoundary(e->handlerPc, blocks);
		if (!handler.catchBlock)
			handler.catchBlock = new(bytecodeGraphPool) CatchBlock(*this, handler);
		e++;
	}
}


//
// Call noteBlockBoundary on every target of this tableswitch instruction.
// bc points to the tableswitch instruction's opcode.
// Return the address of the following instruction.
//
const bytecode *BytecodeGraph::noteTableSwitchTargets(const bytecode *bc, BytecodeBlock **blocks)
{
	Uint32 baseOffset = bc - bytecodesBegin;
	const bytecode *bcDefault = switchAlign(bc + 1);	// Skip past padding.
	noteBlockBoundary(baseOffset + readBigSWord(bcDefault), blocks);

	Int32 low = readBigSWord(bcDefault + 4);
	Int32 high = readBigSWord(bcDefault + 8);
	if (low > high)
		verifyError(VerifyError::badSwitchBytecode);
	bcDefault += 12;
	Uint32 nCases = high - low + 1;
	// The nCases == 0 check ensures that we don't have overflow from low = 0x80000000 and high = 0x7FFFFFFF.
	if (bcDefault > bytecodesEnd || nCases == 0 || nCases > (Uint32)(bytecodesEnd - bcDefault) >> 2)
		verifyError(VerifyError::badBytecodeOffset);
	while (nCases--) {
		noteBlockBoundary(baseOffset + readBigSWord(bcDefault), blocks);
		bcDefault += 4;
	}
	return bcDefault;
}


//
// Call noteBlockBoundary on every target of this lookupswitch instruction.
// bc points to the lookupswitch instruction's opcode.
// Return the address of the following instruction.
//
const bytecode *BytecodeGraph::noteLookupSwitchTargets(const bytecode *bc, BytecodeBlock **blocks)
{
	Uint32 baseOffset = bc - bytecodesBegin;
	const bytecode *bcDefault = switchAlign(bc + 1);	// Skip past padding.
	noteBlockBoundary(baseOffset + readBigSWord(bcDefault), blocks);

	Int32 nPairs = readBigSWord(bcDefault + 4);
	if (nPairs < 0)
		verifyError(VerifyError::badSwitchBytecode);
	bcDefault += 8;
	if (bcDefault > bytecodesEnd || nPairs > (bytecodesEnd - bcDefault) >> 3)
		verifyError(VerifyError::badBytecodeOffset);
	// ****** TO DO: verify that each target match value is unique *******
	while (nPairs--) {
		noteBlockBoundary(baseOffset + readBigSWord(bcDefault + 4), blocks);
		bcDefault += 8;
	}
	return bcDefault;
}


//
// Allocate an array of the target BasicBlocks of this tableswitch instruction.
// bc points to the tableswitch instruction's opcode.
// Return the address of the following instruction, the array of BasicBlocks in
// successors, and the size of this array (not including the nActiveHandlers exception
// handlers at its end) in nSuccessors.  Do not initialize the successor array entries
// corresponding to the exception handlers.
//
const bytecode *BytecodeGraph::recordTableSwitchTargets(const bytecode *bc, BytecodeBlock **blocks,
			BasicBlock **&successors, Uint32 &nSuccessors, Int32 nActiveHandlers)
{
	const bytecode *bcDefault = switchAlign(bc + 1);	// Skip past padding.

	Int32 low = readBigSWord(bcDefault + 4);
	Int32 high = readBigSWord(bcDefault + 8);
	assert(low <= high);
	Uint32 nCases = high - low + 1;
	nSuccessors = nCases + 1;
	BasicBlock **s = new(bytecodeGraphPool) BasicBlock *[nCases + 1 + nActiveHandlers];
	successors = s;

	BytecodeBlock *bb = blocks[bc + readBigSWord(bcDefault) - bytecodesBegin];
	assert(bb);
	*s++ = bb;
	bcDefault += 12;
	while (nCases--) {
		bb = blocks[bc + readBigSWord(bcDefault) - bytecodesBegin];
		assert(bb);
		*s++ = bb;
		bcDefault += 4;
	}
	return bcDefault;
}


//
// Allocate an array of the target BasicBlocks of this lookupswitch instruction.
// bc points to the lookupswitch instruction's opcode.
// Return the address of the following instruction, the array of BasicBlocks in
// successors, and the size of this array (not including the nActiveHandlers exception
// handlers at its end) in nSuccessors.  Do not initialize the successor array entries
// corresponding to the exception handlers.
//
const bytecode *BytecodeGraph::recordLookupSwitchTargets(const bytecode *bc, BytecodeBlock **blocks,
			BasicBlock **&successors, Uint32 &nSuccessors, Int32 nActiveHandlers)
{
	const bytecode *bcDefault = switchAlign(bc + 1);	// Skip past padding.

	Int32 nPairs = readBigSWord(bcDefault + 4);
	assert(nPairs >= 0);
	nSuccessors = nPairs + 1;
	BasicBlock **s = new(bytecodeGraphPool) BasicBlock *[nPairs + 1 + nActiveHandlers];
	successors = s;

	BytecodeBlock *bb = blocks[bc + readBigSWord(bcDefault) - bytecodesBegin];
	assert(bb);
	*s++ = bb;
	bcDefault += 8;
	while (nPairs--) {
		bb = blocks[bc + readBigSWord(bcDefault + 4) - bytecodesBegin];
		assert(bb);
		*s++ = bb;
		bcDefault += 8;
	}
	return bcDefault;
}


//
// Initialize the handler successors of each BasicBlock in this function according
// to the start_pc, end_pc, and handler_pc values of every exception handler in this
// function.  Memory for the handler pointers (as indicated by the BasicBlocks'
// successorsPhysicalEnd fields) and for the exceptionClasses arrays must have been allocated.
// On entry each BasicBlock's handlersEnd value should be equal to its successorsEnd;
// on exit the handlersEnd value will be advanced past all of that block's handler pointers.
//
void BytecodeGraph::recordHandlerTargets(BytecodeBlock **blocks)
{
	const bytecode *const bytecodesBegin = BytecodeGraph::bytecodesBegin;
	const ExceptionItem *e = exceptionEntries;
	const ExceptionItem *eEnd = e + nExceptionEntries;
	while (e != eEnd) {
		Uint32 pc = e->startPc;
		Uint32 endPC = e->endPc;
		BytecodeBlock *handlerBytecodeBlock = blocks[e->handlerPc];
		assert(handlerBytecodeBlock);
		CatchBlock *handler = handlerBytecodeBlock->catchBlock;
		assert(handler);
		ConstantPoolIndex c = e->catchType;
		const Class *filter = &Standard::get(cThrowable);
		if (c) {
			filter = &classFileSummary.lookupClass(c);
			if (!filter->implements(Standard::get(cThrowable)))
				verifyError(VerifyError::nonThrowableCatch);
		}

		do {
			BytecodeBlock *block = blocks[pc];
			assert(block);
			if (block->bytecodesBegin == bytecodesBegin + pc) {
				// Add the handler to the list of this block's handlers.
				if (!block->handlersForbidden()) {
					assert(block->handlersEnd < block->successorsPhysicalEnd);
					*block->handlersEnd++ = handler;
					*block->exceptionClassesEnd++ = filter;
				}
				// Skip to the next BytecodeBlock
				pc = block->bytecodesEnd - bytecodesBegin;
			} else {
				// We have a forwarded return, goto, or goto_w bytecode.
				const BytecodeControlInfo &bci = normalBytecodeControlInfos[bytecodesBegin[pc]];
				assert(bci.kind == BytecodeControlInfo::bckGoto ||
					   bci.kind == BytecodeControlInfo::bckGoto_W ||
					   bci.kind == BytecodeControlInfo::bckReturn);
				pc += bci.size;
			}
			assert(pc <= endPC);
		} while (pc != endPC);
		e++;
	}
}


//
// Verify that no bytecode basic blocks begin between bc1 and bc2, exclusive.
// This ensures that nothing jumps into the middle of a bytecode instruction.
//
inline void BytecodeGraph::verifyNoBoundariesBetween(const bytecode *bc1, const bytecode *bc2, BytecodeBlock **blocks) const
{
	assert(bc1 < bc2);
	BytecodeBlock **bb1 = blocks + (bc1 - bytecodesBegin);
	BytecodeBlock **bb2 = blocks + (bc2 - bytecodesBegin);
	for (bb1++; bb1 != bb2; bb1++)
		if(*bb1)
			verifyError(VerifyError::badBytecodeOffset);
}


//
// Create the BasicBlocks for this function and initialize their links to
// each other and their pointers to bytecodes.  Initialize beginBlock and endBlock
// and set returnBlock if present.
// Allocate temporary storage that can be discarded after this function finishes
// from tempPool.
//
void BytecodeGraph::createBlocks(Pool &tempPool)
{
	// Entry i of the blocks array will contain either:
	//    null if a BytecodeBlock does not begin at bytecode offset i, or
	//    a pointer to a BytecodeBlock that begins at bytecode offset i, or
	//    a pointer to the return BytecodeBlock if one of the return instructions is present at bytecode
	//       offset i and that is not the last return instruction in the function  (Only a one-byte-long
	//       return instruction can be forwarded in this manner), or
	//    a pointer to a BytecodeBlock that is the ultimate target of a goto (or goto_w) or a chain of
	//       gotos or goto_ws that begins at bytecode offset i.
	// Every conditional, branch or exception target, or beginning or end of
	// a range protected by an exception handler introduces a bytecode block
	// boundary.
	// We can detect the third and fourth cases above (a forwarded return or goto BytecodeBlock) by
	// checking whether the BytecodeBlock's bytecodesBegin corresponds to the index i of its blocks array entry.
	BytecodeBlock **blocks = new(tempPool) BytecodeBlock *[bytecodesSize];
	BytecodeBlock **blocksEnd = blocks + bytecodesSize;
	// Clear the blocks array.
	for (BytecodeBlock **bb = blocks; bb != blocksEnd; bb++)
		*bb = 0;

	// Entry i of the activeHandlerDeltas array will contains the difference between the
	// number of exception handlers that apply to the bytecode instruction just before
	// bytecode offset i (zero if i is zero) and the number of exception handlers that
	// apply to the bytecode instruction at bytecode offset i.
	Int32 *activeHandlerDeltas = new(tempPool) Int32[bytecodesSize+1];
	Int32 *activeHandlerDeltasEnd = activeHandlerDeltas + bytecodesSize + 1;
	// Clear the activeHandlerDeltas array.
	for (Int32 *rl = activeHandlerDeltas; rl != activeHandlerDeltasEnd; rl++)
		*rl = 0;

	const bytecode *const bytecodesBegin = BytecodeGraph::bytecodesBegin;
	const bytecode *const bytecodesEnd = BytecodeGraph::bytecodesEnd;

	// Allocate the end block.
	endBlock = new(bytecodeGraphPool) EndBlock(*this);

	// First pass:
	// Collect all branch destinations and statements after conditional branches
	// and make sure that each starts a BytecodeBlock.  At this time only fill
	// in the bytecodesBegin fields of the newly created BasicBlocks.
	hasJSRs = false;
	noteBlockBoundary(0, blocks);	// Allocate the first basic block.
	const bytecode *bc = bytecodesBegin;
	while (true) { // We know there must be at least one bytecode.
		if (bc >= bytecodesEnd)
			verifyError(VerifyError::badBytecodeOffset);
		Uint32 baseOffset = bc - bytecodesBegin;
		const BytecodeControlInfo &bci = getBytecodeControlInfo(bc);
		bc += bci.size;

		switch (bci.kind) {
			case BytecodeControlInfo::bckNormal:
			case BytecodeControlInfo::bckExc:
				continue;	// Don't force a new BytecodeBlock to start right after this bytecode.

			case BytecodeControlInfo::bckGoto:
				assert(bci.size == 3);
				if (!blocks[baseOffset])
					followGotos(baseOffset, readBigSHalfwordUnaligned(bc-2), blocks);
				break;
			case BytecodeControlInfo::bckGoto_W:
				assert(bci.size == 5);
				if (!blocks[baseOffset])
					followGotos(baseOffset, readBigSWordUnaligned(bc-4), blocks);
				break;

			case BytecodeControlInfo::bckJsr:
				hasJSRs = true;
				// Put the jsr into its own block.
				noteBlockBoundary(baseOffset, blocks);
			case BytecodeControlInfo::bckIf:
				assert(bci.size == 3);
				noteBlockBoundary(baseOffset + readBigSHalfwordUnaligned(bc-2), blocks);
				break;
			case BytecodeControlInfo::bckJsr_W:
				hasJSRs = true;
				// Put the jsr_w into its own block.
				noteBlockBoundary(baseOffset, blocks);
				assert(bci.size == 5);
				noteBlockBoundary(baseOffset + readBigSWordUnaligned(bc-4), blocks);
				break;

			case BytecodeControlInfo::bckTableSwitch:
				assert(bci.size == 1);
				bc = noteTableSwitchTargets(bc-1, blocks);
				break;
			case BytecodeControlInfo::bckLookupSwitch:
				assert(bci.size == 1);
				bc = noteLookupSwitchTargets(bc-1, blocks);
				break;

			case BytecodeControlInfo::bckReturn:
				assert(bci.size == 1);
				// Ensure that all return instructions in this method return the correct kind.
				if (bc[-1] != returnBytecode)
					verifyError(VerifyError::badReturn);
				// noteBlockBoundary handles and combines return blocks
				noteBlockBoundary(baseOffset, blocks);
				break;

			case BytecodeControlInfo::bckThrow:
				break;

			case BytecodeControlInfo::bckRet:
			case BytecodeControlInfo::bckRet_W:
				// Put the ret or ret_w into its own block.
				noteBlockBoundary(baseOffset, blocks);
				break;

			default:
				verifyError(VerifyError::badBytecode);
		}
		// A new BytecodeBlock starts right after this bytecode.
		if (bc == bytecodesEnd)
			break;
		noteBlockBoundary(bc - bytecodesBegin, blocks);
	}

	// Note the boundaries of try blocks and start addresses of exception handlers.
	// Also calculate the activeHandlerDeltas array values.
	noteExceptionBoundaries(blocks, activeHandlerDeltas);

	// Second pass:
	// Fill in the bytecodesEnd, successorsBegin, successorsEnd, and successors of the
	// BasicBlocks.  Initialize handlersEnd to be the same as successorsEnd for now.
	BytecodeBlock *thisBlock = blocks[0];
	assert(thisBlock);
	Int32 nActiveHandlers = activeHandlerDeltas[0];
	bc = bytecodesBegin;
	while (true) {
		assert(bc < bytecodesEnd && nActiveHandlers >= 0);
		const BytecodeControlInfo &bci = getBytecodeControlInfo(bc);
		uint instSize = bci.size;
		if (instSize != 1)
			verifyNoBoundariesBetween(bc, bc + instSize, blocks);
		bc += instSize;
		Uint32 bcOffset = bc - bytecodesBegin;

		BasicBlock **successors;
		Uint32 nSuccessors;
		BasicBlock *bb1;
		BasicBlock *bb2;

		switch (bci.kind) {
			case BytecodeControlInfo::bckNormal:
			case BytecodeControlInfo::bckExc:
				// Continue processing this BytecodeBlock if it's not finished.
				bb1 = blocks[bcOffset];
				if (!bb1)
					continue;
			  processOneSuccessor:
				assert(bb1);
				successors = new(bytecodeGraphPool) BasicBlock *[1 + nActiveHandlers];
				successors[0] = bb1;
				nSuccessors = 1;
				break;

			case BytecodeControlInfo::bckThrow:
				successors = nActiveHandlers ? new(bytecodeGraphPool) BasicBlock *[nActiveHandlers] : 0;
				nSuccessors = 0;
				break;

			case BytecodeControlInfo::bckGoto:
				assert(instSize == 3 && blocks[bcOffset - 3]);
				// Don't generate anything for this goto if it's an alias.
				if (blocks[bcOffset - 3]->bytecodesBegin != bc-3)
					// thisBlock is an alias to the target of the goto, which is somewhere else.
					goto startNewBlock;
				bb1 = blocks[bcOffset - 3 + readBigSHalfwordUnaligned(bc-2)];
				goto processOneSuccessor;

			case BytecodeControlInfo::bckGoto_W:
				assert(instSize == 5 && blocks[bcOffset - 5]);
				// Don't generate anything for this goto_w if it's an alias.
				if (blocks[bcOffset - 5]->bytecodesBegin != bc-5)
					// thisBlock is an alias to the target of the goto_w, which is somewhere else.
					goto startNewBlock;
				bb1 = blocks[bcOffset - 5 + readBigSWordUnaligned(bc-4)];
				goto processOneSuccessor;

			case BytecodeControlInfo::bckIf:
				bb1 = blocks[bcOffset];
				assert(instSize == 3);
				bb2 = blocks[bcOffset - 3 + readBigSHalfwordUnaligned(bc-2)];
				assert(bb1 && bb2);
				successors = new(bytecodeGraphPool) BasicBlock *[2 + nActiveHandlers];
				successors[0] = bb1;
				successors[1] = bb2;
				nSuccessors = 2;
				break;

			case BytecodeControlInfo::bckTableSwitch:
				assert(instSize == 1);
				bc = recordTableSwitchTargets(bc-1, blocks, successors, nSuccessors, nActiveHandlers);
				bcOffset = bc - bytecodesBegin;
				break;

			case BytecodeControlInfo::bckLookupSwitch:
				assert(instSize == 1);
				bc = recordLookupSwitchTargets(bc-1, blocks, successors, nSuccessors, nActiveHandlers);
				bcOffset = bc - bytecodesBegin;
				break;

			case BytecodeControlInfo::bckReturn:
				assert(instSize == 1 && blocks[bcOffset - 1]);
				// Don't generate anything for this return if it's an alias.
				if (thisBlock->bytecodesBegin != bc-1)
					// thisBlock is an alias to the true return block, which is somewhere else.
					goto startNewBlock;
				// The last return jumps to the end block.
				thisBlock->subkind = BytecodeBlock::skReturn;		 // Forbid handlers here to help avoid creating false loops;
				successors = new(bytecodeGraphPool) BasicBlock *[1]; // we can therefore also omit space for handlers.
				successors[0] = endBlock;
				nSuccessors = 1;
				break;

			case BytecodeControlInfo::bckJsr:
				assert(instSize == 3);
				bb1 = blocks[bcOffset - 3 + readBigSHalfwordUnaligned(bc-2)];
			  processJSR:
				bb2 = blocks[bcOffset];
				assert(bb1 && bb2);
				thisBlock->subkind = BytecodeBlock::skJsr;			 // Forbid handlers here;
				successors = new(bytecodeGraphPool) BasicBlock *[2]; // we can therefore also omit space for handlers.
				successors[0] = bb1;
				successors[1] = bb2;
				nSuccessors = 2;
				break;
			case BytecodeControlInfo::bckJsr_W:
				assert(instSize == 5);
				bb1 = blocks[bcOffset - 5 + readBigSWordUnaligned(bc-4)];
				goto processJSR;

			case BytecodeControlInfo::bckRet:
			case BytecodeControlInfo::bckRet_W:
				thisBlock->subkind = BytecodeBlock::skRet;	// Forbid handlers here;
				successors = 0;								// we can therefore also omit space for handlers.
				nSuccessors = 0;							// We don't know the actual successors of the ret yet.
				break;

			default:
				trespass("Reached bad BytecodeControlInfo");
		}

		{// Braces needed because Visual C++ complains about the bc2 variable without them.
			// Finish the previous BytecodeBlock.
			thisBlock->initSuccessors(successors, nSuccessors, nActiveHandlers, bytecodeGraphPool);
			thisBlock->bytecodesEnd = bc;
		  #ifdef DEBUG
			// Assert that we defined BytecodeBlocks in such a way as to make
			// exception ranges begin and end only at BytecodeBlock boundaries.
			for (const bytecode *bc2 = thisBlock->bytecodesBegin; ++bc2 != bc; )
				assert(activeHandlerDeltas[bc2 - bytecodesBegin] == 0);
		  #endif
		}

	  startNewBlock:
		// Start a new BytecodeBlock.
		nActiveHandlers += activeHandlerDeltas[bcOffset];
		if (bc == bytecodesEnd)
			break;
		thisBlock = blocks[bcOffset];
		assert(thisBlock);
	}
	// The counts of exception range begins and ends must be equal.
	assert(nActiveHandlers == 0);

	// Third pass:
	// Fill in the handler successors of the BasicBlocks.
	recordHandlerTargets(blocks);

	beginBlock = blocks[0];
	assert(beginBlock);
}


class BytecodeDFSHelper
{
  public:
	typedef BasicBlock *Successor;
	typedef BasicBlock *NodeRef;

	bool hasBackEdges;							// True if the graph contains a cycle
	bool hasBackExceptionEdges;					// True if the graph contains a cycle
  private:
	BasicBlock **dfsList;						// Alias to dfsList from the BytecodeGraph

  public:
	BytecodeDFSHelper(BasicBlock **dfsList): hasBackEdges(false), hasBackExceptionEdges(false), dfsList(dfsList) {}

	static Successor *getSuccessorsBegin(NodeRef n) {return n->successorsBegin;}
	static Successor *getSuccessorsEnd(NodeRef n) {return n->handlersEnd;}
	static bool isNull(const Successor s) {return s == 0;}
	static NodeRef getNodeRef(Successor s) {return s;}
	static bool isMarked(NodeRef n) {return n->dfsNum != BasicBlock::unmarked;}
	static bool isUnvisited(NodeRef n) {return n->dfsNum == BasicBlock::unvisited;}
	static bool isNumbered(NodeRef n) {return n->dfsNum >= 0;}
	static void setMarked(NodeRef n) {n->dfsNum = BasicBlock::unvisited;}
	static void setVisited(NodeRef n) {n->dfsNum = BasicBlock::unnumbered;}
	void setNumbered(NodeRef n, Int32 i) {assert(i >= 0); n->dfsNum = i; dfsList[i] = n;}
	void notePredecessor(NodeRef n) {n->nPredecessors++;}
	void noteIncomingBackwardEdge(NodeRef);
};

//
// depthFirstSearchWithEnd calls this when it notices that node n has an incoming
// backward edge.  Note the fact that this graph has a cycle by setting hasBackEdges.
// If this node is a catch node, also note the fact that we will need a second pass
// to split this catch node so as to make all exception edges go forward only.
//
inline void BytecodeDFSHelper::noteIncomingBackwardEdge(NodeRef n)
{
	hasBackEdges = true;
	if (!n->hasKind(BasicBlock::bbBytecode))
		hasBackExceptionEdges = true;
  #ifdef DEBUG
	n->hasIncomingBackEdges = true;
  #endif
}


//
// Allocate dfsList and fill it out with pointers to the BasicBlocks ordered
// by depth-first search.  Fill out each BasicBlock's dfsNum to be its index
// into dfsList.  Set or clear hasBackEdges depending on whether the graph has
// any back edges.  Return true if the graph has backward exception edges.
//
// tempPool is a pool for temporary allocations that are no longer needed after
// depthFirstSearch terminates.
//
bool BytecodeGraph::depthFirstSearch(Pool &tempPool)
{
	// Initialize the blocks.
	Uint32 maxNBlocks = 0;
	for (DoublyLinkedList<BasicBlock>::iterator i = blocks.begin(); !blocks.done(i); i = blocks.advance(i)) {
		BasicBlock &n = blocks.get(i);
		n.dfsNum = BasicBlock::unmarked;
		n.nPredecessors = 0;
	  #ifdef DEBUG
		n.hasIncomingBackEdges = false;
	  #endif
		maxNBlocks++;
	}

	// Count the blocks reachable from beginBlock.
	assert(beginBlock);
	dfsList = new(bytecodeGraphPool) BasicBlock *[maxNBlocks];
	BytecodeDFSHelper dfsHelper(dfsList);
	SearchStackEntry<BasicBlock *> *searchStack = new(tempPool) SearchStackEntry<BasicBlock *>[maxNBlocks];
	BasicBlock *begin = beginBlock;
	BasicBlock *end = endBlock;
	nBlocks = graphSearchWithEnd(dfsHelper, begin, end, maxNBlocks, searchStack);

	// Now do the actual depth-first search.
	depthFirstSearchWithEnd(dfsHelper, begin, end, nBlocks, searchStack);
	hasBackEdges = dfsHelper.hasBackEdges;
	return dfsHelper.hasBackExceptionEdges;
}


//
// Split CatchBlocks with incoming backward edges into multiple CatchBlocks
// with a separate CatchBlock for each incoming backward edge.  This should
// eliminate any backward exception edges from the graph.
//
void BytecodeGraph::splitCatchNodes()
{
	BasicBlock **block = dfsList;
	BasicBlock **dfsListEnd = block + nBlocks;
	while (block != dfsListEnd) {
		BasicBlock *b = *block;
		Int32 dfsNum = b->dfsNum;
		BasicBlock **handler = b->successorsEnd;
		BasicBlock **handlersEnd = b->handlersEnd;
		while (handler != handlersEnd) {
			BasicBlock *h = *handler;
			if (dfsNum >= h->dfsNum) {
				// We have a backward exception edge from b to h.
				// Make a copy of h and point this edge to that copy.
				assert(h->hasKind(BasicBlock::bbCatch));
				*handler = new(bytecodeGraphPool) CatchBlock(*this, static_cast<CatchBlock *>(h)->getHandler());
			}
			handler++;
		}
		block++;
	}
}


//
// Create the BasicBlocks for this function and then call depthFirstSearch.
// tempPool is a pool for temporary allocations that are no longer needed after
// divideIntoBlocks terminates.
//
void BytecodeGraph::divideIntoBlocks(Pool &tempPool)
{
	if (!bytecodesSize)
		verifyError(VerifyError::noBytecodes);
	createBlocks(tempPool);
	if (true /******* hasJSRs ***********/) {
		depthFirstSearch(tempPool);
		BytecodeVerifier::inlineSubroutines(*this, tempPool);
	}
  #ifdef DEBUG
	bool didSplit = false;
  #endif
	while (depthFirstSearch(tempPool)) {
		// We have some backward exception edges.  Split catch nodes to eliminate them.
		splitCatchNodes();
	  #ifdef DEBUG
		// We expect to only have to run the split phase at most once given the current
		// deterministic implementation of depth-first search; however, if the depth-first
		// search becomes nondeterministic, this assumption may no longer hold for
		// irreducible graphs, so we put splitCatchNodes into a loop and add asserts
		// to warn about the weird case where calling it once isn't enough.
		assert(!didSplit);
		didSplit = true;
	  #endif
	}
}


#ifdef DEBUG_LOG
//
// Print a BytecodeGraph for debugging purposes.
// If c is non-nil, disassemble constant pool indices symbolically using the information
// in c.
//
void BytecodeGraph::print(LogModuleObject &f, const ConstantPool *c) const
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("BytecodeGraph %p\n", this));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("isInstanceMethod: %d\n", isInstanceMethod));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("Arguments: "));
	bool printSeparator = false;
	const ValueKind *kEnd = argumentKinds + nArguments;
	for (const ValueKind *k = argumentKinds; k != kEnd; k++) {
		if (printSeparator)
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, (", "));
		printSeparator = true;
		::print(f, *k);
	}
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\nResult: "));
	::print(f, resultKind);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\nhasBackEdges: %d\n", hasBackEdges));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\nhasJSRs: %d\n", hasJSRs));
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("beginBlock: "));
	beginBlock->printRef(f, *this);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\nendBlock: "));
	endBlock->printRef(f, *this);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n%d BasicBlocks:\n\n", nBlocks));
	if (dfsList)
		for (Uint32 i = 0; i != nBlocks; i++)
			dfsList[i]->printPretty(f, *this, c, 4);
	else
		for (DoublyLinkedList<BasicBlock>::iterator i = blocks.begin(); !blocks.done(i); i = blocks.advance(i))
			blocks.get(i).printPretty(f, *this, c, 4);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("End BytecodeGraph\n\n"));
}
#endif
