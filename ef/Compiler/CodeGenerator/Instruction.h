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
// Instruction.h
//
// Scott M. Silver
// Peter Desantis
// Laurent Morichetti
//
// Interface for schedulable and formattable instructions.

// Summary
//
// Instructions are medium weight representations of actual native instructions
// on a machine.  They are manipulated for register allocation, scheduling
// and eventually to output themselves to memory.
//
// The ideas is that subclasses of Instruction (or one of the "helper" subclasses)
// are the basic unit of "cross-platform" manipulation.  They contain
// all resources consumed or created (ie registers, memory, etc).
//
// Each instruction must implement a series of "interface" methods which
// are essentially queries, so that subsequent users of Instructions can 
// make intelligent decision. (eg. whether an instruction is a call, so
// the register allocator can save non-volatiles)

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "prtypes.h"
#include "Primitives.h"
#include "CpuInfo.h"

class Instruction;
class VirtualRegister;
class InstructionEmitter;

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊInstructionUse/Define ¥
#endif

// InstructionUseOrDefine is a class which represents the
// use or definition of a resource (either a store, cond, or
// register resource).  Assigned to each use and define
// is a ResourceName which is the "name" of the resource
// being use, eg a virtual register.  In addition InstructionUse's
// (not defines) contain (if available) the definer of the resource
// they are using.  This is so that resources can be defined multiple 
// times, but (data) dependencies remain accurate.
typedef enum 
{
	udNone = 0,
	udUninitialized,
	udStore,
	udCond,
	udOrder,
	udRegister
} UseDefineKind;

struct ResourceName
{
	VirtualRegisterPtr 	vr;
	DataNode*			dp;
	Instruction*		instruction;
};

struct InstructionUseOrDefine
{
	UseDefineKind	kind;	
	ResourceName	name;	
	
	inline PRUint32 			getRegisterIndex() 		{ assert(isVirtualRegister()); return getVirtualRegister().getRegisterIndex(); }
	inline VirtualRegister& 	getVirtualRegister() 	{ assert(isVirtualRegister()); return (name.vr.getVirtualRegister()); }
	inline VirtualRegisterPtr& 	getVirtualRegisterPtr() { assert(isVirtualRegister()); return (name.vr); }
	inline bool 				isVirtualRegister()		{ return (kind >= udRegister); }
};

struct InstructionUse : InstructionUseOrDefine
{
	Instruction*	src;
	
	Instruction& 	getDefiningInstruction() { return (*src); }
	void 			setDefiningInstruction(Instruction& inInstruction) { src = &inInstruction; }
};

struct InstructionDefine : InstructionUseOrDefine
{
};

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊInstructionFlags ¥
#endif

// This is a bitmap of interesting things an instruction might do
typedef Uint16 InstructionFlags;
enum
{
	ifNone                = 0x0000,		// default
	ifModCC 	          = 0x0001,		// defines a condition edge (eg. cmp)
	ifModStore	          = 0x0002,		// modifies the store (eg. st)
	ifCopy		          = 0x0004,		// copies its uses to its defines (eg mov)
	ifExternalInsn        = 0x0008,		// used by the register allocator to keep track of uses/defines outside of a function
	ifCall				  = 0x0010,		// makes a call - indicates that non-volatile registers should be saved before this instruction (eg call)
	ifSpecialCall         = 0x0020		// FIX-ME Laurent!!! If you ad a new flag please document it!!!
};

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊInstruction ¥
#endif

