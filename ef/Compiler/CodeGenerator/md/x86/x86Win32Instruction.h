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
//	x86Win32Instruction.h
//
//	Peter DeSantis
//	Simon Holmes a Court

#ifndef X86_WIN32_INSTRUCTION
#define X86_WIN32_INSTRUCTION

#include "CatchAssert.h"
#include <stdio.h>

#include "prtypes.h"
#include "Instruction.h"
#include "InstructionEmitter.h"
#include "VirtualRegister.h"
#include "Value.h"
#include "x86Opcode.h"
#include "x86ArgumentList.h"
#include "MemoryAccess.h"
#include "LogModule.h"
#include "NativeCodeCache.h"


#ifdef DEBUG
enum instructionObjType
{
	kx86Instruction,
	kSetInstruction,

	kInsnDoubleOpDir,
	kInsnDoubleOp,

	kStandardArith,
	kStandardArithDisp,
	kNotKnown
};
#endif
//================================================================================
/*

		x86Instruction(being eliminated)	InsnSet	
		InsnDoubleOp						InsnSwitch
		InsnDoubleOpDir						InsnSysCallCondBranch
			 |								InsnCondBranch
			 |								InsnNoArgs
		x86ArglistInstruction				Call
                        \				  /
				         \				 /
						  \				/
				     InsnUseXDefineYFromPool
								|
						   Instruction (can't spill)
*/
//================================================================================
// x86ArgListInstruction

class  x86ArgListInstruction :
  public InsnUseXDefineYFromPool
{
public:
	x86ArgListInstruction(DataNode* inPrimitive, Pool& inPool, Uint8 inX, Uint8 inY) :
	  InsnUseXDefineYFromPool (inPrimitive, inPool, inX, inY ) 
	  { 
		  DEBUG_ONLY(debugType = kNotKnown);
	  }

	// utility
	void			    x86StandardUseDefine(x86Win32Emitter& inEmitter);

	// virtual methods that must be written
	virtual Uint8		opcodeSize() = 0;

	// defaults to (result of opcodeSize() + iArgumentList->alSize(*this))
	virtual size_t  	getFormattedSize(MdFormatter& /*inFormatter*/); 
	virtual void		formatToMemory(void * /*inStart*/, Uint32 /*inCurOffset*/, MdFormatter& /*inFormatter*/) = 0;

	// flags
	InstructionFlags	getFlags() const { return ifNone; }

	// spilling
	virtual bool		switchUseToSpill(Uint8 inWhichUse, VirtualRegister&inVR);
	virtual bool		switchDefineToSpill(Uint8 inWhichDefine, VirtualRegister& inVR);

	// Control Spilling
	// eg switch cannot spill (at least for now)
	// eg special mem-reg type does funky things when spilling
	virtual bool		opcodeAcceptsSpill() { return true; }	// most instructions can spill
	virtual void		switchOpcodeToSpill() { }				// most opcodes don't change on spilling

	// simple arithmetic and move instructions have a direction bit
	virtual bool		canReverseOperands() { return false; }	// most instructions can't reverse their operands

	// immediates
	virtual bool		opcodeCanAccept1ByteImmediate() { return false; }
	virtual bool		opcodeCanAccept4ByteImmediate() { return false; }
	virtual bool		opcodeCanAcceptImmediate()		{ return (opcodeCanAccept1ByteImmediate() || opcodeCanAccept4ByteImmediate()); }

	// condensable
	virtual bool		regOperandCanBeCondensed()		{ return false; }

protected:
	x86ArgumentList*	iArgumentList;

public:
#ifdef DEBUG_LOG
	// Debugging Methods
	virtual void		printPretty(LogModuleObject &f);
	virtual void		printOpcode(LogModuleObject &f) = 0;
#endif // DEBUG_LOG

#ifdef DEBUG
	instructionObjType	debugType;			// used so we know what type of object we are in the debugger
	void				checkIntegrity()	{ iArgumentList->alCheckIntegrity(*this); }
	void				printArgs()			{ iArgumentList->alPrintArgs(*this); }
#endif // DEBUG
};

inline size_t x86ArgListInstruction::
getFormattedSize(MdFormatter& inFormatter)
{
	assert(iArgumentList != NULL);
	size_t size = opcodeSize() + iArgumentList->alSize(*this, inFormatter);
	return size;
}

