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
//	x86Win32Instruction.cpp
//
//	Simon Holmes a Court
//	Peter DeSantis

#include "prtypes.h"
#include "x86Win32Instruction.h"
#include "InstructionEmitter.h"
#include "x86Formatter.h"

class x86Win32Emitter;
extern char* x86GPRText[];

UT_DEFINE_LOG_MODULE(x86Spill);


//================================================================================
// Debugging Structures
#ifdef DEBUG_LOG

char* conditionalSuffixes[] =
{
	"o",	// ccJO
	"no",	// ccJNO
	"b",	// ccJB
	"nb",	// ccJNB
	"e",	// ccJE
	"ne",	// ccJNE
	"be",	// ccJBE
	"nbe",	// ccJNBE
	"s",	// ccJS
	"ns",	// ccJNS
	"p",	// ccJP
	"np",	// ccJNP
	"l",	// ccJL
	"nl",	// ccJNL
	"le",	// ccJLE
	"nle",	// ccJNLE
};

#endif // DEBUG_LOG

//================================================================================
// x86ArgListInstruction Methods

// For now this is a member of x86ArgListInstruction, but it really should be available for all
// instructions on the x86 platform

// Method:		x86StandardUseDefine
// Caller:		Emitter
// Purpose:		Due to x86 behaviour of modifying source register, we must make a copy of the source before 
//				the operation. Unnecessary copies can be removed by the register allocator.
void x86ArgListInstruction::
x86StandardUseDefine(x86Win32Emitter& inEmitter)
{
	InstructionUse* 	instructionUseBegin = getInstructionUseBegin();
	InstructionDefine* 	instructionDefineBegin = getInstructionDefineBegin();
	Uint8				curIndex;					

	// Copy the virtual register which will be overwritten
	VirtualRegister&    vrToBeOverwritten = inEmitter.emit_CopyOfInput(*this, *mSrcPrimitive, 0);
	
	inEmitter.useProducer(*mSrcPrimitive, *this, 0);

	// Set the rest of the uses.
	InstructionUse*		curUse;
	for (curUse = instructionUseBegin + 1, curIndex = 1; curUse < getInstructionUseEnd(); curUse++, curIndex++)
		addStandardUse(inEmitter, curIndex);
	
	// Now redefine the copied register.
	inEmitter.redefineTemporary(*this, vrToBeOverwritten, 0);	

	assert(instructionDefineBegin + 1 >= getInstructionDefineEnd());
	//// Set the rest of the defines.
	//InstructionDefine*	curDefine;
	//for (curDefine = instructionDefineBegin + 1, curIndex = 1; curDefine < getInstructionDefineEnd(); curDefine++, curIndex++)
	//	inEmitter.defineProducer(nthOutputProducer(*mSrcPrimitive, curIndex), *this, curIndex);
}

// Method:		switchUseToSpill
// Caller:		Register Allocator
// Purpose:		Folds the spill into the instruction if possible.
// Returns:		Returns true if possible, false otherwise
bool x86ArgListInstruction::
switchUseToSpill(Uint8 inWhichUse, VirtualRegister& inVR)
{
#ifdef DEBUG_LOG
	UT_LOG(x86Spill, PR_LOG_DEBUG, (" spill use %d of (%p)'", inWhichUse, this));
	printOpcode(UT_LOG_MODULE(x86Spill));
#endif
	DEBUG_ONLY(checkIntegrity();)

	if (!opcodeAcceptsSpill())
		goto SpillFail;

	// Ask the argument list if it can switch the current argument
	if (iArgumentList->alSwitchArgumentTypeToSpill(inWhichUse, *this))	// has side effect!
	{	
		// Tell the Opcode that the argumentlist has been switched to a spill
		switchOpcodeToSpill();

		// Replace the old virtual register with a new one which contains the colored stack slot.
		InstructionUse& use = getInstructionUseBegin()[inWhichUse];
		InstructionDefine& define = getInstructionDefineBegin()[0];
		assert(use.isVirtualRegister());

		if(inWhichUse == 0 && define.isVirtualRegister() && use.getVirtualRegister().getRegisterIndex() == define.getVirtualRegister().getRegisterIndex())
			// this virtual register is also redefined by this instruction so we must reinitialize the define also
			define.getVirtualRegisterPtr().initialize(inVR);
		use.getVirtualRegisterPtr().initialize(inVR);
		DEBUG_ONLY(checkIntegrity();)

		goto SpillSuccess;
	}

	// by default we fail
SpillFail:
	UT_LOG(x86Spill, PR_LOG_DEBUG, ("': false\n"));
	DEBUG_ONLY(checkIntegrity();)
	return false;

SpillSuccess:
	UT_LOG(x86Spill, PR_LOG_DEBUG, ("': true\n"));
	DEBUG_ONLY(checkIntegrity();)
	return true;
}