class Instruction :
	public DoublyLinkedEntry<Instruction>		// Inserted into a list of scheduled instructions
{
public:
								Instruction(DataNode* inSrcPrimitive);

	void 						addUse(Uint8 inWhichInput, VirtualRegister& inVR, VRClass cl = vrcInteger);
	void 						addUse(Uint8 inWhichInput, DataNode& inProducer);
	void						addUse(Uint8 inWhichInput, UseDefineKind inKind);
	void 						addUse(Uint8 inWhichInput, InstructionDefine& inUseOrder);
	
	void						addDefine(Uint8 inWhichOutput, DataNode& inProducer);
	void						addDefine(Uint8 inWhichOutput, VirtualRegister& inProducer, VRClass cl = vrcInteger);
	void						addDefine(Uint8 inWhichOutput, UseDefineKind inKind);
	void 						addDefine(Uint8 inWhichOutput, InstructionDefine& inDefineOrder);
	
	virtual InstructionUse*		getInstructionUseBegin() = 0;
	virtual InstructionUse*		getInstructionUseEnd() = 0;

	virtual InstructionDefine*	getInstructionDefineBegin() = 0;
	virtual InstructionDefine*	getInstructionDefineEnd() = 0;

	void						standardUseDefine(InstructionEmitter& inEmitter);
	
	virtual PRUint32* 			getExtraRegisterInterferenceBegin() { return NULL; } // what??  laurent-try again. other registers defined by this instruction.
	virtual PRUint32* 			getExtraRegisterInterferenceEnd() { return NULL; }

	inline const Uint16			getLineNumber() const { return (mSrcPrimitive->getLineNumber()); }
	inline DataNode*			getPrimitive() const { return( mSrcPrimitive); }			
	virtual InstructionFlags	getFlags() const { return (ifNone); }

	virtual size_t				getFormattedSize(MdFormatter& /*inFormatter*/) { return (0); }		
	virtual void				formatToMemory(void * /*inStart*/, Uint32 /*inCurOffset*/, MdFormatter& /*inFormatter*/) { }

	virtual bool				switchUseToSpill(Uint8 /*inWhichUse*/, VirtualRegister& /*inVR*/) { return false; }
	virtual bool				switchDefineToSpill(Uint8 /*inWhichDefine*/, VirtualRegister& /*inVR*/) { return false; }

	void						unlinkRegisters();		// uninitialize the VirtualRegisterPtr for each use and define
	virtual bool				canSwitchToFormat(Uint32 /*mask*/) { return false; }
	virtual void				switchToFormat(Uint32 /*mask*/) {}

protected:
	void						standardDefine(InstructionEmitter& inEmitter);
	VirtualRegister*			addStandardUse(InstructionEmitter& inEmitter, Uint8 curIndex);
	
protected:	
	void						initialize();

public:
#ifdef DEBUG_LOG
	virtual void				printPretty(LogModuleObject &f) { UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("none")); }
	virtual void				printDebug(LogModuleObject &f);
#endif
#ifdef DEBUG
	int 						mDebug;
#endif

#ifdef DEBUG	// added by simon, FIX should remove
	virtual void				checkIntegrity()	{  };
	virtual void				printArgs()			{  };
#endif // DEBUG
	

protected:
	DataNode* mSrcPrimitive;		// the primitive from which this Instruction was generated
};

typedef DoublyLinkedList<Instruction> InstructionList;

 
// Comparator function for SortedDoublyLinkedList.  To get the desired effects, we want to comparison function to 
// return 1 if instruction a is of a great or equal line number then b.  (ensures that later scheduled 
// instructions of the same lineNumber end up later in the list.  Also if the two instructions are the 
// same, we want to return a 0. (Avoids duplicate scheduling)/
inline int instructionCompare(const Instruction* a, const Instruction* b)
{
	assert(a != NULL && b != NULL);
	
	if(a->getLineNumber() < b->getLineNumber())
		return -1;
	if(a == b)  
		return 0;
	return 1;
}

class Pool;

class InsnUseXDefineYFromPool :
	public Instruction
{
public:
								InsnUseXDefineYFromPool(DataNode* inSrcPrimitive, Pool& inPool, Uint8 inX, Uint8 inY);

	virtual InstructionUse*		getInstructionUseBegin() { return (mInsnUse); }
	virtual InstructionUse*		getInstructionUseEnd() { return (&mInsnUse[0] + mX); }
	virtual InstructionDefine*	getInstructionDefineBegin() { return (mDefineResource); }
	virtual InstructionDefine*	getInstructionDefineEnd() { return (&mDefineResource[0] + mY); }

protected:

	Uint8				mX;
	Uint8				mY;
	InstructionUse*		mInsnUse;
	InstructionDefine*	mDefineResource;
};

#ifdef __MWERKS__
#pragma mark -
#pragma mark ¥ÊInsnExternalUse/Define ¥
#endif

class InsnExternalUse : public InsnUseXDefineYFromPool
{
protected:
  InstructionUse externalUse;

public:
  InsnExternalUse(DataNode* inSrcPrimitive, Pool& inPool, Uint8 inUse) : 
  	InsnUseXDefineYFromPool(inSrcPrimitive, inPool, inUse, 0) {}

  virtual ~InsnExternalUse() {}

  virtual InstructionFlags 		getFlags() const { return (ifExternalInsn); }

#ifdef DEBUG_LOG
  virtual void printPretty(LogModuleObject &f) { UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("(external use)")); }
#endif
};

class InsnExternalDefine : public InsnUseXDefineYFromPool
{
protected:
  InstructionDefine externalDef;

public:
  InsnExternalDefine(DataNode* inSrcPrimitive, Pool& inPool, Uint8 inDefine) :
  	InsnUseXDefineYFromPool(inSrcPrimitive, inPool, 0, inDefine) {}

  virtual ~InsnExternalDefine() {}

  virtual InstructionFlags getFlags() const { return (ifExternalInsn); }

#ifdef DEBUG_LOG
  virtual void printPretty(LogModuleObject &f) { UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("(external def)")); }
#endif
};

#endif // INSTRUCTION_H
