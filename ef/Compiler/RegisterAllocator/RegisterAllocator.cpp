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

#define INCLUDE_EMITTER
#include "CpuInfo.h"

#include "Fundamentals.h"
#include "RegisterAllocator.h"
#include "RegisterAssigner.h"
#include "Spilling.h"
#include "InstructionEmitter.h"

#include "ControlGraph.h"
#include "Primitives.h"

#include "VirtualRegister.h"
#include "FastBitMatrix.h"
#include "FastBitSet.h"

#include "CpuInfo.h"
#include "Attributes.h"

UT_DEFINE_LOG_MODULE(RegisterAllocator);

#ifdef DEBUG_LOG
void printGraph(Uint32 nNodes, ControlNode** dfsList)
{
	for (Uint32 m = 0; m < nNodes; m++) {
		ControlNode& node = *dfsList[m];
		node.printRef(UT_LOG_MODULE(RegisterAllocator));
		UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("\n"));

		DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();
		for (DoublyLinkedList<PhiNode>::iterator p = phiNodes.begin(); !phiNodes.done(p); p = phiNodes.advance(p)) {
			PhiNode& phiNode = phiNodes.get(p);
			ValueKind kind = phiNode.getKind();
			if (isStorableKind(kind))
				if (isDoublewordKind(kind)) {
					UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("PhiNode: (%d,%d) <- ", phiNode.getLowVirtualRegisterAnnotation()->getRegisterIndex(), 
							phiNode.getHighVirtualRegisterAnnotation()->getRegisterIndex()));
					for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < phiNode.getInputsEnd(); consumer++)
						UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("%d ", consumer->getVariable().getVirtualRegisterAnnotation()->getRegisterIndex()));
				} else {
				UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("PhiNode: %d <- ", phiNode.getVirtualRegisterAnnotation()->getRegisterIndex()));
				for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < phiNode.getInputsEnd(); consumer++)
					UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("%d ", consumer->getVariable().getVirtualRegisterAnnotation()->getRegisterIndex()));
				}
			UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("\n"));
		}
		InstructionList& instructions = node.getInstructions();
#ifdef DEBUG_LOG
		for(InstructionList::iterator i = instructions.begin(); !instructions.done(i); i = instructions.advance(i)) {
			instructions.get(i).printDebug(UT_LOG_MODULE(RegisterAllocator));
			UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("\n"));
		}
#endif
	}
}
#endif


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::allocateRegisters --
 *
 *   Register allocation main loop.
 *
 *    1- calculate the interference graph.
 *    2- coalesce the registers (remove uneeded copy instructions).
 *    3- assign a color to each VirtualRegister.
 *    4- spill some VirtualRegisters if (3) failed.
 *    5- goto (1) if (3) failed.
 *
 *-----------------------------------------------------------------------
 */
void RegisterAllocator::
allocateRegisters()
{
  PRUint32 nVirtualRegisters = vRegManager.count();

  if (nVirtualRegisters == 0)
	{
	  // DEBUG_ONLY(UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("**** No register allocation needed ****\n");))
	  return;
	}

  // DEBUG_ONLY(PRUint32 loopCounter = 0);
  // DEBUG_ONLY(UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("**** Allocate Registers ****\n");))


  // Check if all phiNodes have an allocated VirtualRegister for each outgoing edge.
  checkPhiNodesAnnotation();

  // Resolve any VirtualRegister class conflicts.
  checkRegisterClassConflicts();

  do 
	{
	  while(true)
		{
		  // DEBUG_ONLY(UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("-- loop %d --\n", ++loopCounter)));

		  // We are compacting the VirtualRegister array to minimize the number of bits in the BitSets.
		  vRegManager.compactMemory();
		  
		  // Build the interference matrix. 
		  buildInterferenceGraph();

		  // remove as many copy instructions as possible. 
		  coalesce();
		  
		  // If we compile a DEBUG target, we want to be sure that the register coalescing 
		  // didn't forget any register interference.
		  DEBUG_ONLY(FastBitMatrix backupMatrix = interferenceMatrix);
		  buildInterferenceGraph();
#ifdef DEBUG
		  if (backupMatrix != interferenceMatrix)
			{
#ifdef DEBUG_LOG

			  UT_LOG(RegisterAllocator, PR_LOG_ALWAYS, ("!!!! interference matrix is <> after a 2nd register interference analysis !!!!\n"));
//			  backupMatrix.printDiff(stdout, interferenceMatrix);
#endif
			}
#endif
		  // Reset the registers.
		  for (VirtualRegisterManager::iterator i = vRegManager.begin(); !vRegManager.done(i); i = vRegManager.advance(i))
			{
			  VirtualRegister& vReg = vRegManager.getVirtualRegister(i);
			  vReg.resetColoringInfo();
			  
			  FastBitSet inter(interferenceMatrix.getRow(i), nVirtualRegisters);
			  vReg.colorInfo.interferenceDegree = inter.countOnes();
			}

		  // Calculate each register's spill cost.
		  calculateSpillCosts();
		  
		  if (!registerAssigner.assignRegisters(interferenceMatrix))
			{
			  spilling.insertSpillCode(dfsList, nNodes);
			  spillPhiNodes();
			}
		  else
			break;
		}
	}
  while (removePhiNodes());

  // Update the info in the LocalVariableEntry mappings
  updateLocalVariableTable();

  // DEBUG_ONLY(printRegisterDebug(stdout, true));
}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::checkPhiNodesAnnotation --
 *
 *  Check if the phi nodes have valid register annotations.
 *
 *-----------------------------------------------------------------------
 */
