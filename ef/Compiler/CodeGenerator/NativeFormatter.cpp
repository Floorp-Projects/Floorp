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
//
//	NativeFormatter.cpp
//	
//	Scott M. Silver
//	
//	Actually output native code for a function to memory (from the NativeCodeCache).
//
// Summary
//
// A NativeFormatter takes an InstructionEmitter (a per/method data structure) and a Method
// (a runtime representation of a method) and actually outputs the native representation
// of the function out to memory.  Memory is acquired from the NativeCodeCache.
//
// Each function has the following structural layout:
//
// ControlNodes				Formatted Correlation
//							PreMethod
// ckBegin					Prolog
// ckReturn					Epilog
// ckEnd					PostMethod
//
// Prolog and Epilog are normal entry and exit points from a function.  Note that even
// if a function does not return a value a ckReturn node must exist.
// PreMethod and PostMethod are invented pieces of code/data which may be used
// for debuggers or stack inspection (eg. MacsBug symbols, TraceBack tables, etc...)

#include "NativeFormatter.h"
#include "ControlGraph.h"
#include "InstructionEmitter.h"
#include "NativeCodeCache.h"
#include "ExceptionTable.h"

#ifdef DEBUG

#include "JavaVM.h"	// to insert breakpoints in jitted code

#endif

UT_DEFINE_LOG_MODULE(ExceptionMatrix);

//-----------------------------------------------------------------------------------------------------------
// format
//
// Actually output the native instructions of a method out to memory
// acquired from the cache.  
//
// outInfo: Normally temporal information about the formatting the function
// 			mainly used for debugging purposes
void* NativeFormatter::
format(Method& inMethod, FormattedCodeInfo*	outInfo)
{
	// now setup a FormattedCodeInfo with collected data about the method
	FormattedCodeInfo	fci;
	fci.method = &inMethod;

	// Need policy before any formatting can be done
	mFormatter.initStackFrameInfo();

	mFormatter.calculatePrologEpilog(inMethod, fci.prologSize, fci.epilogSize);
	mFormatter.calculatePrePostMethod(inMethod, fci.preMethodSize, fci.postMethodSize);
	
	// potentially shortens or lengthens branches
	fci.bodySize = resolveBranches(fci.prologSize, fci.epilogSize);
		
	// actually grab some memory for this method
	Uint32	methodSize = fci.preMethodSize + fci.bodySize + fci.postMethodSize;

#ifdef DEBUG
#if defined(XP_PC) || defined(LINUX)
    DebugDesc *bps;
    Uint32 nbps;    
    nbps = VM::theVM.getExecBreakPoints(bps);
    bool hasInt3 = false;
    bool foundBreakPoint = false;

    for (Uint32 index = 0; index < nbps; index++)
        if (inMethod.isSelf(bps[index].className, bps[index].methodName, bps[index].sig)) {
            methodSize++;  // for int 3
            fci.methodStart = NativeCodeCache::getCache().acquireMemory(methodSize);
            *fci.methodStart++ = 0xcc;
            hasInt3 = true;
            foundBreakPoint = true;
            break;
        }
    
    if (!foundBreakPoint)
        fci.methodStart = NativeCodeCache::getCache().acquireMemory(methodSize);

#else
    fci.methodStart = NativeCodeCache::getCache().acquireMemory(methodSize);
#endif
#else
	fci.methodStart = NativeCodeCache::getCache().acquireMemory(methodSize);
#endif

	// build the exception table
	// FIX FIX FIX
	// for now just grab some memory
	Pool* tempPool = new Pool();
	ExceptionTable& eTable = buildExceptionTable(fci.methodStart, tempPool);
	DEBUG_LOG_ONLY(eTable.print(UT_LOG_MODULE(ExceptionMatrix)));
	DEBUG_LOG_ONLY(eTable.printFormatted(UT_LOG_MODULE(ExceptionMatrix), nodes, nNodes, false));
	
	// fci is now valid
	mFormatter.beginFormatting(fci);

	// dump out the function to memory
	outputNativeToMemory(fci.methodStart, fci);

	// make an entry point for this function
	MethodDescriptor	md(inMethod);
	
	mFormatter.endFormatting(fci);

#if defined(DEBUG) && (defined(XP_PC) || defined(LINUX))
	if (hasInt3)
		fci.methodStart--;
#endif

	PlatformTVector functionDescriptor;
	functionDescriptor.setTVector(mFormatter.createTransitionVector(fci));

	fci.methodEnd = fci.methodStart + methodSize;
#ifdef WIN32
	// temporarily check this invariant
	assert((fci.methodStart + methodSize) == (functionDescriptor.getFunctionAddress() + methodSize));
#endif

	// Eventually we may wish to have choose different GPR/FPR save procedures and local store sizes for
	// different nodes in the control graph. Correspondingly the restore policies in an exception unwind
	// can vary by control node -- ie they are not necessarily the same across the whole method.
	// For now we will give each method the same info, but we reserve the right later to make this optimisation.

	// FIX for mow, NativeCodeCache::mapMemoryToMethod caches a copy of the policy
	StackFrameInfo& policy = mFormatter.getStackFrameInfo();
	NativeCodeCache::getCache().mapMemoryToMethod(md, functionDescriptor, fci.methodEnd, &eTable, policy);

	if (outInfo)
	  *outInfo = fci;

#ifdef DEBUG_LOG	
	// debugging stuff
	if(VM::theVM.getEmitHTML())
		dumpMethodToHTML(fci, eTable);
#endif

	// return the entry point
	return (fci.methodStart);
}

