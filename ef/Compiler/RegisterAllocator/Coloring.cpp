/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "Coloring.h"
#include "VirtualRegister.h"
#include "FastBitSet.h"
#include "FastBitMatrix.h"
#include "CpuInfo.h"

bool Coloring::
assignRegisters(FastBitMatrix& interferenceMatrix)
{
  PRUint32 *stackPtr = new(pool) PRUint32[vRegManager.count()];

  return select(interferenceMatrix, stackPtr, simplify(interferenceMatrix, stackPtr));
}

PRInt32 Coloring::
getLowestSpillCostRegister(FastBitSet& bitset)
{
  PRInt32 lowest = bitset.firstOne();
  if (lowest != -1)
	{
	  Flt32 cost = vRegManager.getVirtualRegister(lowest).spillInfo.spillCost;
	  for (PRInt32 r = bitset.nextOne(lowest); r != -1; r = bitset.nextOne(r))
		{
		  VirtualRegister& vReg = vRegManager.getVirtualRegister(r);
		  if (!vReg.spillInfo.infiniteSpillCost && (vReg.spillInfo.spillCost < cost))
			{
			  cost = vReg.spillInfo.spillCost;
			  lowest = r;
			}
		}
	}
  return lowest;
}

PRUint32* Coloring::
simplify(FastBitMatrix interferenceMatrix, PRUint32* stackPtr)
{
  // first we construct the sets low and high. low contains all nodes of degree
  // inferior to the number of register available on the processor. All the
  // nodes with an high degree and a finite spill cost are placed in high.
  // Nodes of high degree and infinite spill cost are not included in either sets.

  PRUint32 nRegisters = vRegManager.count();
  FastBitSet low(pool, nRegisters);
  FastBitSet high(pool, nRegisters);
  FastBitSet stack(pool, nRegisters);
  bool all_low = true;

  for (VirtualRegisterManager::iterator i = vRegManager.begin(); !vRegManager.done(i); i = vRegManager.advance(i))
	{
	  VirtualRegister& vReg = vRegManager.getVirtualRegister(i);
	  
	  if (vReg.getClass() == vrcStackSlot)
		{
		  stack.set(i);
		  vReg.colorRegister(nRegisters);
		}
	  else
		{
		  if (vReg.colorInfo.interferenceDegree < NUMBER_OF_REGISTERS) {
			low.set(i);
		  } else {
			high.set(i);
			all_low = false;
		  }

		  // Set coloring info.
		  vReg.spillInfo.willSpill = false;

		  switch(vReg.getClass())
			{
			case vrcInteger:
			  vReg.colorRegister(LAST_GREGISTER + 1);
			  break;
			case vrcFloatingPoint:
			case vrcFixedPoint:
			  vReg.colorRegister(LAST_FPREGISTER + 1);
			  break;
			default:
			  PR_ASSERT(false); // Cannot happen.
			}
		}
	}

  // push the stack registers
  PRInt32 j;
  for (j = stack.firstOne(); j != -1; j = stack.nextOne(j))
	  *stackPtr++ = j;
  
  /* Simplify handling when every node in the interference graph has degree less
   * than K and is therefore trivially K-colorable.  In this case, no spilling
   * code is necessary.
   */
  if (all_low)
  {
	  for (j = low.firstOne(); j != -1; j = low.nextOne(j))
		  *stackPtr++ = j;
	  return stackPtr;
  }


  // simplify
  PRInt32 r = low.firstOne();
  while (true)
	{
	  /*
	   * Remove any node of degree less than K from the graph, since that node is 
	   * K-colorable. That deletion may, in turn, cause the degree of other nodes
	   * in the graph to fall below K and these nodes are then removed.
	   */
	  while (r != -1)
		{
		  VirtualRegister& vReg = vRegManager.getVirtualRegister(r);

		  /* update low and high */
		  FastBitSet inter(interferenceMatrix.getRow(r), nRegisters);
		  for (j = inter.firstOne(); j != -1; j = inter.nextOne(j))
			{
			  VirtualRegister& neighbor = vRegManager.getVirtualRegister(j);
			  // if the new interference degree of one of his neighbor becomes
			  // NUMBER_OF_REGISTERS - 1 then it is added to the set 'low'.
			  
			  PRUint32 maxInterference = 0;
			  switch (neighbor.getClass())
				{
				case vrcInteger:
				  maxInterference = NUMBER_OF_GREGISTERS;
				  break;
				case vrcFloatingPoint:
				case vrcFixedPoint:
				  maxInterference = NUMBER_OF_FPREGISTERS;
				  break;
				default:
				  PR_ASSERT(false);
				}
			  if ((vRegManager.getVirtualRegister(j).colorInfo.interferenceDegree-- == maxInterference))
				{
				  high.clear(j);
				  low.set(j);
				}
			  vReg.colorInfo.interferenceDegree--;
			  interferenceMatrix.clear(r, j);
			  interferenceMatrix.clear(j, r);
			}
		  low.clear(r);

		  // Push this register.
		  *stackPtr++ = r;

		  r = low.firstOne();
		}

	  /*
	   * The absence of any node with degree less than K means that a spill might be
	   * necessary.  Use spill cost heurestics to choose the register node to be
	   * removed from the graph.
	   */
	  if ((r = getLowestSpillCostRegister(high)) != -1)
		{
		  high.clear(r);
		}
	  else
		break;
	}

  return stackPtr;
}