void RegisterAllocator::
checkPhiNodesAnnotation()
{
  for (PRUint32 n = 0; n < nNodes; n++)
	{
	  ControlNode& node = *dfsList[n];
	  DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();

	  for (DoublyLinkedList<PhiNode>::iterator i = phiNodes.begin(); !phiNodes.done(i); i = phiNodes.advance(i))
		{
		  PhiNode& phiNode = phiNodes.get(i);
		  ValueKind kind = phiNode.getKind();
		  if (isStorableKind(kind))
			  {
				DataNode& producer = phiNode;

				if (isDoublewordKind(kind)) {
					// Check the outgoing edge.
					if (producer.getLowVirtualRegisterAnnotation() == NULL)
						producer.annotateLow(vRegManager.newVirtualRegister(), vrcInteger /* FIXME */);
					if (producer.getHighVirtualRegisterAnnotation() == NULL)
						producer.annotateHigh(vRegManager.newVirtualRegister(), vrcInteger /* FIXME */);

					// Check the incoming edges.
					DataConsumer* limit = phiNode.getInputsEnd();
					for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < limit; consumer++)
					{
						DataNode& p = consumer->getVariable();
						if (p.getLowVirtualRegisterAnnotation() == NULL)
							p.annotateLow(vRegManager.newVirtualRegister(), vrcInteger /* FIXME */);
						if (p.getHighVirtualRegisterAnnotation() == NULL)
							p.annotateHigh(vRegManager.newVirtualRegister(), vrcInteger /* FIXME */);
					}
				} else {
					// Check the outgoing edge.
					if (producer.getVirtualRegisterAnnotation() == NULL)
						producer.annotate(vRegManager.newVirtualRegister(), vrcInteger /* FIXME */);
					
					// Check the incoming edges.
					DataConsumer* limit = phiNode.getInputsEnd();
					for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < limit; consumer++)
					{
						DataNode& p = consumer->getVariable();
						if (p.getVirtualRegisterAnnotation() == NULL)
							p.annotate(vRegManager.newVirtualRegister(), vrcInteger /* FIXME */);
					}
				}
			  }
		}
	}
}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::checkRegisterClassConflicts --
 *
 *  Check that each instruction will get the correct kind of register.
 *
 *-----------------------------------------------------------------------
 */
void RegisterAllocator::
checkRegisterClassConflicts()
{
#if GENERATE_FOR_HPPA
  // FIXME

  // Reset all equivalents registers for the VirtualRegisters.
  for (VirtualRegisterManager::iterator i = vRegManager.begin(); !vRegManager.done(i); i = vRegManager.advance(i))
	fill_n(vRegManager.getVirtualRegister(i).equivalentRegister, nVRClass, (VirtualRegister *) 0);

  for (PRUint32 n = 0; n < nNodes; n++)
	{
	  ControlNode& node = *dfsList[n];

	  InstructionList& instructions = node.getInstructions();
	  for (InstructionList::iterator i = instructions.end(); !instructions.done(i); i = instructions.retreat(i))
		{
		  Instruction& instruction = instructions.get(i);
		  InstructionUse* useBegin = instruction.getInstructionUseBegin();
		  InstructionUse* useEnd = instruction.getInstructionUseEnd();
		  InstructionUse* usePtr;

		  for (usePtr = useBegin; usePtr < useEnd; usePtr++)
			if (usePtr->isVirtualRegister())
			  {
				VirtualRegister& vReg = usePtr->getVirtualRegister();
				VirtualRegisterPtr& vRegPtr = usePtr->getVirtualRegisterPtr();
				VRClass wanted = vRegPtr.getClassConstraint();
				VRClass current = vReg.getClass();
				if (wanted != current)
				  {
					if (vReg.equivalentRegister[wanted] == NULL)
					  {
						Instruction& definingInstruction = *vReg.getDefiningInstruction();
						vReg.equivalentRegister[wanted] = &vRegManager.newVirtualRegister(wanted);
						instructionEmitter.emitCopyAfter(*definingInstruction.getPrimitive(), &definingInstruction.getLinks(),
														 vReg, *vReg.equivalentRegister[wanted]);
					  }
					vRegPtr.initialize(*vReg.equivalentRegister[wanted], wanted);
				  }
			  }
		}
	}
#endif
}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::buildInterferenceGraph --
 *
 *   Build the interferenceMatrix. Each bit in the interferenceMatrix 
 *   corresponds to a register interference.
 *
 *-----------------------------------------------------------------------
 */