// outputNativeToMemory
// actually ouput this method to memory starting at inWhere.
// inInfo contains some precalculated information about the method
// which is useful in its formatting.
void NativeFormatter::
outputNativeToMemory(void* inWhere, const FormattedCodeInfo& inInfo)
{
	Uint32			curOffset;
	char*			nextMemory = (char*) inWhere;	

	assert(nodes[0]->hasControlKind(ckBegin));
	assert(nodes[nNodes-1]->hasControlKind(ckEnd));
	
	// first output the PreMethod
	mFormatter.formatPreMethodToMemory(nextMemory, inInfo);
	nextMemory += inInfo.preMethodSize;
	
	mFormatter.formatPrologToMemory(nextMemory);
	nextMemory += inInfo.prologSize;	
	curOffset = inInfo.prologSize;	
	
	for (Uint32 n = 0; n < nNodes; n++)
	{
		InstructionList& instructions = nodes[n]->getInstructions();
		for(InstructionList::iterator i = instructions.begin(); !instructions.done(i); i = instructions.advance(i))
		{
			Instruction&	curInstruction = instructions.get(i);

			curInstruction.formatToMemory(nextMemory, curOffset, mFormatter);
			curOffset += curInstruction.getFormattedSize(mFormatter);
			nextMemory += curInstruction.getFormattedSize(mFormatter);
		}

		if (nodes[n]->hasControlKind(ckReturn))
		{			mFormatter.formatEpilogToMemory(nextMemory);
			curOffset += inInfo.epilogSize;
			nextMemory += inInfo.epilogSize;	
		}
	}	

	// Now end with the PostMethod
	mFormatter.formatPostMethodToMemory(nextMemory, inInfo);
	//nextMemory += inInfo.preMethodSize;	
}


// accumulateSize
//
// Accumulate the size of the the formatted (ie in-memory) representation
// of each Instruction in this ControlNode.  ckBegin and ckReturn nodes
// include the prolog and epilog size, respectively.
Uint32 NativeFormatter::
accumulateSize(ControlNode& inNode, Uint32 inPrologSize, Uint32 inEpilogSize)
{
	Uint32 accum;

	InstructionList& instructions = inNode.getInstructions();

	accum = 0;
	for(InstructionList::iterator i = instructions.begin(); !instructions.done(i); i = instructions.advance(i))
		accum += instructions.get(i).getFormattedSize(mFormatter);

	if (inNode.hasControlKind(ckReturn))
		accum += inEpilogSize;	
	else if (inNode.hasControlKind(ckBegin))
		accum += inPrologSize;
		
	return (accum);
}

// resolveBranches
//
// for each node in the scheduled ControlNodes
// assign a native offset to each ControlNode.
// decisions to make short/long branches would normally be made
// here.  returns the size of the formatted method. including
// epilog and prolog.  The PreMethod and PostMethod are not
// considered here.
Uint32 NativeFormatter::
resolveBranches(Uint32 inPrologSize, Uint32 inEpilogSize)
{
	Uint32			accumSize;
	
	accumSize = 0;
	for (Uint32 i = 0; i < nNodes; i++)
	{
		ControlNode& node = *nodes[i];
		Uint32	formattedSize;

		node.setNativeOffset(accumSize);		
		// fix-me this should be a function of the list, ie the list 
		// should maintain the size of the instructions in it
		formattedSize = accumulateSize(node, inPrologSize, inEpilogSize);
		accumSize += formattedSize;
	}

	// make long/short branch decisions here
	return (accumSize);
}

// buildExceptionTable
// In:	start of method
//		pool to create table in	
// Out:	the Exception Table

// #define DEBUG_GENERATE_EXCEPTION_MATRIX

