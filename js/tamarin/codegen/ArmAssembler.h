/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __avmplus_ArmAssembler__
#define __avmplus_ArmAssembler__


namespace avmplus
{
	class ArmAssembler
	{
	public:
		/* ARM registers */
		typedef enum 
		{
			R0  = 0,
			R1  = 1,
			R2  = 2,
			R3  = 3,
			R4  = 4,
			R5  = 5,
			R6  = 6,
			R7  = 7,
			R8  = 8,
			R9  = 9,
			R10 = 10,
			FP  = 11,
			IP  = 12,
			SP  = 13,
			LR  = 14,
			PC  = 15,

			// Pseudo-register for floating point
			F0  = 0,
			
			Unknown = -1
		}
		Register;

		/* ARM registers */
		typedef enum 
		{
			R0_mask  = (1<<0),
			R1_mask  = (1<<1),
			R2_mask  = (1<<2),
			R3_mask  = (1<<3),
			R4_mask  = (1<<4),
			R5_mask  = (1<<5),
			R6_mask  = (1<<6),
			R7_mask  = (1<<7),
			R8_mask  = (1<<8),
			R9_mask  = (1<<9),
			R10_mask = (1<<10),
			FP_mask  = (1<<11),
			IP_mask  = (1<<12),
			SP_mask  = (1<<13),
			LR_mask  = (1<<14),
			PC_mask  = (1<<15)
		}
		RegisterMask;
		
		/* ARM condition codes */
		typedef enum
		{
			EQ = 0x0, // Equal
			NE = 0x1, // Not Equal
			CS = 0x2, // Carry Set (or HS)
			CC = 0x3, // Carry Clear (or LO)
			MI = 0x4, // MInus
			PL = 0x5, // PLus
			VS = 0x6, // oVerflow Set
			VC = 0x7, // oVerflow Clear
			HI = 0x8, // HIgher
			LS = 0x9, // Lower or Same
			GE = 0xA, // Greater or Equal
			LT = 0xB, // Less Than
			GT = 0xC, // Greater Than
			LE = 0xD, // Less or Equal
			AL = 0xE, // ALways
			NV = 0xF  // NeVer
		}
		ConditionCode;

		/* --- Data Processing Instructions --- */

		/* Values for operator "a" */
		typedef enum
		{
			AND_op = 0x0, // Boolean And                 Rd = Rn AND Op2
			EOR_op = 0x1, // Boolean Eor                 Rd = Rn EOR Op2
			SUB_op = 0x2, // Subtract                    Rd = Rn - Op2
			RSB_op = 0x3, // Reverse Subtract            Rd = Op2 - Rn
			ADD_op = 0x4, // Addition                    Rd = Rn + Op2
			ADC_op = 0x5, // Add with Carry              Rd = Rn + Op2 + C
			SBC_op = 0x6, // Subtract with Carry         Rd = Rn - Op2 - (1-C)
			RSC_op = 0x7, // Reverse Subtract with Carry Rd - Op2 - Rn - (1-C)
			TST_op = 0x8, // Test bit                    Rn AND Op2
			TEQ_op = 0x9, // Test equality               Rn EOR Op2
			CMP_op = 0xA, // Compare                     Rn - Op2
			CMN_op = 0xB, // Compare Negative            Rn + Op2
			ORR_op = 0xC, // Boolean Or                  Rd = Rn OR Op2
			MOV_op = 0xD, // Move value                  Rd = Op2
			BIC_op = 0xE, // Bit Clear                   Rd = Rn AND NOT Op2
			MVN_op = 0xF  // Move Not                    Rd = NOT Op2
		}
		Operator;

		/* Values for operand "t" */
		typedef enum
		{
			LSL_imm = 0, // LSL #c - Logical Shift Left
			LSL_reg = 1, // LSL Rc - Logical Shift Left
			LSR_imm = 2, // LSR #c - Logical Shift Right
			LSR_reg = 3, // LSR Rc - Logical Shift Right
			ASR_imm = 4, // ASR #c - Arithmetic Shift Right
			ASR_reg = 5, // ASR Rc - Arithmetic Shift Right
			ROR_imm = 6, // Rotate Right (c != 0)
			RRX     = 6, // Rotate Right one bit with extend (c == 0)
			ROR_reg = 7  // Rotate Right
		}
		ShiftOperator;

		typedef uint32 MDInstruction;
		
		/* Instruction pointer */
		MDInstruction *mip;
		MDInstruction *mipStart;

		int		mInstructionCount;  // number of machine instructions
		#define incInstructionCount() mInstructionCount++		

		/* Current condition code */
		ConditionCode conditionCode;

		ArmAssembler();
		
		void SET_CONDITION_CODE(ConditionCode conditionCode);
		
		void IMM32(int value);
		void MOV(Register dst, Register src);
		void STMFD_bang(Register dst, int mask);
		void SUB_imm8(Register dst, Register src, int imm8);
		void RSB_imm8(Register dst, Register src, int imm8);
		void B(int offset24);		
		void BL(int offset24);
		void LDR(Register dst, int offset, Register base);
		void BIC_imm8(Register dst, Register src, int imm8);
		void MOV_imm8(Register dst, int imm8);
		void MOV_imm16(Register dst, int imm16);
		void CMP_imm8(Register src, int imm8);
		void ADD(Register dst, Register src1, Register src2);
		void SUB(Register dst, Register src1, Register src2);
		void AND(Register dst, Register src1, Register src2);
		void ORR(Register dst, Register src1, Register src2);
		void EOR(Register dst, Register src1, Register src2);
		void MUL(Register dst, Register src1, Register src2);
		void CMP(Register Rn, Register Rm);		
		void LSL(Register dst, Register src, Register rShift);
		void LSR(Register dst, Register src, Register rShift);
		void ASR(Register dst, Register src, Register rShift);
		void LSL_i(Register dst, Register src, int iShift);
		void LSR_i(Register dst, Register src, int iShift);
		void ASR_i(Register dst, Register src, int iShift);
		void STR(Register src, int offset, Register base);
		void ADD_imm8(Register dst, Register src, int imm8);
		void ADD_imm8_hi(Register dst, Register src, int imm8);
		void ADD_imm16(Register dst, Register src, int imm16);
		void AND_imm8(Register dst, Register src, int imm8);
		void ORR_imm8(Register dst, Register src, int imm8);
		void EOR_imm8(Register dst, Register src, int imm8);
		void LDMFD(Register src, int mask);
		void LDMFD_bang(Register src, int mask);		

		// Cheeseball way of doing imm32.
		void MOV_imm32(Register dst, int imm32);

		void flushDataCache(void *start, int len);
		
		// Set if verbose output desired
		#ifdef AVMPLUS_VERBOSE
		bool verboseFlag;
		#endif
		PrintWriter *console;

		// Immediate pool
		MDInstruction *immediatePool;
		int immediatePoolCount;
		const static int kImmediatePoolMax = 16;
		
		static const char* const regNames[];
#define gpregNames regNames
		static const char* const conditionCodes[];
	};
}

#endif /* __avmplus_ArmAssembler__ */