void RegisterAllocator::
buildInterferenceGraph()
{
  PRUint32 nVirtualRegisters = vRegManager.count();

  // Reset the Bitsets in each ControlNode.
  for (PRUint32 n = 0; n < nNodes; n++)
	{
	  dfsList[n]->liveAtBegin.sizeToAndClear(nVirtualRegisters);
	  dfsList[n]->liveAtEnd.sizeToAndClear(nVirtualRegisters);
	  dfsList[n]->hasNewLiveAtEnd = true;
	  dfsList[n]->liveAtBeginIsValid = false;
	}

  // Reset the liveness info.
  for (VirtualRegisterManager::iterator i = vRegManager.begin(); !vRegManager.done(i); i = vRegManager.advance(i)) {
	  VirtualRegister& vReg = vRegManager.getVirtualRegister(i);
	  vReg.liveness.sizeToAndClear(nNodes);
	  if (vReg.hasSpecialInterference) {
		  vReg.specialInterference.clear();;
	  }
  }

  // Reset the interference matrix.
  interferenceMatrix.sizeToAndClear(nVirtualRegisters, nVirtualRegisters);

  // Bitset of the VirtualRegisters currently alive.
  FastBitSet currentLive(nVirtualRegisters);
  // Bitset of the VirtualRegisters alive at the end of a node.
  FastBitSet liveAtEnd(nVirtualRegisters);

  for (bool loop = true; loop;)
	{
	  loop = false;

	  for (PRInt32 n = (nNodes - 1); n >= 0; n--)
		{
		  ControlNode& node = *dfsList[n];
		  bool hasPhiNodes = !node.getPhiNodes().empty();

		  // If liveAtEnd didn't change there's no new interferences to find. Just skip this node.
		  if ((!node.hasNewLiveAtEnd) && node.liveAtBeginIsValid)
			continue;

		  node.hasNewLiveAtEnd = false;
		  currentLive = node.liveAtEnd;

		  // All the virtual registers in currentLive are alive in this node.
		  for (PRInt32 k = currentLive.firstOne(); k != -1; k = currentLive.nextOne(k))
			vRegManager.getVirtualRegister(k).liveness.set(n);

		  
		  // We walk backward all the instructions defined in this node.
		  // A register becomes alive with its last use (first found), interfere with
		  // all the registers alive at its definition, and is killed by its definition.
		  InstructionList& instructions = node.getInstructions();
		  for (InstructionList::iterator i = instructions.end(); !instructions.done(i);
			   i = instructions.retreat(i))
			{
			  Instruction&       instruction = instructions.get(i);
			  InstructionUse*    useBegin    = instruction.getInstructionUseBegin();
			  InstructionUse*    useEnd      = instruction.getInstructionUseEnd();
			  InstructionUse*    usePtr;
			  InstructionDefine* defBegin    = instruction.getInstructionDefineBegin();
			  InstructionDefine* defEnd      = instruction.getInstructionDefineEnd();
			  InstructionDefine* defPtr;

			  InstructionFlags flags = instruction.getFlags();
			  if (flags & ifCopy)
				{
				  // Move is handled specially to avoid adding an interference between the
				  // source and the destination. If an interference exists between these
				  // registers another instruction will set it. For now the source register
				  // and the destination register will contain the same value.
				  PR_ASSERT(useBegin[0].isVirtualRegister());
				  currentLive.clear(useBegin[0].getRegisterIndex());
				}

			  // Registers are defined. This definition has an interference with all the
			  // registers currently alive.
			  for (defPtr = defBegin; defPtr < defEnd; defPtr++)
				if (defPtr->isVirtualRegister())
				  {
					VirtualRegister& defReg = defPtr->getVirtualRegister();
					PRUint32 defRegIndex = defReg.getRegisterIndex();

					interferenceMatrix.orRow(defRegIndex, currentLive);

					// We use the definition of this register to update the precoloring information.
					//   In a program we can have different VirtualRegisters precolored to the same color.
					// During the register coalescing we need to know if a non-precolored register can
					// be coalesced with a precolored to C. It is possible to do so only if the non
					// precolored register has no interference with any other register also precolored to C.
					// For this reason the VirtualRegisterManager will create a PseudoVirtualRegister corresponding
					// to one machine register. As the Pseudo VirtualRegister is never seen by the interference graph
					// algorithm we can use the row corresponding to its index to store the index of all VirtualRegisters
					// precolored to the color C.
					if (defReg.isPreColored())
					  interferenceMatrix.set(defReg.colorInfo.preColoredRegisterIndex, defRegIndex);
				  }
			  
			  // The definition just killed these registers.
			  for (defPtr = defBegin; defPtr < defEnd; defPtr++)
				if (defPtr->isVirtualRegister())
				  currentLive.clear(defPtr->getRegisterIndex());

			  // If this instruction is a Call instruction, we want all the caller-save 
			  // registers to interfere with the current live registers. We do not want an 
			  // interference with the callee-save registers because the call will save &
			  // restore these registers.
			  if (flags & ifCall)
				{
				  for (PRInt32 r = currentLive.firstOne(); r != -1; r = currentLive.nextOne(r))
					{
					  VirtualRegister& vReg = vRegManager.getVirtualRegister(r);
					  
					  if (!vReg.hasSpecialInterference)
						{
						  vReg.specialInterference.sizeToAndClear(NUMBER_OF_REGISTERS);
						  vReg.hasSpecialInterference = true;
						}
					  vReg.specialInterference.set(FIRST_CALLER_SAVED_GR, LAST_CALLER_SAVED_GR);
#if NUMBER_OF_FPREGISTERS != 0
					  vReg.specialInterference.set(FIRST_CALLER_SAVED_FPR, LAST_CALLER_SAVED_FPR);
#endif
					}
				}

#if FIXME
			  if (flags & ifSpecialCall)
				{
				}
#endif

			  // Each use of a register makes it alive.
			  for (usePtr = useBegin; usePtr < useEnd; usePtr++)
				if (usePtr->isVirtualRegister())
				  {
					VirtualRegister& vReg = usePtr->getVirtualRegister();
					currentLive.set(vReg.getRegisterIndex());
					vReg.liveness.set(n);
				  }
			}

		  if (currentLive != node.liveAtBegin)
			{
			  const DoublyLinkedList<ControlEdge>& predecessors = node.getPredecessors();
			  PRUint32 edgeIndex = 0;

			  for (DoublyLinkedList<ControlEdge>::iterator i = predecessors.begin(); !predecessors.done(i); i = predecessors.advance(i), edgeIndex++)
				{
				  ControlNode& predecessor = predecessors.get(i).getSource();

				  if (hasPhiNodes)
					{
					  // Add liveness information for the phi nodes.
					  liveAtEnd = currentLive;

					  DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();
					  for (DoublyLinkedList<PhiNode>::iterator p = phiNodes.end(); !phiNodes.done(p); p = phiNodes.retreat(p)) {
						  PhiNode& phiNode = phiNodes.get(p);
						  ValueKind kind = phiNode.getKind();

						  if (isStorableKind(kind)) {
							  PRUint32 in = phiNode.nthInput(edgeIndex).getVariable().getVirtualRegisterAnnotation()->getRegisterIndex();
							  PRUint32 out = phiNode.getVirtualRegisterAnnotation()->getRegisterIndex();
							  
							  if (in != out) {
								  // Handled like a Copy Instruction.
								  liveAtEnd.clear(in);
								  interferenceMatrix.orRow(out, liveAtEnd);
								  liveAtEnd.clear(out);
								  liveAtEnd.set(in);
								  
								  VirtualRegister& outVR = *phiNode.getVirtualRegisterAnnotation();
								  if (outVR.isPreColored())
									  interferenceMatrix.set(outVR.colorInfo.preColoredRegisterIndex, out);
							  }
							  if (isDoublewordKind(kind)) {
								  in = phiNode.nthInput(edgeIndex).getVariable().getHighVirtualRegisterAnnotation()->getRegisterIndex();
								  out = phiNode.getHighVirtualRegisterAnnotation()->getRegisterIndex();
								  
								  if (in != out) {
									  // Handled like a Copy Instruction.
									  liveAtEnd.clear(in);
									  interferenceMatrix.orRow(out, liveAtEnd);
									  liveAtEnd.clear(out);
									  liveAtEnd.set(in);
									  
									  VirtualRegister& outVR = *phiNode.getHighVirtualRegisterAnnotation();
									  if (outVR.isPreColored())
										  interferenceMatrix.set(outVR.colorInfo.preColoredRegisterIndex, out);
								  }
							  }
						  }
					  }
					  bool changed = predecessor.liveAtEnd.setAndTest(liveAtEnd);
					  predecessor.hasNewLiveAtEnd |= changed;
					  loop |= changed;
					} else { /* !hasPhiNodes */
						bool changed = predecessor.liveAtEnd.setAndTest(currentLive);
						predecessor.hasNewLiveAtEnd |= changed;
						loop |= changed;
					}
				}
			}
		  node.liveAtBegin = currentLive;
		  node.liveAtBeginIsValid = true;
		}
	}

  FastBitSet gReg(regAllocPool, nVirtualRegisters);
  FastBitSet fpReg(regAllocPool, nVirtualRegisters);
  FastBitSet ssReg(regAllocPool, nVirtualRegisters);
  VirtualRegisterManager::iterator r;

  for (r = vRegManager.begin(); !vRegManager.done(r); r = vRegManager.advance(r))
	{
	  VirtualRegister& vReg = vRegManager.getVirtualRegister(r);
	  switch (vReg.getClass())
		{
		case vrcInteger:
		  gReg.set(r);
		  break;
		case vrcFloatingPoint:
		case vrcFixedPoint:
		  fpReg.set(r);
		  break;
		case vrcStackSlot:
		  ssReg.set(r);
		  break;
		default:
		  PR_ASSERT(false);
		}
	}
  for (r = vRegManager.begin(); !vRegManager.done(r); r = vRegManager.advance(r))
	{
	  VirtualRegister& vReg = vRegManager.getVirtualRegister(r);
	  switch (vReg.getClass())
		{
		case vrcInteger:
		  interferenceMatrix.andRow(r, gReg);
		  break;
		case vrcFloatingPoint:
		case vrcFixedPoint:
		  interferenceMatrix.andRow(r, fpReg);
		  break;
		case vrcStackSlot:
		  interferenceMatrix.andRow(r, ssReg);
		  break;
		default:
		  PR_ASSERT(false);
		}
	}

  FastBitMatrix trans(regAllocPool, nVirtualRegisters, nVirtualRegisters);
  for (r = vRegManager.begin(); !vRegManager.done(r); r = vRegManager.advance(r))
	{
	  interferenceMatrix.clear(r, r);
	  FastBitSet row(interferenceMatrix.getRow(r), nVirtualRegisters);
	  for (PRInt32 i = row.firstOne(); i != -1; i = row.nextOne(i))
		trans.set(i, r);
	}
  for (r = vRegManager.begin(); !vRegManager.done(r); r = vRegManager.advance(r))
	{
	  FastBitSet row(trans.getRow(r), nVirtualRegisters);
	  interferenceMatrix.orRow(r, row);
	}

}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::canCoalesceRegisters --
 *
 *   Return true if it is possible to coalesce inVr & outVr. If it is
 *   from & to will contain the register indexes to coalesce.
 *
 *-----------------------------------------------------------------------
 */