// Debugging code to print out a matrix of exception handlers
// Ugly -- farm out to a class, time permitting
#ifdef DEBUG_GENERATE_EXCEPTION_MATRIX

#define	DEBUG_SET_UP_EXCEPTION_MATRIX			\
	UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, ("\n\nBuilding Exception Table\n"));	\
	assert(nNodes < 100);	/* dumb check */	\
	int _exceptionMatrix[100][100];				\
	Uint32 _rows, _cols;						\
	for(_rows = 0; _rows < nNodes; _rows++)		\
		for(_cols = 0; _cols < nNodes; _cols++)	\
			_exceptionMatrix[_rows][_cols] = 0;	\
	Uint32 _addorder = 1;

#define	DEBUG_ADD_ENTRY_TO_EXCEPTION_MATRIX		\
	_exceptionMatrix[node->dfsNum][e->getTarget().dfsNum] = _addorder++;

#define	DEBUG_PRINT_EXCEPTION_MATRIX			\
	/* print header */							\
	UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, ("\n\nException Matrix\n      "));		\
	for(_cols = 0; _cols < nNodes; _cols++)		\
		UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, ("N%02d ", nodes[_cols]->dfsNum));	\
	UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, ("\n"));								\
	/* print rows */							\
	for(_rows = 0; _rows < nNodes; _rows++)		\
	{											\
		UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, (" N%02d: ", nodes[_rows]->dfsNum));		\
		for(_cols = 0; _cols < nNodes; _cols++)			\
		{												\
			_addorder = _exceptionMatrix[nodes[_rows]->dfsNum][nodes[_cols]->dfsNum];	\
			if(_addorder != 0)							\
				UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, (" %02d ", _addorder));			\
			else										\
				UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, ("  - "));							\
		}												\
		UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, ("\n"));									\
	}													\
	UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, ("\n\n\n"));

#else

#define	DEBUG_SET_UP_EXCEPTION_MATRIX
#define	DEBUG_ADD_ENTRY_TO_EXCEPTION_MATRIX
#define	DEBUG_PRINT_EXCEPTION_MATRIX

#endif	// DEBUG_GENERATE_EXCEPTION_MATRIX

ExceptionTable& NativeFormatter::
buildExceptionTable(Uint8* methodStart, Pool* inPool)
{
	Uint32 i;

	DEBUG_SET_UP_EXCEPTION_MATRIX;
	ExceptionTableBuilder eBuilder(methodStart);

	for (i = 0; i < nNodes; i++)										// iterate through nodes in scheduled order
	{								
		ControlNode* node = nodes[i];
		ControlKind kind = node->getControlKind();
		if(kind == ckExc || kind == ckAExc || kind == ckThrow) 
		{	// has exception
			ControlNode::ExceptionExtra& excExtra = node->getExceptionExtra();
			const Class **exceptionClass = excExtra.handlerFilters + excExtra.nHandlers;

			if (kind == ckAExc)
				eBuilder.addAEntry(node->getNativeOffset());
			// iterate through exception edges
			Uint32 startIndex = 0;										// start extending exceptions from beginning of table
			ControlEdge *e = node->getSuccessorsEnd();
			for (Uint32 exceptNum = 0; exceptNum < excExtra.nHandlers; exceptNum++)
			{
				e--;													// move to previous handler edge
				exceptionClass--;										// move to previous handler filter

				assert(&(e->getSource()) == node);

				if(e->getTarget().getControlKind() != ckEnd)			// only interested in exceptions handled locally
				{
//					assert(e->getTarget().getControlKind() == ckCatch);	// targets of exception edges must be End or Catch nodes
					ExceptionTableEntry ete;							// container for exception info passing
					ete.pStart = node->getNativeOffset();
					ete.pEnd = nodes[i+1]->getNativeOffset();			// points to byte after 'node'
					ete.pHandler = e->getTarget().getNativeOffset();
					ete.pExceptionType = *exceptionClass;
					UT_LOG(ExceptionMatrix, PR_LOG_ALWAYS, ("N%02d->N%02d ", node->dfsNum, e->getTarget().dfsNum));
					eBuilder.addEntry(startIndex, ete);					// note that startIndex is passed by reference
					DEBUG_ADD_ENTRY_TO_EXCEPTION_MATRIX;
				}
			}
		}
	}

	DEBUG_PRINT_EXCEPTION_MATRIX;
	DEBUG_ONLY(eBuilder.checkInOrder());

	// copy table into new (efficient) storage and insert into NativeCodeCache
	ExceptionTable* eTable = new(*inPool) ExceptionTable(eBuilder, *inPool);
	return (*eTable);
}