// Method:		switchDefineToSpill
// Caller:		Register Allocator
// Purpose:		Folds the spill into the instruction if possible.
// Returns:		Returns true if possible, false otherwise
bool x86ArgListInstruction::
switchDefineToSpill(Uint8 inWhichDefine, VirtualRegister& inVR)
{
#ifdef DEBUG_LOG
	UT_LOG(x86Spill, PR_LOG_DEBUG, (" spill def %d of (%p)'", inWhichDefine, this));
	printOpcode(UT_LOG_MODULE(x86Spill));
#endif
	DEBUG_ONLY(checkIntegrity();)

	assert(inWhichDefine == 0);			// can only switch the first define

	InstructionUse& use = getInstructionUseBegin()[0];
	InstructionUse* useEnd = getInstructionUseEnd();
	InstructionDefine& define = getInstructionDefineBegin()[inWhichDefine];

	assert(define.isVirtualRegister());	// cannot call this routine on anything other than a virtual register

	// some instructions cannot spill
	if (!opcodeAcceptsSpill())
		goto SpillFail;
	
	// If this register is being redefined then it should correspond to the first use
	// Make sure that there is a first use
	if(&use < useEnd)
	{	
		// If it is indeed the same virtual register
		if(use.isVirtualRegister() && use.getVirtualRegister().getRegisterIndex() == define.getVirtualRegister().getRegisterIndex()) 
		{	
			// define == first use, try to switch the first argument to spill type
			if(opcodeAcceptsSpill() && iArgumentList->alSwitchArgumentTypeToSpill(0, *this)) 
			{		
				switchOpcodeToSpill();	// Tell the Opcode that the argumentlist has been switched to a spill
				// Replace the old virtual register with a new one which contains the colored stack slot
				// The define is also the same as the use so we need to reinitialize both the use and the define VR.
				use.getVirtualRegisterPtr().initialize(inVR);
				define.getVirtualRegisterPtr().initialize(inVR);
				goto SpillSuccess;
			}
		}
		else 
		{
			// There are no other VRs in the uses, define is the second argument
			if(opcodeAcceptsSpill() && iArgumentList->alSwitchArgumentTypeToSpill(1, *this)) 
			{
				switchOpcodeToSpill();	// Tell the Opcode that the argumentlist has been switched to a spill
				// Replace the old virtual register with a new one which contains the colored stack slot
				define.getVirtualRegisterPtr().initialize(inVR);
				goto SpillSuccess;
			}
		}
	}
	else 
	{
		// There are no VRs in the uses so we need to try to switch the first argument
		if(opcodeAcceptsSpill() && iArgumentList->alSwitchArgumentTypeToSpill(0, *this)) 
		{
			switchOpcodeToSpill();		// Tell the Opcode that the argumentlist has been switched to a spill
			// Replace the old virtual register with a new one which contains the colored stack slot
			define.getVirtualRegisterPtr().initialize(inVR);
			goto SpillSuccess;
		}
	}

	// by default we fail
SpillFail:
	UT_LOG(x86Spill, PR_LOG_DEBUG, ("': false\n"));
	DEBUG_ONLY(checkIntegrity();)
	return false;

SpillSuccess:
	UT_LOG(x86Spill, PR_LOG_DEBUG, ("': true\n"));
	DEBUG_ONLY(checkIntegrity();)
	return true;
}

//================================================================================
// x86Instruction