bool RegisterAllocator::
canCoalesceRegisters(VirtualRegister& inVR, VirtualRegister& outVR, PRUint32& from, PRUint32& to)
{
  // We dont have to check the register class because if a Copy instruction
  // is emitted then in & out have the same register class.

  from = inVR.getRegisterIndex();
  to = outVR.getRegisterIndex();
  bool inIsPreColored = inVR.isPreColored();
  bool outIsPreColored = outVR.isPreColored();

  if (inIsPreColored && outIsPreColored)
	{
	  // The only case where we can remove this copy is if this two registers
	  // have the same color.
	  if ((inVR.getPreColor() == outVR.getPreColor()) && !interferenceMatrix.test(from, to))
		{
		  if (from != to)
			interferenceMatrix.clear(inVR.colorInfo.preColoredRegisterIndex, from);
		  return true;
		}
	}
  else if (inIsPreColored) // && (!outIsPreColored)
	{
	  // outVR is not precolored but cannot have an interference with any registers preColored to inPreColor.
	  if (outVR.hasSpecialInterference)
		if (outVR.specialInterference.test(inVR.getPreColor()))
		  return false;

	  if (!interferenceMatrix.test(to, interferenceMatrix.getRow(inVR.colorInfo.preColoredRegisterIndex)))
		{
		  // We must swap in & out, because inVR must stay preColored.
		  PRUint32 tmp = to; to = from; from = tmp;
		  return true;
		}
	}
  else if (outIsPreColored) // && (!inIsPreColored)
	{
	  if (inVR.hasSpecialInterference)
		if (inVR.specialInterference.test(outVR.getPreColor()))
		  return false;

	  // inVR is not precolored but cannot have an interference with any registers preColored to outPreColor.
	  if (!interferenceMatrix.test(from, interferenceMatrix.getRow(outVR.colorInfo.preColoredRegisterIndex)))
		return true;
	}
  else // (!inIsPreColored && !outIsPreColored)
	{
	  if ((from == to) || !interferenceMatrix.test(from, to))
		return true;
	}
  return false;
}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::coalesceRegisters --
 *
 *   Coalesce registers at indexes in & out. Update the interference 
 *   matrix.
 *
 *-----------------------------------------------------------------------
 */
