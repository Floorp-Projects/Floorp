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

#ifndef _VIRTUAL_REGISTER_H_
#define _VIRTUAL_REGISTER_H_

#include "Fundamentals.h"
#include "FastBitMatrix.h"
#include "FastBitSet.h"

class Instruction;
class Pool;

enum VirtualRegisterKind
{
	InvalidRegisterKind,
	IntegerRegister,
	FloatingPointRegister,
	FixedPointRegister,
	MemoryRegister,
	StackSlotRegister,
	MemoryArgumentRegister,
};
const nVirtualRegisterKind =  MemoryArgumentRegister + 1;

// ----------------------------------------------------------------------------
// VirtualRegister

// A VirtualRegisterIndex is an index to the VirtualRegisterManager table
// of handles. It is an invalid index if its value is 'invalidRegisterIndex'.
typedef Int32 VirtualRegisterIndex;
typedef Uint8 VirtualRegisterColor;

const VirtualRegisterIndex invalidRegisterIndex = -1;			// An invalid register index.
const VirtualRegisterColor invalidRegisterColor = 255;			// An invalid register color.

struct VirtualRegisterHandle;

const Flt32 copyCost = 1.;
const Flt32 loadCost = 2.;
const Flt32 storeCost = 2.;

class VirtualRegister
{
public:

	VirtualRegisterIndex index;									// This VirtualRegister's index.
	const VirtualRegisterKind kind;								// This VirtualRegister kind (i.e. IntegerRegister, FloatingPointRegister ...).

	struct SpillCost
	{
		Flt32 copyCosts;
		Flt32 loadOrStoreCosts;
	} spillCosts;

private:

	Instruction* definingInstruction;							// The instruction defining this register.
	VirtualRegisterHandle* handle;								// This VirtualRegister's handle.

	struct 
	{
		VirtualRegisterColor color;								// Register's color
		VirtualRegisterColor preColor;							// PreColor or 255 if it is not precolored.
	} colorInfo;

private:

	VirtualRegister(const VirtualRegister&);					// No copy constructor.
	void operator = (const VirtualRegister&);					// No copy operator.

public:

	// Create a new VirtualRegister
	inline VirtualRegister(VirtualRegisterHandle* handle, VirtualRegisterIndex index, VirtualRegisterKind kind);

	// Set the defining instruction (for code generation).
	void setDefiningInstruction(Instruction& instruction) {definingInstruction = &instruction;}
	// Get the defining instruction (for code generation).
	Instruction* getDefiningInstruction() {return definingInstruction;}

	//
	// Helpers.
	//

	// Get this VirtualRegister's handle.
	VirtualRegisterHandle* getHandle() {return handle;}

	// Set the Register's color.
	void setColor(VirtualRegisterColor color) {colorInfo.color = color;}
	// Set the preColor (by setting the preColor this register becomes a preColored register).
	void setPreColor(VirtualRegisterColor color) {colorInfo.preColor = color;}
	// Return true is this register has been succesfully colored.
	bool isColored() const {return colorInfo.color != invalidRegisterColor;}
	// Return true is this register is preColored. (If this register is not colored then return invalidRegisterColor).
	bool isPreColored() const {return colorInfo.preColor != invalidRegisterColor;}
	// Return this register's color.
	VirtualRegisterColor getColor() const {return colorInfo.color;}
	// Return this register's preColor. (If this register is not preColored then return invalidRegisterColor).
	VirtualRegisterColor getPreColor() const {return colorInfo.preColor;}

#ifdef DEBUG_LOG
	// Print the register in the file f.
	void printPretty(FILE* f) const;
#endif // DEBUG_LOG
};

struct VirtualRegisterHandle
{
	VirtualRegister* 		ptr;
	VirtualRegisterHandle* 	next;
};

// ----------------------------------------------------------------------------
// VirtualRegisterManager

template<class Class>
struct BlocksHeader
{
	const Uint32 numberOfElementsInABlock = 128;				// Number of elements in a block.

	Uint32 								nBlocks;				// Number of available blocks.
	Uint32								next;					// Next available element.
	Class* 								blocks[1];				// Array of block pointers.

	// Allocate another block and return the new this.
	BlocksHeader<Class>* allocateAnotherBlock(Pool& pool);
	// Get the block number for this element index.
	static Uint32 getBlockNumber(Uint32 index) {return index >> 7;}
	// Get the element's pointer at index.
	Class* getElementPtr(Uint32 index);
};