void x86Instruction::
formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& inFormatter) 
{ 
	assert(iOpcode != NULL && iArgumentList != NULL); 
	// Format the opcode to memory
	iOpcode->opFormatToMemory(inStart, *iArgumentList, *this);
	// Find the location of the argumnet list and format it to memory
	Uint8* argLocation = (Uint8*)inStart + iOpcode->opSize();
	iArgumentList->alFormatToMemory((void*)argLocation , inOffset, *this, inFormatter);
	// If the opcode has an opcode extension then or it into the proper place.  ( the reg field of the modr/m byte.)
	if(iOpcode->opHasRegFieldExtension( *iArgumentList, *this ))
	{
		Uint8 temp = iOpcode->opGetRegFieldExtension();
		*argLocation = *argLocation | temp;
	}		
}

//================================================================================
// InsnSwitch

InsnSwitch::
InsnSwitch(DataNode* inPrimitive, Pool& inPool) :
	InsnUseXDefineYFromPool(inPrimitive, inPool, 1, 0 )
{
	mControlNode = inPrimitive->getContainer();
	assert(mControlNode);
	mNumCases = mControlNode->nSuccessors();
}

void InsnSwitch::
formatToMemory(void* inStartAddress, Uint32 /*inOffset*/, MdFormatter& inFormatter)
{
	Uint8* start = (Uint8*)inStartAddress;

	// calculate position of jump table
	mTableAddress = start + 7;

	// get the register
	Uint8 reg = useToRegisterNumber(*getInstructionUseBegin());
	assert (reg != ESP);			// ESP is an invalid index 

	// calculate the SIB
	Uint8 SIB = 0x80 | ( reg << 3) | 0x05;

	// write out instruction
	*start++ = 0xff;				// opcode for jump
	*start++ = 0x24;				// mod/ext/rm for jmp disp32[reg * 4]
	*start++ = SIB;

	// write address of jump table
	writeLittleWordUnaligned(start, (int)mTableAddress);

	// write out table
	Uint8* methodBegin = inFormatter.getMethodBegin();
	ControlEdge* edgesEnd = mControlNode->getSuccessorsEnd();
	for(ControlEdge* edge = mControlNode->getSwitchSuccessors(); edge != edgesEnd; edge++)
	{
		Uint8* destAddress = methodBegin + edge->getTarget().getNativeOffset();
		start += 4;
		writeLittleWordUnaligned(start, (Uint32)destAddress);
	}
}

size_t InsnSwitch::
getFormattedSize(MdFormatter& /*inFormatter*/)
{
	// reserve 7 bytes for the indexed jump, plus 4 bytes per entry
	return 7 + (4 * mNumCases);
}

#ifdef DEBUG_LOG
void InsnSwitch::
printPretty(LogModuleObject &f)
{
	Uint8 reg = useToRegisterNumber(*getInstructionUseBegin());
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("jmp      0x%x[4 * %s]", Uint32(mTableAddress), x86GPRText[reg]));

	// print table
	ControlEdge* edgesEnd = mControlNode->getSuccessorsEnd();
	for(ControlEdge* edge = mControlNode->getSwitchSuccessors(); edge != edgesEnd; edge++)
	{
		Uint32 destAddress = edge->getTarget().getNativeOffset();
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\n   Method Start + 0x%x [<A HREF=\"#N%d\">N%d</A>]", 
			destAddress, edge->getTarget().dfsNum, edge->getTarget().dfsNum));
	}
}
#endif

//================================================================================
// InsnCondBranch

void InsnCondBranch::
formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& /*inFormatter*/)
{
	uint8* start = (uint8*) inStart;

	*start++ = 0x0f;
	*start++ = 0x80 | condType;

	// find destination
	ControlNode& cnoTarget = cnoSource.getTrueSuccessor().getTarget();

	// To compute the relative branch we subtract our current address from out target address.  Then we subtract the size
	// of our instruction because our current IP is actually there.  This is the sum of the size of the opcode and the size of the
	// argumentlist.  NOTE: If we use 1 byte jumps in the future then this needs to be fixed from a constant 4.
	const int opcodeSize = 2;
	Int32 jumpOffset = cnoTarget.getNativeOffset() - inOffset - opcodeSize - 4;
	writeLittleWordUnaligned(start, jumpOffset);
}

#ifdef DEBUG_LOG