inline void RegisterAllocator::
coalesceRegisters(PRUint32 from, PRUint32 to)
{
  PRUint32 nVirtualRegisters = vRegManager.count();
  FastBitSet row(interferenceMatrix.getRow(from), nVirtualRegisters);
  for (PRInt32 r = row.firstOne(); r != -1; r = row.nextOne(r))
	{
	  interferenceMatrix.clear(r, from);
	  interferenceMatrix.set(r, to);
	}
  interferenceMatrix.orRow(to, row);
  DEBUG_ONLY(interferenceMatrix.clearRow(from));

  // update the liveness info.
  VirtualRegister& fromVR = vRegManager.getVirtualRegister(from);
  VirtualRegister& toVR = vRegManager.getVirtualRegister(to);

  FastBitSet& liveness = fromVR.liveness;
  for (PRInt32 i = liveness.firstOne(); i != -1; i = liveness.nextOne(i))
	{
	  dfsList[i]->liveAtEnd.clear(from);
	  dfsList[i]->liveAtEnd.set(to);
	}
  toVR.liveness |= liveness;

  if (fromVR.hasSpecialInterference) {
	  if (toVR.hasSpecialInterference)
		  toVR.specialInterference |= fromVR.specialInterference;
	  else {
		  toVR.specialInterference.sizeTo(NUMBER_OF_REGISTERS);
		  toVR.specialInterference = fromVR.specialInterference;
	  }
	  toVR.hasSpecialInterference = true;
  }

  vRegManager.moveVirtualRegister(from, to);
}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::coalesce --
 *
 *   Check all Copy instructions & phiNodes for possible register
 *   coalescing. If two registers are coalesced there's no need to
 *   keep the instruction (or the phiNode). The ControlFlow graph
 *   is analysed from the most executed node to the least.
 *
 *   TODO: loop nesting depth is not good enough (should analyse 
 *          the program regions instead).
 *
 *-----------------------------------------------------------------------
 */
void
RegisterAllocator::coalesce()
{
  for (PRInt32 n = nNodes - 1; n >= 0; n--)
	{
	  ControlNode& node = *lndList[n];

	  InstructionList& instructions = node.getInstructions();
	  InstructionList::iterator i = instructions.begin();

	  // Check the instructions in this node.
	  while(!instructions.done(i))
		{
		  Instruction& insn = instructions.get(i);
		  // We might remove this instruction. It is necessary to advance the iterator 
		  // before the remove.
		  i = instructions.advance(i);

		  if (insn.getFlags() & ifCopy)
			{
			  PR_ASSERT(insn.getInstructionUseBegin()[0].isVirtualRegister() && insn.getInstructionDefineBegin()[0].isVirtualRegister());

			  VirtualRegister& inVR = insn.getInstructionUseBegin()[0].getVirtualRegister();
			  VirtualRegister& outVR = insn.getInstructionDefineBegin()[0].getVirtualRegister();
			  PRUint32 from, to;

			  if (canCoalesceRegisters(inVR, outVR, from, to))
				{
				  if (from != to)
					coalesceRegisters(from, to);
				  
				  // Before removing the instruction, we unitialize the VirtualRegisterPtrs in its annotations.
				  insn.unlinkRegisters();
				  insn.remove();  
				}
			}
		}

	  // Check the phiNodes in this node.
	  DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();
	  for (DoublyLinkedList<PhiNode>::iterator p = phiNodes.end(); !phiNodes.done(p);)
		{
		  PhiNode& phiNode = phiNodes.get(p);
		  ValueKind kind = phiNode.getKind();
		  p = phiNodes.retreat(p);
		  
		  if (isStorableKind(kind))
			  {
				bool removePhiNode = true;
				DataConsumer* limit = phiNode.getInputsEnd();

				for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < limit; consumer++)
				  {
					VirtualRegister* inVR = consumer->getVariable().getVirtualRegisterAnnotation();
					VirtualRegister* outVR = phiNode.getVirtualRegisterAnnotation();
					PRUint32 from, to;
						
					if (inVR->getClass() == outVR->getClass()) {
						if (inVR != outVR) {
							if (canCoalesceRegisters(*inVR, *outVR, from, to))
								coalesceRegisters(from, to);
							else
								removePhiNode = false;
						}
					} else
						removePhiNode = false;

					if (isDoublewordKind(kind)) {
						inVR = consumer->getVariable().getHighVirtualRegisterAnnotation();
						outVR = phiNode.getHighVirtualRegisterAnnotation();
						
						if (inVR->getClass() == outVR->getClass()) {
							if (inVR != outVR) {
								if (canCoalesceRegisters(*inVR, *outVR, from, to))
									coalesceRegisters(from, to);
								else
									removePhiNode = false;
							}
						} else
							removePhiNode = false;
					}
				  }
				if (removePhiNode)
				  phiNode.remove();
			}
		}
	}
}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::spillPhiNodes --
 *
 *  Replace the phiNodes spilled VirtualRegisters by their StackSlot 
 *  equivalent.
 *
 *-----------------------------------------------------------------------
 */