#ifdef DEBUG_LOG

inline void x86ArgListInstruction::
printPretty(LogModuleObject &f)
{
	printOpcode(f);
	assert(iArgumentList != NULL); 
    iArgumentList->alPrintPretty(f, *this ); 
}

#endif // DEBUG_LOG

//================================================================================
// x86Instruction
// This mega-class will eventually be phased out as it is split up into smaller classes

class  x86Instruction :
  public x86ArgListInstruction
{
public:
	//--------------------------------------------------------------------------------
	// ImmediateArithType Instructions

	x86Instruction (DataNode* inPrimitive, Pool& inPool, ControlNode& inControlNode) :
		x86ArgListInstruction (inPrimitive, inPool, 0, 0 ) , flags(ifNone)
		{ 
			iOpcode = new x86Opcode_ImmArith( iaJmp );
			iArgumentList = new x86ControlNodeOffsetArgumentList( inControlNode ); 
		}
			  
	x86Instruction (DataNode* inPrimitive, Pool& inPool, x86ImmediateArithType inOpInfo, Uint32 inConstant, x86ArgumentType inArgType1, Uint8 inX, Uint8 inY) :
		x86ArgListInstruction (inPrimitive, inPool, inX, inY ) , flags(ifNone)
		{ 
			iOpcode = new x86Opcode_ImmArith( inOpInfo );	
			iArgumentList = new x86ImmediateArgumentList( inArgType1, inConstant ); 
		} 			

	x86Instruction (DataNode* inPrimitive, Pool& inPool, x86ImmediateArithType inOpInfo, Uint32 inConstant, x86ArgumentType inArgType1, Uint32 inDisplacement, Uint8 inX, Uint8 inY) :
		x86ArgListInstruction (inPrimitive, inPool, inX, inY ) , flags(ifNone)
			{ iOpcode = new x86Opcode_ImmArith( inOpInfo );	
			  iArgumentList = new x86ImmediateArgumentList( inArgType1, inDisplacement, inConstant ); } 	

	//--------------------------------------------------------------------------------
	// ExtendedType Instructions

	x86Instruction (DataNode* inPrimitive, Pool& inPool, x86ExtendedType inOpInfo, Uint32 inConstant, x86ArgumentType inArgType1, Uint8 inX, Uint8 inY) :
	x86ArgListInstruction (inPrimitive, inPool, inX, inY ) , flags(ifNone)
			{ iOpcode = new x86Opcode_Reg( inOpInfo );	
			  iArgumentList = new x86ImmediateArgumentList( inArgType1, inConstant ); } 	

	x86Instruction (DataNode* inPrimitive, Pool& inPool, x86ExtendedType inOpInfo, Uint32 inConstant, x86ArgumentType inArgType1, Uint32 inDisplacement, Uint8 inX, Uint8 inY) :
	x86ArgListInstruction (inPrimitive, inPool, inX, inY ) , flags(ifNone)
			{ iOpcode = new x86Opcode_Reg( inOpInfo );	
			  iArgumentList = new x86ImmediateArgumentList( inArgType1, inDisplacement, inConstant ); } 	

	x86Instruction (DataNode* inPrimitive, Pool& inPool, x86ExtendedType inOpInfo, x86ArgumentType inArgType1, Uint8 inX, Uint8 inY ) :
		x86ArgListInstruction (inPrimitive, inPool, inX, inY ) , flags(ifNone)
			{ iOpcode = new x86Opcode_Reg( inOpInfo );	
			  iArgumentList = new x86SingleArgumentList( inArgType1 ); } 

	x86Instruction (DataNode* inPrimitive, Pool& inPool, x86ExtendedType inOpInfo, x86ArgumentType inArgType1, Uint32 inDisplacement, Uint8 inX, Uint8 inY) :
		x86ArgListInstruction (inPrimitive, inPool, inX, inY ) , flags(ifNone)
			{ iOpcode = new x86Opcode_Reg( inOpInfo );	
			  iArgumentList = new x86SingleArgumentList( inArgType1, inDisplacement ); } 

	//--------------------------------------------------------------------------------
	// CondensableExtendedType Instructions
	x86Instruction (DataNode* inPrimitive, Pool& inPool, x86CondensableExtendedType inOpInfo, Uint32 inConstant, x86ArgumentType inArgType1, Uint8 inX, Uint8 inY) :
	x86ArgListInstruction (inPrimitive, inPool, inX, inY ) , flags(ifNone)
			{ iOpcode = new x86Opcode_Condensable_Reg( inOpInfo );	
			  iArgumentList = new x86CondensableImmediateArgumentList( inArgType1, inConstant ); } 

	x86Instruction (DataNode* inPrimitive, Pool& inPool, x86CondensableExtendedType inOpInfo, x86ArgumentType inArgType1, Uint8 inX, Uint8 inY) :
		x86ArgListInstruction (inPrimitive, inPool, inX, inY ) , flags(ifNone)
			{ iOpcode = new x86Opcode_Condensable_Reg( inOpInfo );	
			  iArgumentList = new x86SingleArgumentList( inArgType1 ); } 

	//--------------------------------------------------------------------------------
	// SpecialRegMemType Instruction
	x86Instruction (DataNode* inPrimitive, Pool& inPool, x86SpecialRegMemType inType, Uint32 inImmediate, Uint8 inX, Uint8 inY ) :
		x86ArgListInstruction (inPrimitive, inPool, inX, inY ) , flags(ifNone)
			{ iOpcode = new x86Opcode_SpecialRegMem( inType );	
			  iArgumentList = new x86SpecialRegMemArgumentList( inImmediate ); } 
								
	size_t  			getFormattedSize(MdFormatter& /*inFormatter*/); 
	virtual void		formatToMemory(void * /*inStart*/, Uint32 /*inCurOffset*/, MdFormatter& /*inFormatter*/);

	// access flags
	InstructionFlags	getFlags() const { return flags; }
	void				setFlags(InstructionFlags f)   { flags = f; }

	// Allows ArgumentList access to opcode without passing the extra reference to the opcode.
	Uint8		opcodeSize(){ return iOpcode->opSize(); }
	bool		opcodeCanAccept1ByteImmediate() { return iOpcode->opCanAccept1ByteImmediate(); }
	bool		opcodeCanAccept4ByteImmediate() { return iOpcode->opCanAccept4ByteImmediate(); }
	bool		regOperandCanBeCondensed() { return iOpcode->opRegOperandCanBeCondensed(); }

	
	virtual bool		opcodeAcceptsSpill() { return true; }
	virtual void		switchOpcodeToSpill() { iOpcode->opSwitchToRegisterIndirect(); }				

protected:
	x86Opcode*		iOpcode;
	InstructionFlags  flags;			// Used to mark copy instructions so the register allocator can remove them.

public:
#ifdef DEBUG_LOG
  void				printOpcode(LogModuleObject &f);
#endif // DEBUG_LOG
};