void InsnCondBranch::
printPretty(LogModuleObject &f)
{
	ControlNode& cnoTarget = cnoSource.getTrueSuccessor().getTarget();
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("j%-7s start+%0d [<A HREF=\"#N%d\">N%d</A>]", 
		conditionalSuffixes[condType], cnoTarget.getNativeOffset(), cnoTarget.dfsNum, cnoTarget.dfsNum));
}

#endif

//================================================================================
// Set

// Can only be EAX, ECX, EDX or EBX
void InsnSet::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/ )
{
	uint8* start = (uint8*) inStart;

	// format the opcode to memory
	*start++ = 0x0f;
	*start++ = 0x90 | condType;

	// find the register
    InstructionDefine* defineBegin = getInstructionDefineBegin();

#ifdef DEBUG
    InstructionUse* useBegin = getInstructionUseBegin();
	InstructionUse*	useEnd = getInstructionUseEnd();
	assert(useBegin < useEnd);      // condition code always used

    InstructionDefine* defineEnd = getInstructionDefineEnd();
    assert((defineBegin < defineEnd) && defineBegin->isVirtualRegister());
#endif

	Uint8 reg = defineToRegisterNumber(*defineBegin);
	assert(/* (reg >= 0) && */ (reg <= 3));	// these are the only legal registers

	// format the register
	Uint8 modRM = 0xc0 | reg;
	*start = modRM;
}

#ifdef DEBUG_LOG

void InsnSet::
printPretty(LogModuleObject &f)
{
    InstructionDefine* defineBegin = getInstructionDefineBegin();
	Uint8 reg = defineToRegisterNumber(*defineBegin);
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("set%-5s %s", conditionalSuffixes[condType], x86GPRText[reg]));
}

#endif

//================================================================================
// InsnCondSysCallBranch
#ifdef DEBUG_LOG
void InsnSysCallCondBranch::
printPretty(LogModuleObject &f)
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("j%-7s (over next)\n   call %p", conditionalSuffixes[condType], functionAddress));
}
#endif

//================================================================================
// InsnCondSysCallBranch
struct x86NoArgsInfo
{
	uint8	opcode;				// generally the opcode, but can have additional info
	// eventually will be in DEBUG_LOG build only
	char*	text;				// string for fake disassembly
};

x86NoArgsInfo noArgsInfo[] =
{
	{ 0x99, "cdq"  },			//opCdq			99
	{ 0xcc, "int 3"  },			//opBreak		cc
    { 0x9e, "sahf"  },          //opSahf        9e
};

void InsnNoArgs::
formatToMemory(void* inStart, Uint32 /*inOffset*/, MdFormatter& /*inFormatter*/)
{ 
	*((Uint8*)inStart) = (Uint8) noArgsInfo[code].opcode; 
}

#ifdef DEBUG_LOG
void InsnNoArgs::
printPretty(LogModuleObject &f) 
{ 
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%-8s", noArgsInfo[code].text)); 
}
#endif

//================================================================================
// InsnDoubleOp Methods
struct x86DoubleOpInfo
{
	uint8	opcode;				// generally the opcode, but can have additional info
	bool	hasPrefix;			// does the opcode needs to be prefixed with 0x0f

	// eventually will be in DEBUG_LOG build only
	char*	text;				// string for fake disassembly
};

x86DoubleOpInfo doubleOpInfo[] =
{
	{ 0xaf, true,	"imul " },			//opIMul		0f af
	{ 0xbe, true,	"movsxB "},			//opMovSxB		0f be
	{ 0xbf, true,	"movsxH "},			//opMovSxH		0f bf
	{ 0xb6, true,	"movzxB "},			//opMovZxB		0f b6
	{ 0xb7, true,	"movzxH "}			//opMovZxH		0f b7
};

InsnDoubleOp::
InsnDoubleOp(	DataNode* inPrimitive, Pool& inPool, x86DoubleOpCode inCodeType, 
				x86ArgumentType inArgType1, x86ArgumentType inArgType2, 
				Uint8 uses, Uint8 defines ) :
	x86ArgListInstruction (inPrimitive, inPool, uses, defines ),
	codeType(inCodeType)
{
	iArgumentList = new x86DoubleArgumentList(inArgType1, inArgType2);
	DEBUG_ONLY(debugType = kInsnDoubleOp);
}