void RegisterAllocator::
spillPhiNodes()
{
  for (PRUint32 n = 0; n < nNodes; n++)
	{
	  ControlNode& node = *dfsList[n];
	  DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();

	  for (DoublyLinkedList<PhiNode>::iterator i = phiNodes.begin(); !phiNodes.done(i); i = phiNodes.advance(i))
		{
		  PhiNode& phiNode = phiNodes.get(i);
		  ValueKind kind = phiNode.getKind();
		  if (isStorableKind(kind))
			  {
				DataNode& producer = phiNode;

				if (isDoublewordKind(kind)) {
					VirtualRegister& lowOutVR = *producer.getLowVirtualRegisterAnnotation();
					VirtualRegister& highOutVR = *producer.getHighVirtualRegisterAnnotation();

					// Check the outgoing edge.
					if (lowOutVR.spillInfo.willSpill)
						producer.getLowVirtualRegisterPtrAnnotation().initialize(*lowOutVR.equivalentRegister[vrcStackSlot]);
					if (highOutVR.spillInfo.willSpill)
						producer.getHighVirtualRegisterPtrAnnotation().initialize(*highOutVR.equivalentRegister[vrcStackSlot]);
					
					// Check the incoming edges.
					DataConsumer* limit = phiNode.getInputsEnd();
					for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < limit; consumer++)
					{
						VirtualRegister& lowInVR = *consumer->getVariable().getLowVirtualRegisterAnnotation();
						VirtualRegister& highInVR = *consumer->getVariable().getHighVirtualRegisterAnnotation();
						if (lowInVR.spillInfo.willSpill)
							consumer->getVariable().getLowVirtualRegisterPtrAnnotation().initialize(*lowInVR.equivalentRegister[vrcStackSlot]);
						if (highInVR.spillInfo.willSpill)
							consumer->getVariable().getHighVirtualRegisterPtrAnnotation().initialize(*highInVR.equivalentRegister[vrcStackSlot]);
					}
				} else {
					VirtualRegister& outVR = *producer.getVirtualRegisterAnnotation();

					// Check the outgoing edge.
					if (outVR.spillInfo.willSpill)
						producer.getVirtualRegisterPtrAnnotation().initialize(*outVR.equivalentRegister[vrcStackSlot]);
					
					// Check the incoming edges.
					DataConsumer* limit = phiNode.getInputsEnd();
					for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < limit; consumer++)
					{
						VirtualRegister& inVR = *consumer->getVariable().getVirtualRegisterAnnotation();
						if (inVR.spillInfo.willSpill)
							consumer->getVariable().getVirtualRegisterPtrAnnotation().initialize(*inVR.equivalentRegister[vrcStackSlot]);
					}
				}
			}
		}
	}
}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::removePhiNodes --
 *
 *  Replace the phiNodes by copy instructions.
 *
 *-----------------------------------------------------------------------
 */
bool RegisterAllocator::
removePhiNodes()
{
  bool addedNode = false;
  bool needsRebuild = false;

  for (PRUint32 n = 0; n < nNodes; n++)
	{
	  ControlNode& node = *dfsList[n];
	  DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();

	  if (phiNodes.empty())
		continue;

	  const DoublyLinkedList<ControlEdge>& predecessors = node.getPredecessors();
	  bool hasOneInput = predecessors.lengthIs(1);
	  
	  DoublyLinkedList<PhiNode>::iterator i;
	  // If this node has only one predecessor, it is possible to insert the copy instruction
	  // at the begininig of this node. To respect the interference order, we have to append
	  // each phiNode in the same order they are in the linked list.
	  for (i = hasOneInput ? phiNodes.end() : phiNodes.begin(); !phiNodes.done(i);)
		{
		  PhiNode& phiNode = phiNodes.get(i);
		  ValueKind kind = phiNode.getKind();
		  i = hasOneInput ? phiNodes.retreat(i) : phiNodes.advance(i);
		  
		  if (isStorableKind(kind))
			  {
				VirtualRegister* lowOutVR = phiNode.getVirtualRegisterAnnotation();
				VirtualRegister* highOutVR = isDoublewordKind(kind) ? phiNode.getHighVirtualRegisterAnnotation() : (VirtualRegister *) 0;
				DoublyLinkedList<ControlEdge>::iterator e = predecessors.begin();

				DataConsumer* limit = phiNode.getInputsEnd();
				for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < limit; consumer++)
				  {
					// For each consumer.
					ControlEdge& edge = predecessors.get(e);
					e = predecessors.advance(e);

					for (int w = 1; w <= 2; w++) {
						if (w == 2 && highOutVR == NULL)
							break;

						VirtualRegister* inVR = (w == 1) ? consumer->getVariable().getLowVirtualRegisterAnnotation() :
							consumer->getVariable().getHighVirtualRegisterAnnotation();
						VirtualRegister* outVR = (w == 1) ? lowOutVR : highOutVR;

						if (inVR != outVR)
						{
							// We have to emit a Copy from inVR to outVR.
							
							InstructionList::iterator place; // where to append the copyInstruction.
							
							if (hasOneInput)
							{
								// append at the begining of this node.
								place = node.getInstructions().begin()->prev; 
							}
							else
							{
								ControlNode& source = edge.getSource();
								if (source.nSuccessors() != 1)
								{
								// This predecessor has more than one outgoing edge. We need to add
								// a new control Block to insert the copy instruction.
									ControlNode& cn = edge.getSource().controlGraph.newControlNode();
									cn.setControlBlock();
									ControlEdge& newEdge = cn.getNormalSuccessor();
									
									newEdge.substituteTarget(edge);
									cn.addPredecessor(edge);
									
								// append at the begin of this new node.
									place = cn.getInstructions().begin(); 
									addedNode = true;
								}
								else
								{
								// append at the end of the corresponding predecessor node.
									place = edge.getSource().getInstructions().end();
								}
							}
							
							VRClass inClass = inVR->getClass();
							VRClass outClass = outVR->getClass();
							
							if ((inClass == vrcStackSlot) && ((outClass == vrcStackSlot)))
							{
								// Both register are spilled. We have to load the value in memory and then
								// store it at its destination. To do this we need to create a new temporary
								// VirtualRegister. It might create some new interferences, so we have to
								// do one more register allocation loop.
								VirtualRegister& vReg = vRegManager.newVirtualRegister();
								instructionEmitter.emitStoreAfter(phiNode, place, vReg, *outVR);
								instructionEmitter.emitLoadAfter(phiNode, place, vReg, *inVR);
								needsRebuild = true;
							}
							else if (inClass == vrcStackSlot)
								// outVR needs to be loaded.
								instructionEmitter.emitLoadAfter(phiNode, place, *outVR, *inVR);
							else if (outClass == vrcStackSlot)
								// inVR needs to be stored.
								instructionEmitter.emitStoreAfter(phiNode, place, *inVR, *outVR);
							else 
								// None of them are spilled, it's a regular copy 
								// (!!! inVR & outVR might not have the same register class)
								needsRebuild |= instructionEmitter.emitCopyAfter(phiNode, place, *inVR, *outVR);
						}
					}
				  }
				phiNode.remove();
			  }
		}
	}
  
  if (addedNode)
	{
	  ControlGraph& cg = dfsList[0]->controlGraph;
	  cg.dfsSearch();
	  
	  if (needsRebuild) {
		  nNodes = cg.nNodes;
		  dfsList = cg.dfsList;
		  cg.lndSearch();
		  lndList = cg.lndList;
	  }
	}

  return needsRebuild;
}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::calculateSpillCost --
 *
 *  In order to do a graph coloring, we need to get the spill cost of
 *  each VirtualRegister used in this program.
 *
 *-----------------------------------------------------------------------
 */
