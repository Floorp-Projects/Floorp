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

#include "Fundamentals.h"
#include "VirtualRegister.h"
#include "CpuInfo.h"



/*
 *--------------------------VirtualRegister.cpp--------------------------
 *
 * VirtualRegister::resetColoringInfo --
 *
 *  Reset all coloring information for this VirtualRegister.
 *
 *-----------------------------------------------------------------------
 */

void VirtualRegister::
resetColoringInfo()
{
  spillInfo.nLoads = 0.0;
  spillInfo.nStores = 0.0;
  spillInfo.nCopies = 0.0;
  spillInfo.infiniteSpillCost = false;
  spillInfo.lastUsingInstruction = 0;
  spillInfo.liveLength = 0;
#if DEBUG
  colorInfo.color = 0;
  colorInfo.interferenceDegree = 0;
#endif
}

#define INITIAL_VR_ARRAY_SIZE 128


/*
 *--------------------------VirtualRegister.cpp--------------------------
 *
 * VirtualRegisterManager::VirtualRegisterManager --
 *
 *   VirtualRegisterManager Constructor.
 *
 *-----------------------------------------------------------------------
 */
VirtualRegisterManager::
VirtualRegisterManager(Pool& p) : pool(p)
{
  virtualRegisters = new(pool) VirtualRegister*[INITIAL_VR_ARRAY_SIZE];
  machineRegisters = new(pool) VirtualRegister*[NUMBER_OF_REGISTERS];
  nAllocatedRegisters = INITIAL_VR_ARRAY_SIZE;
  firstFreeRegisterIndex = 0;
  size = 0;

  fill(virtualRegisters, &virtualRegisters[nAllocatedRegisters], (VirtualRegister *)0);
  fill(machineRegisters, &machineRegisters[NUMBER_OF_REGISTERS], (VirtualRegister *)0);

  nUsedCalleeSavedRegisters = 0;
  nUsedStackSlots = 0;
}


/*
 *--------------------------VirtualRegister.cpp--------------------------
 *
 * VirtualRegisterManager::resize --
 *
 *   Resize the VirtualRegister array to newSize.
 *
 *-----------------------------------------------------------------------
 */
void VirtualRegisterManager::
resize(PRUint32 newSize)
{
  VirtualRegister** newVirtualRegisters = new(pool) VirtualRegister*[newSize];

  copy(virtualRegisters, &virtualRegisters[size], newVirtualRegisters);
  fill(&newVirtualRegisters[size], &newVirtualRegisters[newSize], (VirtualRegister *)0);

  virtualRegisters = newVirtualRegisters;
  nAllocatedRegisters = newSize;

  // Update the VirtualRegisterAddress in all the VirtualRegisterPtrs referencing valid VirtualRegisters.
  for (iterator i = begin(); !done(i); i = advance(i))
	{
	  VirtualRegister& vReg = *virtualRegisters[i];
	  DoublyLinkedList<VirtualRegisterPtr>& owners = vReg.getVirtualRegisterPtrs();

	  for (DoublyLinkedList<VirtualRegisterPtr>::iterator j = owners.begin(); !owners.done(j); j = owners.advance(j))
		owners.get(j).virtualRegisterAddress = &vReg;
	}
}


/*
 *--------------------------VirtualRegister.cpp--------------------------
 *
 * VirtualRegisterManager::newVirtualRegister --
 *
 *   Return a new VirtualRegister.
 *
 *-----------------------------------------------------------------------
 */
VirtualRegister& VirtualRegisterManager::
newVirtualRegister(VRClass registerClass)
{
  VirtualRegister& newVR = *new(pool) VirtualRegister(*this, firstFreeRegisterIndex, registerClass);
  virtualRegisters[firstFreeRegisterIndex++] = &newVR;

  for (; firstFreeRegisterIndex < size; firstFreeRegisterIndex++)
	if (virtualRegisters[firstFreeRegisterIndex] == NULL)
	  // This slot is free.
	  break;

  if (firstFreeRegisterIndex > size)
	{
	  size = firstFreeRegisterIndex;
	  if (size >= nAllocatedRegisters)
		resize(2 * nAllocatedRegisters);
	}

  return newVR;
}