//--------------------------------------------------------------------------------

// FIX these two methods should be removed (third method replaces them)
inline Uint8 useToRegisterNumber(InstructionUse& inUse) 
{ 
	return colorTox86GPR[inUse.getVirtualRegister().getColor()]; 
}

inline Uint8 defineToRegisterNumber(InstructionDefine& inDefine) 
{
	Uint8 color = inDefine.getVirtualRegister().getColor();
	return colorTox86GPR[color]; 
} 

inline Uint8 getRegisterNumber(VirtualRegister* vreg) 
{
	Uint8 color = vreg->getColor();
	assert(color < 6);
	return colorTox86GPR[color]; 
}

//--------------------------------------------------------------------------------
inline size_t x86Instruction::
getFormattedSize(MdFormatter& inFormatter) 
{
	assert(iOpcode != NULL && iArgumentList != NULL);
	return (iOpcode->opSize() + iArgumentList->alSize(*this, inFormatter));
}

#ifdef DEBUG_LOG
inline void x86Instruction::
printOpcode(LogModuleObject &f)
{
	iOpcode->opPrintPretty(f);
}
#endif DEBUG_LOG

//================================================================================
// InsnNoArgs
class InsnNoArgs :
	public InsnUseXDefineYFromPool
{
public:
	InsnNoArgs(DataNode* inPrimitive, Pool& inPool, x86NoArgsCode inCode, Uint8 inUses, Uint8 inDefines) :
		InsnUseXDefineYFromPool(inPrimitive, inPool, inUses, inDefines),
		code(inCode)
		{}

	virtual void			formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/);
	virtual size_t			getFormattedSize(MdFormatter& /*inFormatter*/) { return 1; }
	InstructionFlags		getFlags() const { return ifNone; }
	virtual bool			opcodeAcceptsSpill() { return false; }
	virtual void			switchOpcodeToSpill() { assert(false); }

