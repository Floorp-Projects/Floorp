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
// CodeGenerator.h
//
// Scott M. Silver
//

#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H

#include "Vector.h"
#include "CpuInfo.h"

class Primitive;
class ControlNode;
class Pool;
class DataConsumer;
class Instruction;
struct InstructionUse;
#ifdef IGVISUALIZE
class IGVisualizer;
#endif

struct RenumberData
{
  int neededVisits;
  int timesVisited;
  bool renumbered;
};

// RootPair
struct RootPair 
{
  Primitive*	root;		// root of a BURG labellable subtree
  bool			isPrimary;	// if this root is not reachable by any other root in 
  							// this ControlNode
  RenumberData  data;
};

enum RootKind
{
	rkNotRoot,
	rkRoot,
	rkPrimary
};

class CodeGenerator
{
protected:
	Pool&				mPool;
	MdEmitter& 			mEmitter;

protected:
	void				label(Primitive& inPrimitive);
	void				emit(Primitive* p, int goalnt);
	
	// roots, leaves
	void				findRoots(ControlNode& inControlNode, Vector<RootPair>& outRoots);
	static RootKind		isRoot(Primitive& inPrimitive);
	
public:
						CodeGenerator(Pool& inPool, MdEmitter& inEmitter);
	void				generate(ControlNode& inControlNode);	

	// for BURG labeler
	static Primitive*	consumerToPrimitive(Primitive* inPrimitive, DataConsumer* inConsumer);
	static Primitive*	getExpressionLeftChild(Primitive* inPrimitive);
	static Primitive*	getExpressionRightChild(Primitive* inPrimitive);

	static Instruction*	instructionUseToInstruction(InstructionUse& inIUse);

	// visualization
#ifdef IGVISUALIZE
public:
	void				visualize(); 
protected:
	IGVisualizer&		mVisualizer;
#endif
};

#endif // CODEGENERATOR_H