#include "CpuInfo.h"

class VirtualRegisterManager
{
private:

	Pool& 									pool;				// This VirtualRegisterManager's allocation pool.
	BlocksHeader<VirtualRegisterHandle>*	handlesHeader;		// VirtualRegisterHandles blocks' header.
	BlocksHeader<VirtualRegister>*			registersHeader;	// VirtualRegisters blocks' header.
	FastBitMatrix							machineRegisters;	// Matrix of machine registers.
#ifdef DEBUG
	bool 									lock;				// If lock is true then no new register can be created.
#endif // DEBUG

	FastBitSet* 							indexes;			// Register indexes currently used.
	
	Int32 									firstFree;			// First free register index.
	Int32 									biggestUsed;		// Biggest used register index.
  
	VirtualRegisterManager(const VirtualRegisterManager&);		// No copy constructor.
	void operator = (const VirtualRegisterManager&);			// No copy operator.

public:
	// Create a new VirtualRegisterManager.
	VirtualRegisterManager(Pool& pool);

	// Return the maximum number of VirtualRegisters currently in use.
	Uint32 getSize() const {return biggestUsed + 1;}
#ifdef DEBUG
	// Lock the manager. When it is locked no one can create new VirtualRegisters.
	void lockAllocation() {PR_ASSERT(!lock); lock = true;}
	// Unlock the manager.
	void unlockAllocation() {PR_ASSERT(lock); lock = false;}
#endif // DEBUG

	// Return the reference to the VirtualRegister at index.
	inline VirtualRegister& getVirtualRegister(VirtualRegisterIndex index) const;

	// Allocate a new VirtualRegister of kind 'kind'.
	VirtualRegister& newVirtualRegister(VirtualRegisterKind kind);
	// Coalesce the VirtualRegister at index from with the VirtualRegister at index to.
	void coalesceVirtualRegisters(VirtualRegisterIndex from, VirtualRegisterIndex to);
	// PreColor the VirtualRegister at index with color.
	void preColorVirtualRegister(VirtualRegisterIndex index, VirtualRegisterColor color);

	// Return the first valid VirtualRegisterIndex.
	VirtualRegisterIndex first() const {return advance(-1);}
	// Return the next valid VirtualRegister after index.
	inline VirtualRegisterIndex advance(VirtualRegisterIndex index) const {return indexes->nextOne(index);}
	// Return true is this index is outside the array of handles.
	bool done(VirtualRegisterIndex index) const {return (index < 0 || index > biggestUsed);}

	// Return the matrix of valid interferences (i.e. a IntegerRegister interferes only with others IntegerRegisters).
	FastBitMatrix& getValidInterferencesMatrix() const;

#ifdef DEBUG_LOG
	// Print all the VirtualRegisters in this manager.
	void printPretty(FILE* f) const;
#endif // DEBUG_LOG
};

// ----------------------------------------------------------------------------
// VirtualRegisterClass(es)

class VirtualRegisterClass
{
private:
  
	VirtualRegisterClass(const VirtualRegisterClass&);			// No copy constructor.
	void operator = (const VirtualRegisterClass&);				// No copy operator.

public:

	VirtualRegisterKind kind; // This class kind.

	VirtualRegisterClass(VirtualRegisterKind kind) : kind(kind) {}

	// Return the VirtualRegisterKind this class can spill to.
	virtual VirtualRegisterKind willSpillTo() const {return InvalidRegisterKind;}
	// Return the smallest color index this register class can take.
	virtual Uint8 getMinColor() const = 0;
	// Return the bigest color index this register class can take.
	virtual Uint8 getMaxColor() const = 0;
	// Return the number of color available for this class.
	Uint8 getNumberOfColor() const {return getMaxColor() - getMinColor() + 1;}
	// Return true if this class canSpill;
	bool canSpill() const {return willSpillTo() != InvalidRegisterKind;}

	// Return the kind of register for the given color.
	static VirtualRegisterKind getKind(VirtualRegisterColor color);
#ifdef DEBUG_LOG
	// Print the class string for the VirtualRegisterClass.
	virtual void printPretty(FILE* f) const = 0;
#endif // DEBUG_LOG
};