/*
 *--------------------------VirtualRegister.cpp--------------------------
 *
 * VirtualRegisterManager::getMachineRegister --
 *
 *   Return the Machine register r(color).
 *
 *-----------------------------------------------------------------------
 */
VirtualRegister& VirtualRegisterManager::
getMachineRegister(PRUint8 color)
{
  if (machineRegisters[color] == NULL)
	{
	  VRClass registerClass = vrcUnspecified;

	  if (color <= LAST_GREGISTER)
		registerClass = vrcInteger;
#if NUMBER_OF_FPREGISTERS != 0
	  else if (color < LAST_FPREGISTER)
		registerClass = vrcFloatingPoint;
#endif

	  VirtualRegister& machineRegister = newVirtualRegister(registerClass);
	  machineRegister.isMachineRegister = true;
	  machineRegisters[color] = &machineRegister;
	}

  return *machineRegisters[color];
}


/*
 *--------------------------VirtualRegister.cpp--------------------------
 *
 * VirtualRegisterManager::freeVirtualRegister --
 *
 *   Free this VirtualRegister and update the firstFreeRegisterIndex 
 *   field for the next VirtualRegister to be allocated.
 *
 *-----------------------------------------------------------------------
 */
void VirtualRegisterManager::
freeVirtualRegister(PRUint32 index)
{
  PR_ASSERT((index < size) && (virtualRegisters[index] != NULL) && !virtualRegisters[index]->isReferenced());
  
  if (index < firstFreeRegisterIndex)
	firstFreeRegisterIndex = index;

  virtualRegisters[index] = NULL;
}


/*
 *--------------------------VirtualRegister.cpp--------------------------
 *
 * VirtualRegisterManager::compactMemory --
 *
 *   Try to remove any gaps in the VirtualRegister array.
 *
 *-----------------------------------------------------------------------
 */
void VirtualRegisterManager::
compactMemory()
{
  PRInt32 index = count();

  while ((--index >= 0) && (index > PRInt32(firstFreeRegisterIndex)))
	  if (virtualRegisters[index] != NULL) {
		  VirtualRegister& vReg = newVirtualRegister(virtualRegisters[index]->getClass());
		
		  if (virtualRegisters[index]->isPreColored())
			  vReg.preColorRegister(virtualRegisters[index]->getPreColor());

		  moveVirtualRegister(index, vReg.getRegisterIndex());
	  }
}


/*
 *--------------------------VirtualRegister.cpp--------------------------
 *
 * VirtualRegisterManager::checkForVirtualRegisterDeath --
 *
 *   If the VirtualRegister at index is not referenced any more, free it.
 *
 *-----------------------------------------------------------------------
 */
void VirtualRegisterManager::
checkForVirtualRegisterDeath(PRUint32 index)
{
  PR_ASSERT((index < size) && (virtualRegisters[index] != NULL));

  if (!virtualRegisters[index]->isReferenced())
	freeVirtualRegister(index);
}


/*
 *--------------------------VirtualRegister.cpp--------------------------
 *
 * VirtualRegisterManager::moveVirtualRegister --
 *
 *   Update all the VirtualRegisterPtrs pointing to source to point to dest.
 *   Free source when done.
 *
 *-----------------------------------------------------------------------
 */
void VirtualRegisterManager::
moveVirtualRegister(PRUint32 source, PRUint32 dest)
{
  PR_ASSERT(source != dest);
  PR_ASSERT((source < size) && (virtualRegisters[source] != NULL));
  PR_ASSERT((dest < size) && (virtualRegisters[dest] != NULL));

  DoublyLinkedList<VirtualRegisterPtr>& sourceOwners = virtualRegisters[source]->getVirtualRegisterPtrs();
  DoublyLinkedList<VirtualRegisterPtr>& destOwners = virtualRegisters[dest]->getVirtualRegisterPtrs();

  for (DoublyLinkedList<VirtualRegisterPtr>::iterator i = sourceOwners.begin(); !sourceOwners.done(i);)
	{
	  VirtualRegisterPtr& vRegPtr = sourceOwners.get(i);
	  i = sourceOwners.advance(i);
	  vRegPtr.remove();
	  vRegPtr.virtualRegisterAddress = virtualRegisters[dest];
	  destOwners.addLast(vRegPtr);
	}
  
  freeVirtualRegister(source);
}