protected:
	x86NoArgsCode			code;
#ifdef DEBUG_LOG
public:
	virtual void			printPretty(LogModuleObject &f);
#endif
};
	
//================================================================================
// InsnSwitch
class InsnSwitch :
	public InsnUseXDefineYFromPool
{
public:
								InsnSwitch(DataNode* inPrimitive, Pool& inPool);

	virtual void				formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/);
	virtual size_t				getFormattedSize(MdFormatter& /*inFormatter*/);

	InstructionFlags			getFlags() const { return ifNone; }

	// can't spill
	virtual bool				opcodeAcceptsSpill() { return false; }
	virtual void				switchOpcodeToSpill() { assert(false); }

protected:
	Uint32			mNumCases;
	ControlNode*	mControlNode;
	Uint8*			mTableAddress;

#ifdef DEBUG_LOG
public:
	virtual void				printPretty(LogModuleObject &f);
#endif
};

//================================================================================
// InsnCondBranch
// FIX For now all branches take 4 bytes immediates -- eventually we want this to take
// the minimum possible
class InsnCondBranch :
	public InsnUseXDefineYFromPool
{
public:
			InsnCondBranch(DataNode* inPrimitive, Pool& inPool, x86ConditionCode condType, ControlNode& inControlNode) :
				InsnUseXDefineYFromPool(inPrimitive, inPool, 1, 0),
				condType(condType),
				cnoSource(inControlNode)
					{};

	virtual void		formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/);
	virtual size_t		getFormattedSize(MdFormatter& /*inFormatter*/) { return 6; }
	InstructionFlags	getFlags() const { return ifNone; }
	virtual bool		opcodeAcceptsSpill() { return false; }	// spilling makes no sense here
	virtual void		switchOpcodeToSpill() { assert(false); }

protected:
	x86ConditionCode	condType;	// x86 condition codes
	ControlNode&		cnoSource;	// the source of the branch

#ifdef DEBUG_LOG
public:
	virtual void		printPretty(LogModuleObject &f);
#endif
};

//================================================================================
// InsnSysCallCondBranch
// emit
//	jcc OK
//	call inFunc
//	OK:
//
class InsnSysCallCondBranch :
	public InsnUseXDefineYFromPool
{
public:
			InsnSysCallCondBranch(DataNode* inPrimitive, Pool& inPool, x86ConditionCode inCondType, void (*inFunc)()) :
				InsnUseXDefineYFromPool(inPrimitive, inPool, 1, 0),
				functionAddress(inFunc),
				condType(inCondType)
					{};

	virtual void		formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/)
	{
		Uint8* start = (Uint8*) inStart;

		// emit jump
		*start++ = 0x0f;
		*start++ = 0x80 + condType;
		writeLittleWordUnaligned((void*)start, 5);
		start += 4;

		// emit call
		Uint8* callStart = start;
		*start++ = 0xe8;
		int32 branchOffset = (Uint32)functionAddress - (Uint32) callStart - 5;
		writeLittleWordUnaligned((void*)start, branchOffset);
	}

	virtual size_t		getFormattedSize(MdFormatter& /*inFormatter*/) { return 6 + 5; }

	InstructionFlags	getFlags() const { return ifNone; }
	virtual bool		opcodeAcceptsSpill() { return false; }		// spilling makes no sense here
	virtual void		switchOpcodeToSpill() { assert(false); }

protected:
	void*				functionAddress;
	x86ConditionCode	condType;

#ifdef DEBUG_LOG
public:
	virtual void		printPretty(LogModuleObject &f);
#endif
};

//================================================================================
// Set on condition flags
// cannot spill