// virtualRegisterClasses is an array of VirtualRegisterClass instances.
// Each instance is a different class of registers (i.e. Integer, FP, memory).
extern const VirtualRegisterClass* virtualRegisterClasses[];

//
// An IntegerRegister's possible colors are [FIRST_GREGISTER ... LAST_GREGISTER]. 
// It can spill in a MemoryRegister.
//
class IntegerRegisterClass : public VirtualRegisterClass
{
public:
	IntegerRegisterClass() : VirtualRegisterClass(IntegerRegister) {}

	virtual VirtualRegisterKind willSpillTo() const {return MemoryRegister;}
	virtual Uint8 getMinColor() const {return FIRST_GREGISTER;}
	virtual Uint8 getMaxColor() const {return LAST_GREGISTER;}

#ifdef DEBUG_LOG
	virtual void printPretty(FILE* f) const {fprintf(f, "integer register ");}
#endif // DEBUG_LOG
};

//
// A FloatingRegister's possible colors are [FIRST_FPREGISTER ... LAST_FPREGISTER]. 
// It can spill in a MemoryRegister.
//
class FloatingPointRegisterClass : public VirtualRegisterClass
{
public:
	FloatingPointRegisterClass() : VirtualRegisterClass(FloatingPointRegister) {}

	virtual VirtualRegisterKind willSpillTo() const {return MemoryRegister;}
	virtual Uint8 getMinColor() const {return FIRST_FPREGISTER;}
	virtual Uint8 getMaxColor() const {return LAST_FPREGISTER;}

#ifdef DEBUG_LOG
	virtual void printPretty(FILE* f) const {fprintf(f, "floating point register ");}
#endif // DEBUG_LOG
};

//
// A MemoryRegister has an infinite color space. It cannot spill !
//
class MemoryRegisterClass : public VirtualRegisterClass
{
public:
	MemoryRegisterClass() : VirtualRegisterClass(MemoryRegister) {}

	virtual Uint8 getMinColor() const {return 0;}
	virtual Uint8 getMaxColor() const {return ~0;}				// Infinite space for a memory register.

#ifdef DEBUG_LOG
	virtual void printPretty(FILE* f) const {fprintf(f, "memory register ");}
#endif // DEBUG_LOG
};

// ----------------------------------------------------------------------------
// Inlines

inline VirtualRegister& VirtualRegisterManager::getVirtualRegister(VirtualRegisterIndex index) const 
{
	PR_ASSERT(indexes->test(index)); 
	return *registersHeader->getElementPtr(index);
}

inline VirtualRegister::VirtualRegister(VirtualRegisterHandle* handle, VirtualRegisterIndex index, VirtualRegisterKind kind) :
	index(index), kind(kind), definingInstruction(NULL), handle(handle) 
{
	setColor(invalidRegisterColor);
	setPreColor(invalidRegisterColor);
}

//
// Allocate another block and return the new this. 
//
template<class Class> 
BlocksHeader<Class>* BlocksHeader<Class>::allocateAnotherBlock(Pool& pool)
{
	// Get the number of 32bits words needed for this block. A block is composed by:
	// [BlockHeader] (nBlocks-1)*[pointers to block] numberOfElementsInABlock*[element Class].
	Uint32 blockSize = ((sizeof(BlocksHeader<Class>) + nBlocks * sizeof(Uint32) + 
						 (numberOfElementsInABlock * sizeof(Class))) + 3 & -4) >> 2;
	Uint32* ptr = new(pool) Uint32[blockSize];
	fill(ptr, &ptr[blockSize], 0); 
 
	BlocksHeader<Class>* newThis = static_cast<BlocksHeader<Class>*>(ptr);
	*newThis = *this;

	if (nBlocks > 0)
		copy(&blocks[1], &blocks[nBlocks], &newThis->blocks[1]);

	newThis->blocks[nBlocks] = static_cast<Class *>(&newThis->blocks[nBlocks + 1]);
	newThis->nBlocks++;

	return newThis;
}

//
// Get the element's pointer at index.
//
template<class Class>
Class* BlocksHeader<Class>::getElementPtr(Uint32 index) 
{
	PR_ASSERT((getBlockNumber(index) < nBlocks) && (index < next)); 
	return &(blocks[getBlockNumber(index)])[index & (numberOfElementsInABlock - 1)];
}

#endif // _VIRTUAL_REGISTER_H_
