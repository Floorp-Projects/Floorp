/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */


#include "avmplus.h"

//hack
#if !defined(AVMPLUS_SYMBIAN) && !defined(UNDER_CE)
#include <sys/mman.h>
#endif

#include <stdio.h>

namespace avmplus
{
#ifdef AVMPLUS_ARM

	#ifdef AVMPLUS_VERBOSE
	const char* const ArmAssembler::conditionCodes[] =
	{
		"eq",
		"ne",
		"cs",
		"cc",
		"mi",
		"pl",
		"vs",
		"vc",
		"hi",
		"ls",
		"ge",
		"lt",
		"gt",
		"le",
		"  ",
		"nv"
	};
			
	const char* const ArmAssembler::regNames[] =
	{
		"r0",
		"r1",
		"r2",
		"r3",
		"r4",
		"r5",
		"r6",
		"r7",
		"r8",
		"r9",
		"r10",
		"fp",
		"ip",
		"sp",
		"lr",
		"pc"
	};
    #endif /* AVMPLUS_VERBOSE */

	ArmAssembler::ArmAssembler()
	{
		mip = 0;
		mipStart = 0;
		mInstructionCount = 0;
		#ifdef AVMPLUS_VERBOSE
		verboseFlag = false;
		#endif
		console = 0;
		conditionCode = AL;
		immediatePool = 0;
		immediatePoolCount = 0;
	}

	#define PRINT_LAST_INSTRUCTION()
	//#define PRINT_LAST_INSTRUCTION() console->format("%A\n", mip[-1])

	void ArmAssembler::IMM32(int value)
	{
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  %d // 0x%A\n", mip, value, value);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = value;
	}
	