// uses a condition 
class InsnSet :
	public InsnUseXDefineYFromPool
{
public:
						InsnSet(DataNode* inPrimitive, Pool& inPool, x86ConditionCode condType, int inX = 2, int inY = 1) : 
							InsnUseXDefineYFromPool(inPrimitive, inPool, inX, inY ),
							condType(condType)
						{}

	virtual	void		formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /* inFormatter */  );
	virtual size_t		getFormattedSize(MdFormatter& /*inFormatter*/) { return 3; }	// 0xff 0x90 reg
	virtual Uint8		opcodeSize() { return 2; }

protected:
	x86ConditionCode	condType;	// x86 condition codes

#ifdef DEBUG_LOG
public:
	virtual void		printPretty(LogModuleObject &f);
#endif
};

//================================================================================
/* InsnDoubleOp
	0110 1001:11 reg1 reg2: immdata			register1 with immediate to register2
	0110 1001:mod reg r/m: immdata			register with immediate to register

	0000 1111:1010 1111: 11 reg1 reg2		register1 with register2
	0000 1111:1010 1111: mod reg r/m		register with memory
*/
class InsnDoubleOp :
	public x86ArgListInstruction
{
public:
		InsnDoubleOp(	DataNode* inPrimitive, Pool& inPool, x86DoubleOpCode inCodeType, 
						x86ArgumentType inArgType1 = atRegDirect, x86ArgumentType inArgType2 = atRegDirect, 
						Uint8 uses = 2, Uint8 defines = 1	);

		InsnDoubleOp(	DataNode* inPrimitive, Pool& inPool, x86DoubleOpCode inCodeType, 
						Uint32 inDisplacement,
						x86ArgumentType inArgType1 = atRegDirect, x86ArgumentType inArgType2 = atRegDirect, 
						Uint8 uses = 2, Uint8 defines = 1	);

	virtual bool		canReverseOperands() { return false; }
	virtual	void		formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& inFormatter);
	virtual Uint8		opcodeSize();

protected:
	x86DoubleOpCode		codeType;

public:
#ifdef DEBUG_LOG
	virtual void		printOpcode(LogModuleObject &f);
#endif
};

//================================================================================
// InsnDoubleOpDir

// use the default spilling behaviour for x86ArgListInstruction
class InsnDoubleOpDir :
	public x86ArgListInstruction
{
public:
		InsnDoubleOpDir(DataNode* inPrimitive, Pool& inPool, x86DoubleOpDirCode inCodeType, 
						x86ArgumentType inArgType1 = atRegDirect, x86ArgumentType inArgType2 = atRegDirect, 
						Uint8 uses = 2, Uint8 defines = 1	);

		InsnDoubleOpDir(DataNode* inPrimitive, Pool& inPool, x86DoubleOpDirCode inCodeType, 
						Uint32 inDisplacement,
						x86ArgumentType inArgType1 = atRegDirect, x86ArgumentType inArgType2 = atRegDirect, 
						Uint8 uses = 2, Uint8 defines = 1	);

	// the main feature of these instructions is their ability to reverse operands
	virtual bool		canReverseOperands() { return true; }
	virtual	void		formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& inFormatter);
	virtual Uint8		opcodeSize();
	InstructionFlags	getFlags() const;		// ifCopy if we are an unspilt move, ifNone otherwise

protected:
	bool				getDirectionBit();
	x86DoubleOpDirCode	codeType;

public:
#ifdef DEBUG_LOG
	virtual void		printOpcode(LogModuleObject &f);
#endif
};

//================================================================================
// Utililty
// Returns a new copy instruction
inline InsnDoubleOpDir& 
newCopyInstruction(DataNode& inDataNode, Pool& inPool, Uint8 uses = 1, Uint8 defines = 1)
{
	return *new(inPool) InsnDoubleOpDir(&inDataNode, inPool, raCopyI, atRegDirect, atRegDirect, uses, defines);
}

//================================================================================
// Calls

template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic>
class Call :
	public InsnUseXDefineYFromPool
{
public:
	static inline bool 			hasReturnValue(DataNode& inDataNode);
	static inline Uint8			numberOfArguments(DataNode& inDataNode);


			Call(	DataNode* inDataNode,
					Pool& inPool, 
					Uint8 inRegisterArguments, 
					bool inHasReturnValue, 
					x86Win32Emitter& inEmitter, 
					void (*inFunc)() = NULL, 
					DataNode* inUseDataNode = NULL	);

public:
	virtual void				formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/);
	virtual size_t				getFormattedSize(MdFormatter& /*inFormatter*/) { return (5); }
	virtual InstructionFlags	getFlags() const { return (ifCall); }

