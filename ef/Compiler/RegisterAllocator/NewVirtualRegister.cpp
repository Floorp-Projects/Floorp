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
#include "FastBitMatrix.h"
#include "FastBitSet.h"

// ----------------------------------------------------------------------------
// VirtualRegister

#ifdef DEBUG_LOG
//
// Print this VirtualRegister.
//
void VirtualRegister::printPretty(FILE* f) const
{
	virtualRegisterClasses[kind]->printPretty(f);
	fprintf(f, "%d: ", index);

	if (isPreColored())
		fprintf(f, "preColor = %d ", getPreColor());
	if (isColored())
		fprintf(f, "color = %d ", getColor());
	else
		fprintf(f, "not yet colored ");

	fprintf(f, "\n");
}
#endif // DEBUG_LOG

// ----------------------------------------------------------------------------
// VirtualRegisterManager

static BlocksHeader<VirtualRegisterHandle> nullVirtualRegisterHandlesHeader = {0, 0, {NULL}};
static BlocksHeader<VirtualRegister> nullVirtualRegistersHeader = {0, 0, {NULL}};

//
// Create a new VirtualRegisterManager.
//
VirtualRegisterManager::VirtualRegisterManager(Pool& pool) : 
	pool(pool), machineRegisters(pool, NUMBER_OF_REGISTERS, NUMBER_OF_REGISTERS)
{
	handlesHeader = nullVirtualRegisterHandlesHeader.allocateAnotherBlock(pool);
	registersHeader = nullVirtualRegistersHeader.allocateAnotherBlock(pool);

	indexes = new(pool) FastBitSet(pool, registersHeader->numberOfElementsInABlock);
	firstFree = 0;
	biggestUsed = -1;

	DEBUG_ONLY(lock = false);
}

inline void *operator new(size_t, VirtualRegister* ptr) {return ptr;}

//
// Allocate a new VirtualRegister of kind 'kind'.
//
VirtualRegister& VirtualRegisterManager::newVirtualRegister(VirtualRegisterKind kind)
{
	// If we are lock, no new VirtualRegister can be created.
	PR_ASSERT(!lock);

	// Get a new VirtualRegister.
	Int32 registerIndex = firstFree;
	indexes->set(registerIndex);
	firstFree = indexes->nextZero(firstFree);

	if (firstFree == -1 || registersHeader->getBlockNumber(registerIndex) >= registersHeader->nBlocks) {
		registersHeader = registersHeader->allocateAnotherBlock(pool);
		FastBitSet& newIndexes = *new(pool) FastBitSet(pool, registersHeader->nBlocks * registersHeader->numberOfElementsInABlock);
		newIndexes = *indexes;
		indexes = &newIndexes;
		firstFree = indexes->nextZero(registerIndex);
	}
	if (Uint32(registerIndex) == registersHeader->next)
		registersHeader->next++;

	if (registerIndex > biggestUsed)
		biggestUsed = registerIndex;

	// Get a new handle.
	Uint32 handleIndex = handlesHeader->next++;
	if (handlesHeader->getBlockNumber(handleIndex) >= handlesHeader->nBlocks)
		handlesHeader = handlesHeader->allocateAnotherBlock(pool);

	VirtualRegisterHandle* handle = handlesHeader->getElementPtr(handleIndex);

	VirtualRegister& vReg = *new(registersHeader->getElementPtr(registerIndex)) VirtualRegister(handle, registerIndex, kind);

	handle->ptr = &vReg;
	handle->next = handle;

	return vReg;
}