	void ArmAssembler::MOV(Register dst, Register src)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  mov%s   %R, %R\n", mip, conditionCodes[conditionCode], dst, src);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x01A00000 | (conditionCode<<28) | (dst<<12) | src;

		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::LSL(Register dst, Register src, Register rShift)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  lsl%s   %R, %R, %R\n", mip, conditionCodes[conditionCode], dst, src, rShift);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x01A00010 | (conditionCode<<28) | (dst<<12) | src | (rShift<<8);

		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::LSR(Register dst, Register src, Register rShift)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  lsr%s   %R, %R, %R\n", mip, conditionCodes[conditionCode], dst, src, rShift);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x01A00030 | (conditionCode<<28) | (dst<<12) | src | (rShift<<8);

		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::ASR(Register dst, Register src, Register rShift)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  asr%s   %R, %R, %R\n", mip, conditionCodes[conditionCode], dst, src, rShift);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x01A00050 | (conditionCode<<28) | (dst<<12) | src | (rShift<<8);

		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::LSL_i(Register dst, Register src, int iShift)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  lsl%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, iShift);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x01A00000 | (conditionCode<<28) | (dst<<12) | src | (iShift<<7);

		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::LSR_i(Register dst, Register src, int iShift)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  lsr%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, iShift);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x01A00020 | (conditionCode<<28) | (dst<<12) | src | (iShift<<7);

		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::ASR_i(Register dst, Register src, int iShift)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  asr%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, iShift);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x01A00040 | (conditionCode<<28) | (dst<<12) | src | (iShift<<7);

		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::CMP(Register Rn, Register Rm)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  cmp%s   %R, %R\n", mip, conditionCodes[conditionCode], Rn, Rm);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x01500000 | (conditionCode<<28) | (Rn<<16) | Rm;

		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::ADD(Register dst, Register src1, Register src2)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  add%s   %R, %R, %R\n", mip, conditionCodes[conditionCode], dst, src1, src2);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x00800000 | (conditionCode<<28) | (dst<<12) | (src1<<16) | src2;

		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::AND(Register dst, Register src1, Register src2)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  and%s   %R, %R, %R\n", mip, conditionCodes[conditionCode], dst, src1, src2);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x00000000 | (conditionCode<<28) | (dst<<12) | (src1<<16) | src2;

		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::ORR(Register dst, Register src1, Register src2)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  orr%s   %R, %R, %R\n", mip, conditionCodes[conditionCode], dst, src1, src2);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x01800000 | (conditionCode<<28) | (dst<<12) | (src1<<16) | src2;

		PRINT_LAST_INSTRUCTION();
	}	

	void ArmAssembler::EOR(Register dst, Register src1, Register src2)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  eor%s   %R, %R, %R\n", mip, conditionCodes[conditionCode], dst, src1, src2);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x00200000 | (conditionCode<<28) | (dst<<12) | (src1<<16) | src2;

		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::MUL(Register dst, Register src1, Register src2)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  mul%s   %R, %R, %R\n", mip, conditionCodes[conditionCode], dst, src1, src2);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x00000090 | (conditionCode<<28) | (dst<<16) | (src1<<8) | src2;

		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::SUB(Register dst, Register src1, Register src2)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  sub%s   %R, %R, %R\n", mip, conditionCodes[conditionCode], dst, src1, src2);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x00400000 | (conditionCode<<28) | (dst<<12) | (src1<<16) | src2;

		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::STMFD_bang(Register dst, int mask)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  stmfd%s %R!, %A\n", mip, conditionCodes[conditionCode], dst, mask);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x09200000 | (conditionCode<<28) | (dst<<16) | mask;

		PRINT_LAST_INSTRUCTION();		
	}

	void ArmAssembler::SUB_imm8(Register dst, Register src, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  sub%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, imm8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x02400000 | (conditionCode<<28) | (src<<16) | (dst<<12) | (imm8&0xFF);		
		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::EOR_imm8(Register dst, Register src, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  eor%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, imm8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x02200000 | (conditionCode<<28) | (src<<16) | (dst<<12) | (imm8&0xFF);		
		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::ORR_imm8(Register dst, Register src, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  orr%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, imm8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x03800000 | (conditionCode<<28) | (src<<16) | (dst<<12) | (imm8&0xFF);		
		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::RSB_imm8(Register dst, Register src, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  rsb%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, imm8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x02600000 | (conditionCode<<28) | (src<<16) | (dst<<12) | (imm8&0xFF);		
		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::BL(int offset24)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  bl%s    %A\n", mip, conditionCodes[conditionCode], offset24);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x0B000000 | (conditionCode<<28) | ((offset24-8)>>2)&0xFFFFFF;
		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::B(int offset24)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  b%s     %A\n", mip, conditionCodes[conditionCode], offset24);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x0A000000 | (conditionCode<<28) | ((offset24-8)>>2)&0xFFFFFF;
		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::LDR(Register dst, int offset, Register base)
	{
		int off = offset;
		int opcode;
		if (off<0) {
			off = -off;
			opcode = 0x05100000;
		} else {
			opcode = 0x05900000;
		}

		if (off > 0xFFF)
		{
			AvmAssert(base != IP);
			AvmAssert(base != PC);
			MOV_imm32(IP, off);
			off = (int)IP;
			opcode |= 0x02000000;
		}

		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag) {
			if (base == PC) {
				console->format("    %A  ldr%s   %R, %d(%R) // %A\n",
					mip, conditionCodes[conditionCode], dst, offset, base, offset+8+(int)mip);
			} else if (opcode & 0x02000000) {
				console->format("    %A  ldr%s   %R, %R(%R)\n", mip, conditionCodes[conditionCode], dst, off, base);
			} else {
				console->format("    %A  ldr%s   %R, %d(%R)\n", mip, conditionCodes[conditionCode], dst, offset, base);
			}
		}
        #endif /* AVMPLUS_VERBOSE */
		AvmAssert(off <= 0xFFF);
		*mip++ = opcode | (conditionCode<<28) | (base<<16) | (dst<<12) | (off&0xFFF);
		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::BIC_imm8(Register dst, Register src, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  bic%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, imm8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x03C00000 | (conditionCode<<28) | (src<<16) | (dst<<12) | (imm8&0xFF);
		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::MOV_imm8(Register dst, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  mov%s   %R, %d\n", mip, conditionCodes[conditionCode], dst, imm8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x03A00000 | (conditionCode<<28) | (dst<<12) | (imm8&0xFF);
		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::MOV_imm16(Register dst, int imm16)
	{
		AvmAssert((imm16 & 0xFFFF0000) == 0);

		MOV_imm8 (dst, imm16 & 0xFF);

		if (imm16 & 0xFF00)
		{
			ADD_imm8_hi (dst, dst, (imm16 >> 8) & 0xFF);
		}
	}

	void ArmAssembler::ADD_imm16(Register dst, Register src, int imm16)
	{
		AvmAssert((imm16 & 0xFFFF0000) == 0);

		ADD_imm8 (dst, src, imm16 & 0xFF);

		if (imm16 & 0xFF00)
		{
			ADD_imm8_hi (dst, dst, (imm16 >> 8) & 0xFF);
		}
	}
	
	// Cheesy macro for doing IMM32.  Needs work.
	void ArmAssembler::MOV_imm32(Register dst, int imm32)
	{
		if ((imm32&0xFFFF0000) == 0)
		{
			// Use imm8/imm16 instead
			MOV_imm16(dst, imm32);
			return;
		}
		
		// If there is still immediate pool available, and it is in range of LDR,
		// use it.
		if (immediatePool != 0 && mip-immediatePool < 500)
		{
			// Use existing immediate pool
		}
		else
		{
			// Allocate immediate pool
			
			// - Branch to instruction after immediate pool
			B(4+kImmediatePoolMax*4);

			// - New immediate pool starts here
			immediatePool = mip;
			immediatePoolCount = kImmediatePoolMax;
			mip += immediatePoolCount;
		}

		MDInstruction *savedMip = mip;
		mip = immediatePool;

		// Write the constant
		IMM32(imm32);

		mip = savedMip;

		// Load the constant (-8 due to ARM pipeline's +8 bias)
		LDR(dst, -8-(mip-immediatePool)*4, PC);

		immediatePool++;
		if (!--immediatePoolCount)
		{
			immediatePool = 0;
		}
	}

	void ArmAssembler::CMP_imm8(Register src, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  cmp%s   %R, %d\n", mip, conditionCodes[conditionCode], src, imm8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x03500000 | (conditionCode<<28) | (src<<16) | (imm8&0xFF);
		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::STR(Register src, int offset, Register base)
	{
		int off = offset;
		int opcode;
		if (off<0) {
			off = -off;
			opcode = 0x05000000;
		} else {
			opcode = 0x05800000;
		}

		if (off > 0xFFF)
		{
			AvmAssert(base != IP);
			AvmAssert(src != IP);
			AvmAssert(base != PC);
			MOV_imm32(IP, off);
			off = (int)IP;
			opcode |= 0x02000000;
		}

		incInstructionCount();
		#ifdef AVMPLUS_VERBOSE
		if (verboseFlag) {
			if (base == PC) {
				console->format("    %A  str%s   %R, %d(%R) // %A\n", mip, conditionCodes[conditionCode], src, offset, base,
							offset+8+(int)mip);
			} else if (opcode & 0x02000000) {
				console->format("    %A  str%s   %R, %R(%R)\n", mip, conditionCodes[conditionCode], src, off, base);
			} else {
				console->format("    %A  str%s   %R, %d(%R)\n", mip, conditionCodes[conditionCode], src, offset, base);
			}
		}
		#endif /* AVMPLUS_VERBOSE */
		AvmAssert(off <= 0xFFF);
		*mip++ = opcode | (conditionCode<<28) | (base<<16) | (src<<12) | (off&0xFFF);
		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::ADD_imm8(Register dst, Register src, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  add%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, imm8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x02800000 | (conditionCode<<28) | (src<<16) | (dst<<12) | (imm8&0xFF);
		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::ADD_imm8_hi(Register dst, Register src, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  add%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, imm8 << 8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x02800C00 | (conditionCode<<28) | (src<<16) | (dst<<12) | (imm8&0xFF);
		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::AND_imm8(Register dst, Register src, int imm8)
	{
		AvmAssert((imm8 & 0xFFFFFF00) == 0);
		
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  and%s   %R, %R, %d\n", mip, conditionCodes[conditionCode], dst, src, imm8);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x02000000 | (conditionCode<<28) | (src<<16) | (dst<<12) | (imm8&0xFF);
		PRINT_LAST_INSTRUCTION();
	}
	
	void ArmAssembler::LDMFD(Register src, int mask)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  ldmfd%s %R, %A\n", mip, conditionCodes[conditionCode], src, mask);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x08900000 | (conditionCode<<28) | (src<<16) | mask;
		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::LDMFD_bang(Register src, int mask)
	{
		incInstructionCount();
        #ifdef AVMPLUS_VERBOSE
		if (verboseFlag)
			console->format("    %A  ldmfd%s! %R, %A\n", mip, conditionCodes[conditionCode], src, mask);
        #endif /* AVMPLUS_VERBOSE */
		*mip++ = 0x08B00000 | (conditionCode<<28) | (src<<16) | mask;
		PRINT_LAST_INSTRUCTION();
	}

	void ArmAssembler::SET_CONDITION_CODE(ConditionCode conditionCode)
	{
		this->conditionCode = conditionCode;
	}

#ifdef AVMPLUS_MIR

	void CodegenMIR::emitNativeThunk(NativeMethod *info)
	{
		SET_CONDITION_CODE(AL);
		
		// Hack to see what instructions we're generating
		//bool hack = true;
		//verboseFlag = true;
		//core->console << " native thunk for " << info << "\n";

		const Register TEMP  = R7;
		const Register AP    = R6;
		const Register ARGC  = R5;
#ifdef DEBUGGER
		const Register ENV   = R4;
		const Register SAVE0 = R5;
		const Register SAVE1 = R6;
		const int NonVolatileMask = R4_mask | R5_mask | R6_mask | R7_mask;
		const int NonVolatileCount = 4;
#else
		const int NonVolatileMask = R5_mask | R6_mask | R7_mask;
		const int NonVolatileCount = 3;
#endif

		if (!pool)
			core->console << "no pool " << info << "\n";

		code = getMDBuffer(pool);
		mip = (MDInstruction*)code;
		if (!code)
		{
			overflow = true;
			return;
		}

#ifdef FEATURE_BUFFER_GUARD
		GrowthGuard guard(pool->codeBuffer);
#else
		if (!ensureMDBufferCapacity(pool, md_native_thunk_size))
			return;
#endif /* FEATURE_BUFFER_GUARD */

		int in_arg_offset = 0;

		const int kMaxGPRIndex = 4;

		#ifdef DEBUGGER
		uint32 *sendEnterAddr = mip;
		IMM32(ENVADDR(MethodEnv::sendEnter));

		uint32 *sendExitAddr = mip;
		IMM32(ENVADDR(MethodEnv::sendExit));
		#endif

#ifdef _MSC_VER
		uint32 *handlerAddr = mip;
		IMM32(info->m_handler_addr);
#else
		union {
			int handler_addr[2];
			NativeMethod::Handler handler;
		};
		handler = info->m_handler;

		uint32 *handlerAddr = mip;
		IMM32(handler_addr[1]);
#endif
		
		// scan for double optional arguments; these will
		// be constants stored up front
		uint32 *const_ip = mip;
		int first_optional = 1+info->param_count-info->optional_count;
		for (int i=first_optional; i <= info->param_count; i++)
		{
			Traits* type = info->paramTraits(i);
			Atom arg = info->getDefaultValue(i-first_optional);
			
			if (type == OBJECT_TYPE || type == VOID_TYPE)
			{
				IMM32(arg);
			}
			else if (type == BOOLEAN_TYPE)
			{
				IMM32(arg>>3);
			}
			else if (type == NUMBER_TYPE)
			{
				double d = core->number_d(arg);
				int *dp = (int*)&d;
				IMM32(dp[0]);
				IMM32(dp[1]);
			}
			else if (type == INT_TYPE)
			{
				int i = core->integer_i(arg);
				IMM32(i);
			}
			else if (type == UINT_TYPE)
			{
				uint32 u = core->integer_u(arg);
				IMM32(u);
			}
			else
			{
				// push pointer type
				// this case includes kStringType, kObjectType, and kNamespaceType
				int p = (arg&7)==kSpecialType ? 0 : arg & ~7;
				IMM32(p);
			}				
		}

		mipStart = mip;

		// prologue
		MOV     (IP, SP);

		// save callee-saved registers
		STMFD_bang (SP, NonVolatileMask | FP_mask | IP_mask | LR_mask | PC_mask);
		SUB_imm8 (FP, IP, 4);

		// the frame size will be updated later
		uint32 *patch_frame_size = mip;
		SUB_imm8 (SP, SP, 0);
		
		// set up other registers
		MOV     (AP,   R2);
		MOV     (ARGC, R1);
#ifdef DEBUGGER
		MOV     (ENV,  R0);
#endif

#ifdef DEBUGGER
		//MOV (R0, ENV);
		//MOV (R1, ARGC);
		//MOV (R2, AP);
		LDR (IP, (int)sendEnterAddr-(int)mip-8, PC);
		MOV (LR, PC);
		MOV (PC, IP);
		//thincall(ENVADDR(MethodEnv::sendEnter));
#endif

		// Push receiver object as first argument
#ifdef NATIVE_GLOBAL_FUNCTION_HACK
		// hack - native toplevel functions get invoked with user defined receiver
		// but are expecting the global object.  this hack is totally unsound
		// because it gives users the power to crash the vm.  we need to either
		// directly support toplevel native functions, or not use them.
		LDR     (R0, 0, AP); // this is ap[0]
		BIC_imm (R0, R0, 7);
#else
		LDR     (R0, 0, AP); // this is ap[0]
#endif
		in_arg_offset += 4;
		int GPRIndex = 1;
		
		bool need_rest = ((info->flags & AbstractFunction::NEED_REST) != 0);
		
		if (info->flags & AbstractFunction::NATIVE_COOKIE)
		{
			MOV_imm8 (R1, info->m_cookie);
			GPRIndex++;
		}

		// push args left to right
		for (int i=1; i <= info->param_count; i++)
		{
			// param 0 is receiver, real params start at 1
			Traits* type = info->paramTraits(i);

			if (i >= first_optional) {
#ifdef AVMPLUS_VERBOSE
				if (verboseFlag) core->console << "optional " << i << "\n";
#endif /* AVMPLUS_VERBOSE */
				// emit code to check whether argument
				// was specified
				CMP_imm8 (ARGC, i);

				SET_CONDITION_CODE(LT);

				// emit code to push the default value
				Atom arg = info->getDefaultValue(i-first_optional);

				// the arguments have already been coerced to the right type
				// by the caller.  We do not do type conversions here.

				int offset = (int)const_ip - (int)mip - 8; // -8 for pc-relative bias

				if (type == NUMBER_TYPE)
				{
					// Do first word of double
					if (GPRIndex < kMaxGPRIndex) {
						LDR ((Register)(R0+GPRIndex), offset, PC);
					} else {
						LDR (TEMP, offset, PC);
						STR (TEMP, (GPRIndex-kMaxGPRIndex)*4, SP);
					}

					// Do second word of double
					if ((GPRIndex+1) < kMaxGPRIndex) {
						LDR ((Register)(R0+GPRIndex+1), offset+4, PC);
					} else {
						LDR (TEMP, offset+4, PC);
						STR (TEMP, (GPRIndex+1-kMaxGPRIndex)*4, SP);
					}
					
					const_ip += 2;
				}
				else
				{
					if (GPRIndex < kMaxGPRIndex) {
						LDR ((Register)(R0+GPRIndex), offset, PC);
					} else {
						LDR (TEMP, offset, PC);
						STR (TEMP, (GPRIndex-kMaxGPRIndex)*4, SP);
					}

					const_ip++;
				}

				SET_CONDITION_CODE(GE);
			}

			// Generate the code for the non-optional case.
			// these args will already be converted to native form by the caller			
			if (type == NUMBER_TYPE)
			{
				// Copy first word of double
				if (GPRIndex < kMaxGPRIndex) {
					LDR((Register)(R0+GPRIndex), in_arg_offset, AP);
				} else {
					LDR(TEMP, in_arg_offset, AP);
					STR(TEMP, (GPRIndex-kMaxGPRIndex)*4, SP);
				}
				GPRIndex++;
				in_arg_offset += 4;

				// Copy second word of double
				if (GPRIndex < kMaxGPRIndex) {
					LDR((Register)(R0+GPRIndex), in_arg_offset, AP);
				} else {
					LDR(TEMP, in_arg_offset, AP);
					STR(TEMP, (GPRIndex-kMaxGPRIndex)*4, SP);
				}

				GPRIndex++;
				in_arg_offset += 4;
			}
			else
			{
				// push general type
				// this case includes everything except Number/double
				if (GPRIndex < kMaxGPRIndex) {
					LDR((Register)(R0+GPRIndex), in_arg_offset, AP);
				} else {
					LDR(TEMP, in_arg_offset, AP);
					STR(TEMP, (GPRIndex-kMaxGPRIndex)*4, SP);
				}
				in_arg_offset += 4;
				GPRIndex++;
			}

			SET_CONDITION_CODE(AL);
		}

		if (need_rest) {
#ifdef AVMPLUS_VERBOSE
			if (verboseFlag)
				core->console << "need_rest\n";
#endif /* AVMPLUS_VERBOSE */
			// If the method signature ends with "...",
			// deliver any arguments after the explicitly
			// specified ones as (Atom *argv, int argc)

			// rest_count = argc-param_count
			CMP_imm8 (ARGC, info->param_count);
			SET_CONDITION_CODE (LT);
			
			// rest_count<0, push argc=0, argv=NULL

			// push argc
			if (GPRIndex < kMaxGPRIndex) {
				MOV_imm8((Register)(R0+GPRIndex), 0);
			} else {
				MOV_imm8(TEMP, 0);
				STR(TEMP, (GPRIndex-kMaxGPRIndex)*4, SP);
			}

			// push argv
			if ((GPRIndex+1) < kMaxGPRIndex) {
				MOV_imm8((Register)(R0+GPRIndex+1), 0);
			} else {
				MOV_imm8(TEMP, 0);
				STR(TEMP, (GPRIndex+1-kMaxGPRIndex)*4, SP);
			}
			
			SET_CONDITION_CODE(GE);

			// rest_count>=0
			// push rest_count
			// push rest_argv = argv+param_count

			// push argv
			if (GPRIndex < kMaxGPRIndex) {
				ADD_imm8((Register)(R0+GPRIndex), AP, info->restOffset);
			} else {
				ADD_imm8(TEMP, AP, info->restOffset);
				STR(TEMP, (GPRIndex-kMaxGPRIndex)*4, SP);
			}
			GPRIndex++;
			
			// push argc
			if (GPRIndex < kMaxGPRIndex) {
				SUB_imm8((Register)(R0+GPRIndex), ARGC, info->param_count);
			} else {
				SUB_imm8(TEMP, ARGC, info->param_count);
				STR(TEMP, (GPRIndex-kMaxGPRIndex)*4, SP);
			}
			GPRIndex++;

			SET_CONDITION_CODE(AL);
		}

		// all args have been pushed, now call function using thiscall calling conventions
		Traits* type = info->returnTraits();

		LDR (IP, (int)handlerAddr-(int)mip-8, PC);
		MOV (LR, PC);
		MOV (PC, IP);
		//thincall(handler_addr);

#ifdef DEBUGGER
		if (type == NUMBER_TYPE)
		{
			// result is in R0, R1.  Store in nonvolatile while
			// we call sendExit.
			MOV (SAVE0, R0);
			MOV (SAVE1, R1);
		}
		else
		{
			// result is in R0.  Store in nonvolatile while we
			// call sendExit.
			MOV (SAVE0, R0);
		}

		MOV (R0, ENV);
		LDR (IP, (int)sendExitAddr-(int)mip-8, PC);
		MOV (LR, PC);
		MOV (PC, IP);
		//thincall(ENVADDR(MethodEnv::sendExit));

		if (type == NUMBER_TYPE)
		{
			MOV (R0, SAVE0);
			MOV (R1, SAVE1);
		}
		else
		{
			MOV (R0, SAVE0);
		}
#endif

		if (type != NUMBER_TYPE)
		{
			// result in R3
			if (type == BOOLEAN_TYPE)
			{
				// return value already in R3
				// bool is just a byte, so mask it off
				AND_imm8 (R0, R0, 255);
			}
			else if (type == VOID_TYPE)
			{
				MOV_imm8 (R0, undefinedAtom);
			}
		}
		// else, result in FPU register F1
				
		// epilogue
		SUB_imm8 (SP, FP, 12 + NonVolatileCount * 4);
		LDMFD (SP, NonVolatileMask | FP_mask | SP_mask | PC_mask);

		// Patch up the frame size.
		// For each arg, we need 4 bytes in the called parameter
		// area.  We also leave a little cushion in the called
		// parameter area, since there are a few other function
		// calls the thunk makes (intToAtom, doubleToAtom)
		if (GPRIndex > kMaxGPRIndex)
		{
			int frameSize = (GPRIndex-kMaxGPRIndex)*4;
			*patch_frame_size |= (frameSize & 0xFF);
		}

		bindMethod(info);

		//if (hack)
			//verboseFlag = false;
	}

	void ArmAssembler::flushDataCache(void *start, int len)
	{
		#ifdef AVMPLUS_SYMBIAN

		// Adjust start and len to page boundaries
		start = (void*) ((int)start & ~0xFFF);
		len = (len+8192) & ~0xFFF;
		void *end = (void*)((int)start+len);

		User::IMB_Range(start, end);
		
		#elif defined AVMPLUS_IYONIX
		
		// Adjust start and len to page boundaries
		start = (void*) ((int)start & ~0xFFF);
		len = (len+8192) & ~0xFFF;
		void *end = (void*)((int)start+len);

		register unsigned long _beg __asm ("a1") = (unsigned long)(start);
        register unsigned long _end __asm ("a2") = (unsigned long)(end);
        register unsigned long _flg __asm ("a3") = 0;
        __asm __volatile ("swi 0x9f0002		@ sys_cacheflush"
		    : /* no outputs */
		    : /* no inputs */
		    : "a1");

        #elif defined(UNDER_CE)

		// This seems to work, but might not be correct
		// Might be better to call CacheSync(...)
		FlushInstructionCache(GetCurrentProcess(), 0, 0);

		#else

		// Flush the data cache crudely
		#define SCRATCH_SIZE 32768
		volatile int scratch[SCRATCH_SIZE];
		int sum = 0;
		for (int i=0; i<SCRATCH_SIZE; i++) {
			sum += scratch[i];
		}

        #endif
	}
	
	void* CodegenMIR::emitImtThunk(ImtBuilder::ImtEntry *e)
	{
		SET_CONDITION_CODE(AL);
		
		mip = mipStart = (MDInstruction*)getMDBuffer(pool);

#ifdef FEATURE_BUFFER_GUARD
		GrowthGuard guard(pool->codeBuffer);
#else
		if (!ensureMDBufferCapacity(pool, md_native_thunk_size))
			return NULL;
#endif /* FEATURE_BUFFER_GUARD */

		// the generated thunk does not call any helper methods, so we are
		// free to use eax, ecx, edx as scratch regs without touchning the
		// stack.
		
		// in: R0 = MethodEnv for interface thunk
		//     R1 = argc
		//     R2 = ap
		//     R3 = iid
		//     0(ap)  = ScriptObject (concrete instance of class)

#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
		{
			core->console << "imt thunk\n";
		}
#endif

		AvmAssert(e->next != NULL); // must have 2 or more entries

		while (e->next)
		{
			ImtBuilder::ImtEntry *next = e->next;

			#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "              disp_id="<< e->disp_id << " " << e->virt << "\n";
			}
			#endif

			MOV_imm32(IP, e->virt->iid());
			CMP(R3, IP);

			SET_CONDITION_CODE(EQ);
			LDR (IP, 0, R2); // obj
			LDR (IP, offsetof(ScriptObject, vtable), IP); // vtable
			LDR (R0, offsetof(VTable,methods)+4*e->disp_id, IP); // load concrete env
			LDR (PC, offsetof(MethodEnv, impl32), R0); // invoke real method indirectly
			SET_CONDITION_CODE(AL);

			pool->core->gc->Free(e);
			e = next;
		}

		// last one is unconditional
		#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
		{
			core->console << "              disp_id="<< e->disp_id << " " << e->virt << "\n";
		}
		#endif
		LDR (IP, 0, R2); // obj
		LDR (IP, offsetof(ScriptObject, vtable), IP); // vtable
		LDR (R0, offsetof(VTable,methods)+4*e->disp_id, IP); // load concrete env
		LDR (PC, offsetof(MethodEnv, impl32), R0); // invoke real method indirectly

		MDInstruction *retval;
		
		retval = mipStart;
		
		// lock in the next available location in the buffer (16B aligned)
		pool->codeBuffer->setPos((byte*) ( (size_t)mip+15 & ~15 ));

		return retval;
	}

	bool CodegenMIR::canImmFold(OP* ins, OP* rhs) const
	{
		if (rhs->code != MIR_imm)
			return false;
		if (rhs->imm & 0xFFFFFF00)
			return false;
		switch (ins->code)
		{
		case MIR_add:
			return (rhs->imm & 0xFFFF0000) == 0;			
		case MIR_and:
		case MIR_or:
		case MIR_xor:
		case MIR_sub:
			return (rhs->imm & 0xFFFFFF00) == 0;
		case MIR_lsh:
		case MIR_rsh:
		case MIR_ush:
			return true;
		}
		return false;
	}
#endif /* AVMPLUS_MIR */
	
#endif /* AVMPLUS_ARM */
}
