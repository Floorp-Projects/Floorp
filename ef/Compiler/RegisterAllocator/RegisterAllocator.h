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

#ifndef _REGISTER_ALLOCATOR_H_
#define _REGISTER_ALLOCATOR_H_

#include "RegisterAssigner.h"
#include "Coloring.h"
#include "FastBitMatrix.h"
#include "Spilling.h"
#include "LogModule.h"

class ControlGraph;
class InstructionEmitter;
class VirtualRegisterManager;
class FastBitMatrix;
class Pool;

extern void printGraph(Uint32 nNodes, ControlNode** dfsList);


/*
 *--------------------------RegisterAllocator.h----------------------------
 *
 * class RegisterAllocator --
 *
 *-----------------------------------------------------------------------
 */
class RegisterAllocator
{

private:

  Pool&                   regAllocPool;         // Register Allocation Pool.
  /*
   * ControlGraph data.
   */
  ControlNode**           dfsList;              // List of the nodes in Depth First Search order.
  ControlNode**           lndList;              // List of the nodes in Loop Nesting Depth order.
  PRUint32                nNodes;               // Number of nodes in this graph.

  VirtualRegisterManager& vRegManager;          // Virtual Register Manager for this graph.
  MdEmitter&     		  instructionEmitter;   // Instruction emitter for this platform.
  Coloring                registerAssigner;     // Register assigner (right now only graph coloring is supported)
  Spilling                spilling;             // Register spilling
  FastBitMatrix           interferenceMatrix;   // Register interference matrix.

  /*
   * Register Allocation core private methods.
   */
  void checkRegisterClassConflicts();
  void checkPhiNodesAnnotation();
  void buildInterferenceGraph();
  bool canCoalesceRegisters(VirtualRegister& inVR, VirtualRegister& outVR, PRUint32& from, PRUint32& to);
  void coalesceRegisters(PRUint32 from, PRUint32 to);
  void coalesce();
  void calculateSpillCosts();
  void spillPhiNodes();
  bool removePhiNodes();
  void updateLocalVariableTable();

#ifdef DEBUG_LOG
  void printRegisterDebug(LogModuleObject &f, bool verbose = false);
#endif

public:
  /*
   * Constructor.
   */
  RegisterAllocator(Pool& pool, ControlNode** dfs, ControlNode **lnd, PRUint32 n, VirtualRegisterManager& vrMan, MdEmitter& emitter) : 
	regAllocPool(pool), dfsList(dfs), lndList(lnd), nNodes(n), vRegManager(vrMan), instructionEmitter(emitter),
	registerAssigner(pool, vRegManager), spilling(vrMan, emitter), interferenceMatrix(pool) {}

  /*
   * Public call for register allocation.
   */
  void allocateRegisters();
};

#endif /* _REGISTER_ALLOCATOR_H_ */