protected:
	Uint32			mCalleeAddress;

#ifdef DEBUG_LOG
public:
	virtual void				printPretty(LogModuleObject &f) 
		{ 
			CacheEntry* ce = NativeCodeCache::getCache().lookupByRange((Uint8*)mCalleeAddress);
			if(ce)
			{
				Method* method = ce->descriptor.method;
				assert(method);
				const char* name = method->getName();
				const char* tag = method->getHTMLName();
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("   call <A HREF=\"%s\">%s</A>", tag, name));
			}
			else
				UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("   call %p", (Uint32 *)mCalleeAddress));
		}
#endif
};

template<bool tHasIncomingStore, bool tHasOutgoingStore>
class CallS :
	public Call<tHasIncomingStore, tHasOutgoingStore, false, false>
{
public:

	inline 	CallS(	DataNode* inDataNode, 
					Pool& inPool, 
					Uint8 inRegisterArguments, 
					bool inHasReturnValue, 
					x86Win32Emitter& inEmitter, 
					void (*inFunc)(), 
					DataNode* inUseDataNode = NULL	) :
				Call<tHasIncomingStore, tHasOutgoingStore, false, false>(inDataNode, inPool, inRegisterArguments, inHasReturnValue, inEmitter, inFunc, inUseDataNode) { }
};

typedef CallS<true, true> CallS_V;
typedef CallS<true, false> CallS_;
typedef CallS<false, false>	CallS_C;
typedef Call<true, true, true, false> Call_;

// Dynamically dispatched call
class CallD_ :
	public Call<true, true, true, true>
{
public:
	inline			CallD_(DataNode*	inDataNode, Pool& inPool, Uint8 inRegisterArguments, bool inHasReturnValue, x86Win32Emitter& inEmitter) :
						Call<true, true, true, true>(inDataNode, inPool, inRegisterArguments, inHasReturnValue, inEmitter) { }		
 
	inline virtual void	formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& inEmitter);
	virtual size_t	getFormattedSize(MdFormatter& /*inFormatter*/) { return (2); }

#ifdef DEBUG_LOG
	virtual void				printPretty(LogModuleObject &f) { UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("   call ???")); }
#endif
};

template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic> bool
Call<tHasIncomingStore, tHasOutgoingStore, tHasFunctionAddress, tIsDynamic>::
hasReturnValue(DataNode& inDataNode)
{
	bool hasReturnValue = (inDataNode.getOutgoingEdgesBegin() + tHasOutgoingStore < inDataNode.getOutgoingEdgesEnd());
		
	return (hasReturnValue);
}

template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic> Uint8
Call<tHasIncomingStore, tHasOutgoingStore,tHasFunctionAddress, tIsDynamic>::
numberOfArguments(DataNode& inDataNode)
{
	DataConsumer*	firstArg;
	DataConsumer*	lastArg;
	
	assert(!(tHasFunctionAddress && !tHasIncomingStore));	// no such primitive
	firstArg = inDataNode.getInputsBegin() + tHasFunctionAddress + tHasIncomingStore;
	lastArg = inDataNode.getInputsEnd();
	
	return (lastArg - firstArg);
}

template<bool tHasIncomingStore, bool tHasOutgoingStore, bool tHasFunctionAddress, bool tIsDynamic>
void Call<tHasIncomingStore, tHasOutgoingStore, tHasFunctionAddress, tIsDynamic>::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/)
{
	Uint8* start = (Uint8*)inStart;
	int32 branchOffset = mCalleeAddress - (Uint32) inStart - 5;
	*start++ = 0xe8;
	writeLittleWordUnaligned((void*)start, branchOffset);
}


inline void CallD_::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/)
{
	Uint8	*curPC = (Uint8 *) inStart;
	
	*curPC++ = 0xff;
	*curPC++ = 0xd0 | useToRegisterNumber(getInstructionUseBegin()[0]);
}

#endif