void
RegisterAllocator::calculateSpillCosts()
{
  PRUint32 nVirtualRegisters = vRegManager.count();
  FastBitSet needLoad(nVirtualRegisters);
  FastBitSet currentLive(nVirtualRegisters);
  FastBitSet mustSpill(nVirtualRegisters);

  for (PRInt32 n = nNodes - 1; n >=0; n--)
	{
	  ControlNode& node = *dfsList[n];
	  
	  needLoad.clear();
	  currentLive = node.liveAtEnd;
	  mustSpill = currentLive;

	  // Get the copy information about the phiNodes in this node.
	  DoublyLinkedList<PhiNode>& phiNodes = node.getPhiNodes();
	  for (DoublyLinkedList<PhiNode>::iterator i = phiNodes.begin(); !phiNodes.done(i); i = phiNodes.advance(i))
		{
		  PhiNode& phiNode = phiNodes.get(i);
		  ValueKind kind = phiNode.getKind();
		  
		  if (isStorableKind(kind))
			  {
				  // FIXME: This should be more accurate by taking into account the register class of each register.
				  //  vrcStackSlot -> vrcInteger, vrcFloatingPoint, vrcFixedPoint is a load.
				  //  vrcInteger, vrcFloatingPoint, vrcFixedPoint -> vrcStackSlot is a store.
				  //  vrcInteger -> vrcFloatingPoint, vrcFixedPoint (is md)
				  
				  VirtualRegister* outVR = phiNode.getVirtualRegisterAnnotation();
				  if (outVR->getClass() != vrcStackSlot)
				  {
					  DataConsumer* limit = phiNode.getInputsEnd();
					  for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < limit; consumer++)
					  {
						  VirtualRegister* inVR = consumer->getVariable().getVirtualRegisterAnnotation();
						  if (inVR != outVR)
							  if (inVR->getClass() != vrcStackSlot)
								  inVR->spillInfo.nCopies += node.nVisited;
					  }
				  }
				  if (isDoublewordKind(kind)) {
					  outVR = phiNode.getHighVirtualRegisterAnnotation();
					  if (outVR->getClass() != vrcStackSlot)
					  {
						  DataConsumer* limit = phiNode.getInputsEnd();
						  for (DataConsumer *consumer = phiNode.getInputsBegin(); consumer < limit; consumer++)
						  {
							  VirtualRegister* inVR = consumer->getVariable().getHighVirtualRegisterAnnotation();
							  if (inVR != outVR)
								  if (inVR->getClass() != vrcStackSlot)
									  inVR->spillInfo.nCopies += node.nVisited;
						  }
					  }
				  }
			}
		}
  
	  // The instructions in this node.
	  InstructionList& instructions = node.getInstructions();
	  for (InstructionList::iterator j = instructions.end(); !instructions.done(j); j = instructions.retreat(j))
		{
		  Instruction& instruction = instructions.get(j);

		  InstructionUse*    useBegin = instruction.getInstructionUseBegin();
		  InstructionUse*    useEnd   = instruction.getInstructionUseEnd();
		  InstructionUse*    usePtr;
		  InstructionDefine* defBegin = instruction.getInstructionDefineBegin();
		  InstructionDefine* defEnd   = instruction.getInstructionDefineEnd();
		  InstructionDefine* defPtr;

		  // Check copy insn
		  if (instruction.getFlags() & ifCopy)
			{
			  PR_ASSERT(useBegin[0].isVirtualRegister());
			  useBegin[0].getVirtualRegister().spillInfo.nCopies += node.nVisited;
			}

		  // Handle definitions
		  for (defPtr = defBegin; defPtr < defEnd; defPtr++)
			if (defPtr->isVirtualRegister())
			  {
				VirtualRegister& vReg = defPtr->getVirtualRegister();
				if (vReg.getClass() != vrcStackSlot)
				  {
					PRUint32 registerIndex = vReg.getRegisterIndex();
					if (needLoad.test(registerIndex))
					  {
						needLoad.clear(registerIndex);
						if (!mustSpill.test(registerIndex))
						  vReg.spillInfo.infiniteSpillCost = true;
					  }
					vReg.spillInfo.nStores += node.nVisited;
					currentLive.clear(registerIndex);
				  }
			  }
		  
		  // Check for deaths 
		  for (usePtr = useBegin; usePtr < useEnd; usePtr++)
			if (usePtr->isVirtualRegister())
			  {
				VirtualRegister& vReg = usePtr->getVirtualRegister();
				if (vReg.getClass() != vrcStackSlot)
				  {
					if (!currentLive.test(vReg.getRegisterIndex()))
					  {
						for (PRInt32 nl = needLoad.firstOne(); nl != -1; nl = needLoad.nextOne(nl))
						  {
							vRegManager.getVirtualRegister(nl).spillInfo.nLoads += node.nVisited;
							mustSpill.set(nl);
						  }
						needLoad.clear();
					  }
				  }
			  }

		  // Handle uses
		  for (usePtr = useBegin; usePtr < useEnd; usePtr++)
			if (usePtr->isVirtualRegister())
			  {
				VirtualRegister& vReg = usePtr->getVirtualRegister();
				if (vReg.getClass() != vrcStackSlot)
				  {
					PRUint32 registerIndex = vReg.getRegisterIndex();
					currentLive.set(registerIndex);
					needLoad.set(registerIndex);
				  }
			  }
		  
		  // All the registers in currentLive just lived one more cycle.
		  for (PRInt32 registerIndex = currentLive.firstOne(); registerIndex != -1; registerIndex = currentLive.nextOne(registerIndex))
			{
			  VirtualRegister& vReg = vRegManager.getVirtualRegister(registerIndex);
			  if (vReg.getClass() != vrcStackSlot)
				vReg.spillInfo.liveLength++;
			}
		}
	  
	  // All the registers left in needLoad will be loaded.
	  for (PRInt32 nl = needLoad.firstOne(); nl != -1; nl = needLoad.nextOne(nl))
		vRegManager.getVirtualRegister(nl).spillInfo.nLoads += node.nVisited;
	}
  
  // Summarize
  for (VirtualRegisterManager::iterator j = vRegManager.begin(); !vRegManager.done(j); j = vRegManager.advance(j))
	{
	  VirtualRegister& vReg = vRegManager.getVirtualRegister(j);
	  if (vReg.getClass() != vrcStackSlot)
		{
		  if (!vReg.spillInfo.infiniteSpillCost)
			{
			  // It may be good to take into account the size of the live range and to
			  // weight differently a load/store and a copy.
			  vReg.spillInfo.spillCost = (2 * (vReg.spillInfo.nLoads + vReg.spillInfo.nStores) - vReg.spillInfo.nCopies) /
				(((Flt32)vReg.spillInfo.liveLength));
			}
		}
	}
}