bool Coloring::
select(FastBitMatrix& interferenceMatrix, PRUint32* stackBase, PRUint32* stackPtr)
{
  PRUint32 nRegisters = vRegManager.count();
  FastBitSet usedRegisters(NUMBER_OF_REGISTERS + 1); // usedRegisters if used for both GR & FPR.
  FastBitSet preColoredRegisters(NUMBER_OF_REGISTERS + 1);
  FastBitSet usedStack(nRegisters + 1);
  bool success = true;
  Int32 lastUsedSSR = -1;

  // select
  while (stackPtr != stackBase)
	{
	  // Pop one register.
	  PRUint32 r = *--stackPtr;
	  VirtualRegister& vReg = vRegManager.getVirtualRegister(r);

	  FastBitSet neighbors(interferenceMatrix.getRow(r), nRegisters);
		  
	  if (vReg.getClass() == vrcStackSlot)
		// Stack slots coloring.
		{
		  usedStack.clear();

		  for (PRInt32 i = neighbors.firstOne(); i != -1; i = neighbors.nextOne(i))
			usedStack.set(vRegManager.getVirtualRegister(i).getColor());

		  Int32 color = usedStack.firstZero();
		  vReg.colorRegister(color);
		  if (color > lastUsedSSR)
			  lastUsedSSR = color;
		}
	  else
		// Integer & Floating point register coloring.
		{
		  usedRegisters.clear();
		  preColoredRegisters.clear();
		  
		  for (PRInt32 i = neighbors.firstOne(); i != -1; i = neighbors.nextOne(i))
			{
			  VirtualRegister& nvReg = vRegManager.getVirtualRegister(i);
			  usedRegisters.set(nvReg.getColor());
			  if (nvReg.isPreColored())
				preColoredRegisters.set(nvReg.getPreColor());
			}
		  if (vReg.hasSpecialInterference)
			usedRegisters |= vReg.specialInterference;

		  PRInt8 c = -1;
		  PRInt8 maxColor = 0;
		  PRInt8 firstColor = 0;
		  switch (vReg.getClass())
			{
			case vrcInteger:
			  firstColor = FIRST_GREGISTER;
			  maxColor = LAST_GREGISTER;
			  break;
			case vrcFloatingPoint:
			case vrcFixedPoint:
			  firstColor = FIRST_FPREGISTER;
			  maxColor = LAST_FPREGISTER;
			  break;
			default:
			  PR_ASSERT(false);
			}

		  if (vReg.isPreColored())
			{
			  c = vReg.getPreColor();
			  if (usedRegisters.test(c))
				c = -1;
			}
		  else
			{
			  for (c = usedRegisters.nextZero(firstColor - 1); (c >= 0) && (c <= maxColor) && (preColoredRegisters.test(c)); 
				   c = usedRegisters.nextZero(c)) {}
			}

		  if ((c >= 0) && (c <= maxColor))
			{
			  vReg.colorRegister(c);
			}
		  else
			{
			  VirtualRegister& stackRegister = vRegManager.newVirtualRegister(vrcStackSlot);
			  vReg.equivalentRegister[vrcStackSlot] = &stackRegister;	  
			  vReg.spillInfo.willSpill = true;
			  success = false;
			}
		}
	}
  
#ifdef DEBUG
  if (success)
	{
	  for (VirtualRegisterManager::iterator i = vRegManager.begin(); !vRegManager.done(i); i = vRegManager.advance(i))
		{
		  VirtualRegister& vReg = vRegManager.getVirtualRegister(i);
		  switch (vReg.getClass())
			{
			case vrcInteger:
			  if (vReg.getColor() > LAST_GREGISTER)
				PR_ASSERT(false);
			  break;
			case vrcFloatingPoint:
			case vrcFixedPoint:
#if NUMBER_OF_FPREGISTERS != 0
			  if (vReg.getColor() > LAST_FPREGISTER)
				PR_ASSERT(false);
#endif
			  break;
			default:
			  break;
			}
		}
	}
#endif

  vRegManager.nUsedStackSlots = lastUsedSSR + 1;
  return success;
}