InsnDoubleOp::
InsnDoubleOp(	DataNode* inPrimitive, Pool& inPool, x86DoubleOpCode inCodeType, 
				Uint32 inDisplacement, 
				x86ArgumentType inArgType1, x86ArgumentType inArgType2, 
				Uint8 uses, Uint8 defines) :
	x86ArgListInstruction (inPrimitive, inPool, uses, defines ),
	codeType(inCodeType)
{
	iArgumentList = new x86DoubleArgumentList(inArgType1, inArgType2, inDisplacement);
	DEBUG_ONLY(debugType = kInsnDoubleOp);
}

void InsnDoubleOp::
formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& inFormatter)
{
	uint8* start = (uint8*) inStart;
	assert(iArgumentList);
	
	// Format the opcode to memory
	if(doubleOpInfo[codeType].hasPrefix)
	  *start++ = kPrefix_For_2_Byte;
	*start = doubleOpInfo[codeType].opcode;

	// Find the location of the argumnet list and format it to memory
	Uint8* argLocation = (Uint8*)inStart + opcodeSize();
	iArgumentList->alFormatToMemory((void*)argLocation , inOffset, *this, inFormatter);
}

Uint8 InsnDoubleOp::
opcodeSize()
{
	return (doubleOpInfo[codeType].hasPrefix) ? 2 : 1;
}

#ifdef DEBUG_LOG
void InsnDoubleOp::
printOpcode(LogModuleObject &f)
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%-8s ", doubleOpInfo[codeType].text));
}
#endif

//================================================================================
// InsnDoubleOpDir Methods

// used for debugging only (for now)
enum raType
{
	kArith,
	kLoad,
	kStore,
	kCopy
};

struct x86RAOpcodeInfo
{
	uint8	opcode;				// generally the opcode, but can have additional info
	bool	needs16BitPrefix;	// does the opcode need the 'force to 16 bit' opcode?
	raType	type;				// is it  a move

	// eventually will be in DEBUG_LOG build only
	char*	text;				// string for fake disassembly
};

// note: store  dest, source
// note: copy
x86RAOpcodeInfo raInfo[] =
{
	{ 0x01, false, kArith,	"add " },		//raAdd
	{ 0x11, false, kArith,	"adc " },		//raAdc
	{ 0x29, false, kArith,	"sub " },		//raSub
	{ 0x19, false, kArith,	"sbb " },		//raSbb
	{ 0x21, false, kArith,	"and " },		//raAnd
	{ 0x09, false, kArith,	"or  " },		//raOr
	{ 0x31, false, kArith,	"xor " },		//raXor
	{ 0x39, false, kArith,	"cmp " },		//raCmp

	{ 0x89, false, kLoad,	"mov(ld) " },	//raLoadI
	{ 0x89, false, kCopy,	"mov(cp) " },	//raCopyI
	{ 0x89, false, kStore,	"mov(st) " },	//raStoreI
	{ 0x89, false, kStore,	"mov(sv) " },	//raSaveReg

	{ 0x88, false, kStore,	"movB "},		//raStoreB
	{ 0x89, true,  kStore,	"movH "},		//raStoreH
};

InsnDoubleOpDir::
InsnDoubleOpDir(DataNode* inPrimitive, Pool& inPool, x86DoubleOpDirCode inCodeType, 
				x86ArgumentType inArgType1, x86ArgumentType inArgType2, 
				Uint8 uses, Uint8 defines ) :
	x86ArgListInstruction (inPrimitive, inPool, uses, defines ),
	codeType(inCodeType)
{
	iArgumentList = new x86DoubleArgumentList(inArgType1, inArgType2);
	DEBUG_ONLY(debugType = kInsnDoubleOpDir);

	// temp for asserts
	getDirectionBit();
}

InsnDoubleOpDir::
InsnDoubleOpDir(DataNode* inPrimitive, Pool& inPool, x86DoubleOpDirCode inCodeType, 
				Uint32 inDisplacement, x86ArgumentType inArgType1, x86ArgumentType inArgType2, 
				Uint8 uses, Uint8 defines) :
	x86ArgListInstruction (inPrimitive, inPool, uses, defines ),
	codeType(inCodeType)
{
	iArgumentList = new x86DoubleArgumentList(inArgType1, inArgType2, inDisplacement);
	DEBUG_ONLY(debugType = kInsnDoubleOpDir);

	// temp for asserts
	getDirectionBit();
}

