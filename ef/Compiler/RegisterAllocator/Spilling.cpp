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
#include "ControlNodes.h"
#include "Instruction.h"
#include "InstructionEmitter.h"
#include "Spilling.h"


void Spilling::
insertSpillCode(ControlNode** dfsList, Uint32 nNodes)
{
  PRUint32 nVirtualRegisters = vRegManager.count();
  FastBitSet currentLive(vRegManager.pool, nVirtualRegisters);
  FastBitSet usedInThisInstruction(vRegManager.pool, nVirtualRegisters);
  RegisterFifo grNeedLoad(nVirtualRegisters);
  RegisterFifo fpNeedLoad(nVirtualRegisters);

  for (PRInt32 n = nNodes - 1; n >= 0; n--)
	{
	  PR_ASSERT(grNeedLoad.empty() & fpNeedLoad.empty());
	  ControlNode& node = *dfsList[n];

	  currentLive = node.liveAtEnd;

	  PRUint32 nGeneralAlive = 0;
	  PRUint32 nFloatingPointAlive = 0;

	  // Get the number of registers alive at the end of this node.
	  for (PRInt32 j = currentLive.firstOne(); j != -1; j = currentLive.nextOne(j))
		{
		  VirtualRegister& vReg = vRegManager.getVirtualRegister(j);
		  if (vReg.spillInfo.willSpill)
			{
			  currentLive.clear(j);
			}
		  else
			{
			  switch (vReg.getClass())
				{
				case vrcInteger:
				  nGeneralAlive++;
				  break;
				case vrcFloatingPoint:
				case vrcFixedPoint:
				  nFloatingPointAlive++;
				  break;
				default:
				  break;
				}
			}
		}
	  
//		  if(node.dfsNum == 8) printf("\n________Begin Node %d________\n", node.dfsNum);

		InstructionList& instructions = node.getInstructions();
	  for (InstructionList::iterator i = instructions.end(); !instructions.done(i); i = instructions.retreat(i))
		{
		  Instruction&       instruction = instructions.get(i);
		  InstructionUse*    useBegin    = instruction.getInstructionUseBegin();
		  InstructionUse*    useEnd      = instruction.getInstructionUseEnd();
		  InstructionUse*    usePtr;
		  InstructionDefine* defBegin    = instruction.getInstructionDefineBegin();
		  InstructionDefine* defEnd      = instruction.getInstructionDefineEnd();
		  InstructionDefine* defPtr;

//		if(node.dfsNum == 8)  { printf("\n");
//		instruction.printPretty(stdout);
//		printf("\n"); }

		  // Handle definitions
		  for (defPtr = defBegin; defPtr < defEnd; defPtr++)
			if (defPtr->isVirtualRegister())
			  {
				VirtualRegister& vReg = defPtr->getVirtualRegister();
				currentLive.clear(vReg.getRegisterIndex());
				switch (vReg.getClass())
				  {
				  case vrcInteger:
					nGeneralAlive--;
					break;
				  case vrcFloatingPoint:
				  case vrcFixedPoint:
					nFloatingPointAlive--;
					break;
				  default:
					break;
				  }
			  }

		  // Check for deaths
		  for (usePtr = useBegin; usePtr < useEnd; usePtr++)
			if (usePtr->isVirtualRegister())
			  {
				VirtualRegister& vReg = usePtr->getVirtualRegister();
				if (!currentLive.test(vReg.getRegisterIndex()))
				  // This is the last use of this register.
				  {
					currentLive.set(vReg.getRegisterIndex());
					switch (vReg.getClass())
					  {
					  case vrcInteger:
						nGeneralAlive++;
						while (/*(nGeneralAlive > NUMBER_OF_GREGISTERS) &&*/ !grNeedLoad.empty())
						  {
							PRUint32 toLoad = grNeedLoad.get();
							currentLive.clear(toLoad);
							nGeneralAlive--;
							
							VirtualRegister& nReg = vRegManager.getVirtualRegister(toLoad);
							Instruction& lastUsingInstruction = *nReg.spillInfo.lastUsingInstruction;
							emitter.emitLoadAfter(*lastUsingInstruction.getPrimitive(), lastUsingInstruction.getLinks().prev,
												  nReg.getAlias(), *nReg.equivalentRegister[vrcStackSlot]);
							nReg.releaseSelf();
						  }
						break;
					  case vrcFloatingPoint:
					  case vrcFixedPoint:
						nFloatingPointAlive++;
						while (/*(nFloatingPointAlive > NUMBER_OF_FPREGISTERS) &&*/ !fpNeedLoad.empty())
						  {
							PRUint32 toLoad = fpNeedLoad.get();
							currentLive.clear(toLoad);
							nFloatingPointAlive--;
							
							VirtualRegister& nReg = vRegManager.getVirtualRegister(toLoad);
							Instruction& lastUsingInstruction = *nReg.spillInfo.lastUsingInstruction;
							emitter.emitLoadAfter(*lastUsingInstruction.getPrimitive(), lastUsingInstruction.getLinks().prev,
												  nReg.getAlias(), *nReg.equivalentRegister[vrcStackSlot]);
							nReg.releaseSelf();
						  }
						break;
					  default:
						break;
					  }
				  }
			  }

		  // Handle uses
		  for (usePtr = useBegin; usePtr < useEnd; usePtr++)
			if (usePtr->isVirtualRegister())
			  {
				VirtualRegister& vReg = usePtr->getVirtualRegister();
				PRUint32 registerIndex = vReg.getRegisterIndex();

				if (vReg.spillInfo.willSpill) {
#if defined(GENERATE_FOR_X86)
				  if (!instruction.switchUseToSpill((usePtr - useBegin), *vReg.equivalentRegister[vrcStackSlot]))
#endif
					{
					  switch (vReg.getClass())
						{
						case vrcInteger:
						  if (!grNeedLoad.test(registerIndex))
							{
							  grNeedLoad.put(registerIndex);
							  VirtualRegister& alias = vRegManager.newVirtualRegister(vrcInteger);
							  if (vReg.isPreColored())
								  alias.preColorRegister(vReg.getPreColor());
							  /* if (vReg.hasSpecialInterference) {
								  alias.specialInterference.sizeTo(NUMBER_OF_REGISTERS);
								  alias.specialInterference = vReg.specialInterference;
								  alias.hasSpecialInterference = true;
							  } */
							  vReg.setAlias(alias);
							  vReg.retainSelf();
							}
						  break;
						case vrcFloatingPoint:
						case vrcFixedPoint:
						  if (!fpNeedLoad.test(registerIndex))
							{
							  fpNeedLoad.put(registerIndex);
							  VirtualRegister& alias = vRegManager.newVirtualRegister(vReg.getClass());
							  if (vReg.isPreColored())
								  alias.preColorRegister(vReg.getPreColor());
							  /*if (vReg.hasSpecialInterference) {
								  alias.specialInterference.sizeTo(NUMBER_OF_REGISTERS);
								  alias.specialInterference = vReg.specialInterference;
								  alias.hasSpecialInterference = true;
							  } */
							  vReg.setAlias(alias);
							  vReg.retainSelf();
							}
						  break;
						default:
						  break;
						}
					  usePtr->getVirtualRegisterPtr().initialize(vReg.getAlias());
					  usedInThisInstruction.set(registerIndex);
					  vReg.spillInfo.lastUsingInstruction = &instruction;
					}
				  currentLive.clear(registerIndex);
				} else { // will not spill
				  currentLive.set(registerIndex);
				}
			  }
		  
		  // Handle definitions
		  for (defPtr = defBegin; defPtr < defEnd; defPtr++)
			if (defPtr->isVirtualRegister())
			  {
				VirtualRegister& vReg = defPtr->getVirtualRegister();
				
				if (vReg.spillInfo.willSpill)
#if defined(GENERATE_FOR_X86)
				  if (!instruction.switchDefineToSpill((defPtr - defBegin), *vReg.equivalentRegister[vrcStackSlot]))
#endif
					{
					  if (usedInThisInstruction.test(vReg.getRegisterIndex()))
						// this virtualRegister was used in this instruction and is also defined. We need to move 
						// this virtual register to its alias first and then save it to memory.
						{
						  emitter.emitStoreAfter(*instruction.getPrimitive(), &instruction.getLinks(), 
												 vReg.getAlias(), *vReg.equivalentRegister[vrcStackSlot]);
						  defPtr->getVirtualRegisterPtr().initialize(vReg.getAlias());
						}
					  else
						{
						  emitter.emitStoreAfter(*instruction.getPrimitive(), &instruction.getLinks(), 
												 vReg, *vReg.equivalentRegister[vrcStackSlot]);
						}
					}
			  }
		}
	  while (!grNeedLoad.empty())
		{
		  PRUint32 nl = grNeedLoad.get();
		  VirtualRegister& nlReg = vRegManager.getVirtualRegister(nl);
		  Instruction& lastUse = *nlReg.spillInfo.lastUsingInstruction;
		  
		  emitter.emitLoadAfter(*lastUse.getPrimitive(), lastUse.getLinks().prev, 
								nlReg.getAlias(), *nlReg.equivalentRegister[vrcStackSlot]);
		  nlReg.releaseSelf();
		}
	  while (!fpNeedLoad.empty())
		{
		  PRUint32 nl = fpNeedLoad.get();
		  VirtualRegister& nlReg = vRegManager.getVirtualRegister(nl);
		  Instruction& lastUse = *nlReg.spillInfo.lastUsingInstruction;

		  emitter.emitLoadAfter(*lastUse.getPrimitive(), lastUse.getLinks().prev, 
								nlReg.getAlias(), *nlReg.equivalentRegister[vrcStackSlot]);
		  nlReg.releaseSelf();
		}

//	  if(node.dfsNum == 8) printf("\n________End Node %d________\n", node.dfsNum);

	}
}