// asharma
void 
RegisterAllocator::updateLocalVariableTable()
{
	for (Uint32 m = 0; m < nNodes; m++) {
		ControlNode& node = *dfsList[m];
		DoublyLinkedList<Primitive> &primitives = node.getPrimitives();
		for (DoublyLinkedList<Primitive>::iterator i = primitives.begin(); 
			 !primitives.done(i); 
			 i = primitives.advance(i)) {
			Primitive &p = primitives.get(i);
			// We're not interested in primitives that are not the values of
			// certain local variables
			if (!p.mapping)
				continue;
			switch(p.getKind()) {
			case vkCond:
			case vkMemory:
				break;
			default:
				VirtualRegister *vr = p.getVirtualRegisterAnnotation();
				if (vr) {
					VariableLocation loc(true, vr->getColor());
					p.mapping->setVariableLocation(loc);
				}
				break;
			}
		}
	}
}


/*
 *-------------------------RegisterAllocator.cpp-------------------------
 *
 * RegisterAllocator::printRegisterDebug --
 *
 *   Print informations about valid registers.
 *
 *-----------------------------------------------------------------------
 */
#ifdef DEBUG_LOG
void RegisterAllocator::
printRegisterDebug(LogModuleObject &f, bool verbose)
{
  for (VirtualRegisterManager::iterator i = vRegManager.begin(); !vRegManager.done(i); i = vRegManager.advance(i))
	{
	  VirtualRegister& vReg = vRegManager.getVirtualRegister(i);
	  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%d: ", i));

	  // print the class info.
	  PRUint8 classChar;
	  switch (vReg.getClass())
		{
		case vrcInteger:
		  classChar = 'I';
		  break;
		case vrcFixedPoint:
		  classChar = 'f';
		  break;
		case vrcFloatingPoint:
		  classChar = 'F';
		  break;
		case vrcStackSlot:
		  classChar = 'S';
		  break;
		default:
		  classChar = '?';
		  break;
		}
	  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%c ", classChar));
	  
	  // print the register.
	  if (vReg.isPreColored())
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("r%d ", vReg.getPreColor()));
	  else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("vr%d ", i));

	  // print his interference.
	  if (verbose)
		{
		  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("interferes w/ ( "));
		  FastBitSet inter(interferenceMatrix.getRow(i), vRegManager.count());
		  for (PRInt32 j = inter.firstOne(); j != -1; j = inter.nextOne(j))
			{
			  VirtualRegister& vr = vRegManager.getVirtualRegister(j);
			  if (vr.isPreColored())
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("r%d ", vr.getPreColor()));
			  else
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("vr%d ", j));
			}
		  UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" - "));
		  FastBitSet& special = vReg.specialInterference;
		  for (PRInt32 k = special.firstOne(); k != -1; k = special.nextOne(k))
			  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("r%d ", k));
		  UT_OBJECTLOG(f, PR_LOG_ALWAYS, (") "));
		}

	  // print the liveness info
	  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("alive in nodes ( "));
	  for (PRInt32 k = vReg.liveness.firstOne(); k != -1; k = vReg.liveness.nextOne(k))
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%d ", k));
	  UT_OBJECTLOG(f, PR_LOG_ALWAYS, (") "));

	  // print the interference degree
	  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("nInter=%d ", vReg.colorInfo.interferenceDegree));

	  // print the spill cost
	  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("spillcost="));
	  if (vReg.spillInfo.infiniteSpillCost)
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("infinite "));
	  else
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%.2f ", vReg.spillInfo.spillCost));

	  // done.
	  UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n"));
	}
}
#endif