// copy flag should only be set if insn is a copy and the use and define are registerDirect -- no other flags can be set
InstructionFlags InsnDoubleOpDir::
getFlags() const
{
	return (codeType == raCopyI && iArgumentList->alIsRegisterDirect()) ? ifCopy : ifNone;
}

bool InsnDoubleOpDir::
getDirectionBit()
{
	x86DoubleArgumentList* arglist = (x86DoubleArgumentList*)iArgumentList;
	bool arg1isDirect = arglist->akIsArg1Direct();
	bool arg2isDirect = arglist->akIsArg2Direct();
	assert(arg1isDirect || arg2isDirect);	// at least one has to be register direct

	bool dirBit;

	switch(codeType)
	{
	case raSaveReg:
		// Save Instructions
		// saves use (first arg) to stack slot (define)
		// mov r1 -> M		=>	mov r1 -> M		dir = false
		assert(arg1isDirect);
		assert(!arg2isDirect);
		dirBit = false;
		break;
	
	case raCopyI:
		// Copy Instructions
		/* OLD
		// copies use (first arg) to define (second arg)
		// mov r1 -> r2		=>	mov r1 -> r2	dir = false
		// mov r1 -> M		=>	mov r1 -> M		dir = false
		// mov M -> r2		=>	mov r2 <- M		dir = true
		// ie direction bit is clear iff argument 1 is registerDirect
		dirBit = !arg1isDirect;
		break;
		*/

		// copies use (first arg) to define (second arg)
		// mov r1 -> r2		=>	mov r2 <- r1	dir = true
		// mov r1 -> M		=>	mov r1 -> M		dir = false
		// mov M -> r2		=>	mov r2 <- M		dir = true
		// ie direction bit is clear iff argument 1 is registerDirect
		dirBit = arg2isDirect;
		break;

	case raStoreB:
	case raStoreH:
	case raStoreI:
		// Store Instruction
		// stores second use into first use memory
		// mov M <- r2		=>	mov r2 -> M		dir = false
		dirBit = false;
		assert(!arg1isDirect);
		assert(arg2isDirect);
		break;

	case raLoadI:
		// Load Instruction
		// loads from memory address (use, first arg) into register (define, second arg)
		// mov M -> r1		=>	mov r1 <- M		dir = true
		dirBit = true;
		assert(!arg1isDirect);
		assert(arg2isDirect);
		break;

	default:
		// Arithmetic Instructions
		// add r1, r2	=>	add r1, r2	dir = true
		// add r1, M	=>	add r1, M	dir = true
		// add M, r2	=>	add r2, M	dir = false
		// ie direction bit is set iff argument 1 is registerDirect
		assert(raInfo[codeType].type == kArith);
		dirBit = arg1isDirect;
	}

	return dirBit;
}

void InsnDoubleOpDir::
formatToMemory(void* inStart, Uint32 inOffset, MdFormatter& inFormatter)
{
	uint8* start = (uint8*) inStart;
	assert(iArgumentList);
	
	// Format the opcode to memory
	if(raInfo[codeType].needs16BitPrefix)
	  *start++ = kPrefix_For_16_Bits;

	// calculate opcode
	Uint8 direction = getDirectionBit() ? 2 : 0;
	Uint8 opcode = raInfo[codeType].opcode | direction;
	*start = opcode;

	// Find the location of the argumnet list and format it to memory
	Uint8* argLocation = (Uint8*)inStart + opcodeSize();
	iArgumentList->alFormatToMemory((void*)argLocation , inOffset, *this, inFormatter);
}

Uint8 InsnDoubleOpDir::
opcodeSize()
{
	return (raInfo[codeType].needs16BitPrefix) ? 2 : 1;
}

#ifdef DEBUG_LOG
void InsnDoubleOpDir::
printOpcode(LogModuleObject &f)
{
	UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%-8s ", raInfo[codeType].text));
}
#endif

//================================================================================