//
// Coalesce the VirtualRegister at index from with the VirtualRegister at index to.
//
void VirtualRegisterManager::coalesceVirtualRegisters(VirtualRegisterIndex from, VirtualRegisterIndex to)
{
	// When we coalesce the VirtualRegister 'from' to the VirtualRegister 'to' we must
	// update all from's handle to point to. 'from' has an unique handle index, but this
	// handle is chain to all the handle also pointing it.

	VirtualRegister* toVR = &getVirtualRegister(to);

	VirtualRegisterHandle* beginOfToChain = toVR->getHandle();
	VirtualRegisterHandle* endOfToChain = beginOfToChain;

	// Get the end of to's handle chain.
	while (endOfToChain->next != beginOfToChain)
		endOfToChain = endOfToChain->next;

	VirtualRegister* fromVR = &getVirtualRegister(from);
	VirtualRegisterHandle* beginOfFromChain = fromVR->getHandle();
	VirtualRegisterHandle* endOfFromChain = beginOfFromChain;
  
	// For each from's handle update the pointer.
	while (true) {
		endOfFromChain->ptr = toVR;

		if (endOfFromChain->next == beginOfFromChain)
			break;

		endOfFromChain = endOfFromChain->next;
	}

	// Merge the chains.
	endOfToChain->next = beginOfFromChain;
	endOfFromChain->next = beginOfToChain;

	// Free the from register.
	indexes->clear(from);
	if (from < firstFree)
		firstFree = from;

	if (from == biggestUsed)
		biggestUsed = indexes->previousOne(from);

	if (fromVR->isPreColored())
		machineRegisters.clear(fromVR->getPreColor(), fromVR->index);
}

//
// PreColor the VirtualRegister at index with color.
//
void VirtualRegisterManager::preColorVirtualRegister(VirtualRegisterIndex index, VirtualRegisterColor color)
{
	machineRegisters.set(color, index);
	getVirtualRegister(index).setPreColor(color);
}

//
// Return a Matrix of valid interferences.
//
FastBitMatrix& VirtualRegisterManager::getValidInterferencesMatrix() const
{
	const Uint32 matrixSize = getSize();
	FastBitMatrix& matrix = *new(pool) FastBitMatrix(pool, matrixSize, matrixSize);
	FastBitSet* classesIndex = new FastBitSet[nVirtualRegisterKind](pool, matrixSize);

	for (VirtualRegisterIndex i = first(); !done(i); i = advance(i))
		classesIndex[getVirtualRegister(i).kind].set(i);

	for (VirtualRegisterIndex j = first(); !done(j); j = advance(j))
		matrix.copyRows(classesIndex[getVirtualRegister(j).kind], j);

	return matrix;
}

#ifdef DEBUG_LOG
//
// Print all the VirtualRegisters in this manager.
//
void VirtualRegisterManager::printPretty(FILE* f) const
{
	fprintf(f, "----- VirtualRegisters -----\n");
	for (VirtualRegisterIndex i = first(); !done(i); i = advance(i))
		getVirtualRegister(i).printPretty(f); 

	fprintf(f, "----- MachineRegisters -----\n");
	for (Uint32 j = 0; j < NUMBER_OF_REGISTERS; j++) {
		FastBitSet preColoredRegisters(machineRegisters.getRowBits(j), NUMBER_OF_REGISTERS);
		if (!preColoredRegisters.empty()) {
			fprintf(f, "preColored to %d: ", j);
			preColoredRegisters.printPrettyOnes(f);
		}
	}

	fprintf(f, "----------------------------\n");
}
#endif // DEBUG_LOG

// ----------------------------------------------------------------------------
// VirtualRegisterClass

static IntegerRegisterClass			integerRegister;
static FloatingPointRegisterClass	floatingPointRegister;
static MemoryRegisterClass			memoryRegister;

const VirtualRegisterClass* virtualRegisterClasses[nVirtualRegisterKind] = 
{
	NULL,					// InvalidRegisterKind
	&integerRegister,
	&floatingPointRegister,
	NULL,					// FixedPointRegister
	&memoryRegister,
	NULL,					// StackSlotRegister
	NULL,					// MemoryArgumentRegister 
};

//
// Return the kind of register for the given color.
//
VirtualRegisterKind VirtualRegisterClass::getKind(VirtualRegisterColor color)
{
	for (Uint32 i = 0; i < nVirtualRegisterKind; i++)
		if (virtualRegisterClasses[i] != NULL) {
			const VirtualRegisterClass& vrClass = *virtualRegisterClasses[i];
			if (color >= vrClass.getMinColor() && color <= vrClass.getMaxColor())
				return vrClass.kind;
		}
	return InvalidRegisterKind;
}
