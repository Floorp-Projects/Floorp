/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 4 -*- */
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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
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

#include "nanojit.h"

#ifdef AVMPLUS_PORTING_API
#include "portapi_nanojit.h"
#endif

#ifdef UNDER_CE
#include <cmnintrin.h>
#endif

#if defined(AVMPLUS_LINUX)
#include <asm/unistd.h>
#endif

namespace nanojit
{
	#ifdef FEATURE_NANOJIT

#ifdef NJ_VERBOSE
	const char* regNames[] = {"r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","r11","IP","SP","LR","PC"};

#endif
    const Register Assembler::argRegs[] = { R0, R1, R2, R3 };
    const Register Assembler::retRegs[] = { R0, R1 };
	const Register Assembler::savedRegs[] = { R4, R5, R6, R7 };

	void Assembler::nInit(AvmCore*)
	{
		// Thumb mode does not have conditional move, alas
		has_cmov = false;
	}

	NIns* Assembler::genPrologue(RegisterMask needSaving)
	{
		/**
		 * Prologue
		 */

		// NJ_RESV_OFFSET is space at the top of the stack for us
		// to use for parameter passing (8 bytes at the moment)
		uint32_t stackNeeded = 4 * _activation.highwatermark + NJ_STACK_OFFSET;
		uint32_t savingCount = 0;

		uint32_t savingMask = 0;
		savingCount = 5; // R4-R7, LR
		savingMask = 0xF0;
		(void)needSaving;

		// so for alignment purposes we've pushed  return addr, fp, and savingCount registers
		uint32_t stackPushed = 4 * (2+savingCount);
		uint32_t aligned = alignUp(stackNeeded + stackPushed, NJ_ALIGN_STACK);
		int32_t amt = aligned - stackPushed;

		// Make room on stack for what we are doing
		if (amt)
		{
			// largest value is 508 (7-bits << 2)
			if (amt>508)
			{
				int size = 508;
				while (size>0)
				{
					SUBi(SP, size);
					amt -= size;
					size = amt;
					if (size>508)
						size=508;
				}
			}
			else
				SUBi(SP, amt); 

		}
		verbose_only( verbose_outputf("         %p:",_nIns); )
        verbose_only( verbose_output("         patch entry"); )
        NIns *patchEntry = _nIns;

		MR(FRAME_PTR, SP);
		PUSH_mask(savingMask|rmask(LR));
		return patchEntry;
	}

	void Assembler::nFragExit(LInsp guard)
	{
		SideExit* exit = guard->exit();
		Fragment *frag = exit->target;
		GuardRecord *lr;
		if (frag && frag->fragEntry)
		{
			JMP(frag->fragEntry);
			lr = 0;
		}
		else
		{
			// target doesn't exit yet.  emit jump to epilog, and set up to patch later.
			lr = placeGuardRecord(guard);

			BL(_epilogue);

			lr->jmp = _nIns;
		}

		// pop the stack frame first
		MR(SP, FRAME_PTR);

        #ifdef NJ_VERBOSE
        if (_frago->core()->config.show_stats) {
			// load R1 with Fragment *fromFrag, target fragment
			// will make use of this when calling fragenter().
            int fromfrag = int((Fragment*)_thisfrag);
            LDi(argRegs[1], fromfrag);
        }
        #endif

		// return value is GuardRecord*
        LDi(R2, int(lr));
	}

	NIns* Assembler::genEpilogue(RegisterMask restore)
	{
		(void)restore;
		if (false) {
			// interworking
			BX(R3); // return
			POPr(R3); // POP LR into R3
			POP_mask(0xF0); // {R4-R7}
		} else {
			// return to Thumb caller
			POP_mask(0xF0|rmask(PC));
		}
		MR(R0,R2); // return LinkRecord*
		return _nIns;
	}
	
	void Assembler::asm_call(LInsp ins)
	{
        const CallInfo* call = ins->callInfo();
		uint32_t atypes = call->_argtypes;
		uint32_t roffset = 0;

		// we need to detect if we have arg0 as LO followed by arg1 as F;
		// in that case, we need to skip using r1 -- the F needs to be
		// loaded in r2/r3, at least according to the ARM EABI and gcc 4.2's
		// generated code.
		bool arg0IsInt32FollowedByFloat = false;
		while ((atypes & 3) != ARGSIZE_NONE) {
			if (((atypes >> 4) & 3) == ARGSIZE_LO &&
				((atypes >> 2) & 3) == ARGSIZE_F &&
				((atypes >> 6) & 3) == ARGSIZE_NONE)
			{
				arg0IsInt32FollowedByFloat = true;
				break;
			}
			atypes >>= 2;
		}

		CALL(call);
        ArgSize sizes[10];
        uint32_t argc = call->get_sizes(sizes);
		for(uint32_t i=0; i < argc; i++)
		{
            uint32_t j = argc - i - 1;
            ArgSize sz = sizes[j];
            NanoAssert(sz == ARGSIZE_LO || sz == ARGSIZE_Q);
    		// pre-assign registers R0-R3 for arguments (if they fit)
            Register r = (i+roffset) < 4 ? argRegs[i+roffset] : UnknownReg;
            asm_arg(sz, ins->arg(j), r);

			if (i == 0 && arg0IsInt32FollowedByFloat)
				roffset = 1;
		}
	}
	
	void Assembler::nMarkExecute(Page* page, int flags)
	{
		NanoAssert(sizeof(Page) == NJ_PAGE_SIZE);
	#ifdef UNDER_CE
		static const DWORD kProtFlags[4] = 
		{
			PAGE_READONLY,			// 0
			PAGE_READWRITE,			// PAGE_WRITE
			PAGE_EXECUTE_READ,		// PAGE_EXEC
			PAGE_EXECUTE_READWRITE	// PAGE_EXEC|PAGE_WRITE
		};
		DWORD prot = kProtFlags[flags & (PAGE_WRITE|PAGE_EXEC)];
		DWORD dwOld;
		BOOL res = VirtualProtect(page, NJ_PAGE_SIZE, prot, &dwOld);
		if (!res)
		{
			// todo: we can't abort or assert here, we have to fail gracefully.
			NanoAssertMsg(false, "FATAL ERROR: VirtualProtect() failed\n");
		}
	#endif
	#ifdef AVMPLUS_PORTING_API
		NanoJIT_PortAPI_MarkExecutable(page, (void*)((char*)page+NJ_PAGE_SIZE), flags);
		// todo, must add error-handling to the portapi
	#endif
	}
			
	Register Assembler::nRegisterAllocFromSet(int set)
	{
		// need to implement faster way
		int i=0;
		while (!(set & rmask((Register)i)))
			i ++;
		_allocator.free &= ~rmask((Register)i);
		return (Register) i;
	}

	void Assembler::nRegisterResetAll(RegAlloc& a)
	{
		// add scratch registers to our free list for the allocator
		a.clear();
		a.used = 0;
		a.free = rmask(R0) | rmask(R1) | rmask(R2) | rmask(R3) | rmask(R4) | rmask(R5);
		debug_only(a.managed = a.free);
	}

	void Assembler::nPatchBranch(NIns* branch, NIns* target)
	{
		// Patch the jump in a loop

		// This is ALWAYS going to be a long branch (using the BL instruction)
		// Which is really 2 instructions, so we need to modify both
		// XXX -- this is B, not BL, at least on non-Thumb..

		// branch+2 because PC is always 2 instructions ahead on ARM/Thumb
		int32_t offset = int(target) - int(branch+2);

		//printf("---patching branch at 0x%08x to location 0x%08x (%d-0x%08x)\n", branch, target, offset, offset);

		NanoAssert(-(1<<21) <= offset && offset < (1<<21)); 
		*branch++ = (NIns)(0xF000 | (offset>>12)&0x7FF);
		*branch =   (NIns)(0xF800 | (offset>>1)&0x7FF);
	}

	RegisterMask Assembler::hint(LIns* i, RegisterMask allow /* = ~0 */)
	{
		uint32_t op = i->opcode();
		int prefer = ~0;

		if (op==LIR_call || op==LIR_fcall)
			prefer = rmask(R0);
		else if (op == LIR_callh)
			prefer = rmask(R1);
		else if (op == LIR_param)
			prefer = rmask(imm2register(argRegs[i->imm8()]));

		if (_allocator.free & allow & prefer)
			allow &= prefer;
		return allow;
	}

    void Assembler::asm_qjoin(LIns *ins)
    {
		int d = findMemFor(ins);
		AvmAssert(d);
		LIns* lo = ins->oprnd1();
		LIns* hi = ins->oprnd2();
							
		Register r = findRegFor(hi, GpRegs);
		ST(FP, d+4, r);

        // okay if r gets recycled.
		r = findRegFor(lo, GpRegs);
		ST(FP, d, r);
        freeRsrcOf(ins, false);	// if we had a reg in use, emit a ST to flush it to mem
    }

    void Assembler::asm_store32(LIns *value, int dr, LIns *base)
    {
	    // make sure what is in a register
	    Reservation *rA, *rB;
	    findRegFor2(GpRegs, value, rA, base, rB);
	    Register ra = rA->reg;
	    Register rb = rB->reg;
	    ST(rb, dr, ra);
    }

	void Assembler::asm_restore(LInsp i, Reservation *resv, Register r)
	{
		(void)resv;
        int d = findMemFor(i);
	    LD(r, d, FP);
		verbose_only(if (_verbose) {
			outputf("        restore %s",_thisfrag->lirbuf->names->formatRef(i));
		})
	}

	void Assembler::asm_spill(LInsp i, Reservation *resv, bool pop)
	{
    (void)i;
		(void)pop;
		if (resv->arIndex)
		{
			int d = disp(resv);
			// save to spill location
			Register rr = resv->reg;
			ST(FP, d, rr);
			verbose_only(if (_verbose){
				outputf("        spill %s",_thisfrag->lirbuf->names->formatRef(i));
			})
		}
	}

	void Assembler::asm_load64(LInsp ins)
	{
		LIns* base = ins->oprnd1();
		int db = ins->oprnd2()->constval();
		Reservation *resv = getresv(ins);
		int dr = disp(resv);
		NanoAssert(resv->reg == UnknownReg && dr != 0);

		Register rb = findRegFor(base, GpRegs);
		resv->reg = UnknownReg;
		asm_mmq(FP, dr, rb, db);
		freeRsrcOf(ins, false);
	}

	void Assembler::asm_store64(LInsp value, int dr, LInsp base)
	{
		int da = findMemFor(value);
	    Register rb = findRegFor(base, GpRegs);
	    asm_mmq(rb, dr, FP, da);
	}


	void Assembler::asm_quad(LInsp ins)
	{
		Reservation *rR = getresv(ins);
		int d = disp(rR);
		freeRsrcOf(ins, false);
		if (d)
		{
			const int32_t* p = (const int32_t*) (ins-2);
			STi(FP,d+4,p[1]);
			STi(FP,d,p[0]);
		}
	}

	NIns* Assembler::asm_branch(bool branchOnFalse, LInsp cond, NIns* targ)
	{
		NIns* at = 0;
		LOpcode condop = cond->opcode();
		NanoAssert(cond->isCond());
#ifndef NJ_SOFTFLOAT
		if (condop >= LIR_feq && condop <= LIR_fge)
		{
			return asm_jmpcc(branchOnFalse, cond, targ);
		}
#endif
		// produce the branch
		if (branchOnFalse)
		{
			if (condop == LIR_eq)
				JNE(targ);
			else if (condop == LIR_ov)
				JNO(targ);
			else if (condop == LIR_cs)
				JNC(targ);
			else if (condop == LIR_lt)
				JNL(targ);
			else if (condop == LIR_le)
				JNLE(targ);
			else if (condop == LIR_gt)
				JNG(targ);
			else if (condop == LIR_ge)
				JNGE(targ);
			else if (condop == LIR_ult)
				JNB(targ);
			else if (condop == LIR_ule)
				JNBE(targ);
			else if (condop == LIR_ugt)
				JNA(targ);
			else //if (condop == LIR_uge)
				JNAE(targ);
		}
		else // op == LIR_xt
		{
			if (condop == LIR_eq)
				JE(targ);
			else if (condop == LIR_ov)
				JO(targ);
			else if (condop == LIR_cs)
				JC(targ);
			else if (condop == LIR_lt)
				JL(targ);
			else if (condop == LIR_le)
				JLE(targ);
			else if (condop == LIR_gt)
				JG(targ);
			else if (condop == LIR_ge)
				JGE(targ);
			else if (condop == LIR_ult)
				JB(targ);
			else if (condop == LIR_ule)
				JBE(targ);
			else if (condop == LIR_ugt)
				JA(targ);
			else //if (condop == LIR_uge)
				JAE(targ);
		}
		at = _nIns;
		asm_cmp(cond);
		return at;
	}

	void Assembler::asm_cmp(LIns *cond)
	{
        LOpcode condop = cond->opcode();
        
        // LIR_ov and LIR_cs recycle the flags set by arithmetic ops
        if ((condop == LIR_ov) || (condop == LIR_cs))
            return;
        
        LInsp lhs = cond->oprnd1();
		LInsp rhs = cond->oprnd2();
		Reservation *rA, *rB;

		// Not supported yet.
		NanoAssert(!lhs->isQuad() && !rhs->isQuad());

		// ready to issue the compare
		if (rhs->isconst())
		{
			int c = rhs->constval();
			if (c == 0 && cond->isop(LIR_eq)) {
				Register r = findRegFor(lhs, GpRegs);
				TEST(r,r);
			// No 64-bit immediates so fall-back to below
			}
			else if (!rhs->isQuad()) {
				Register r = getBaseReg(lhs, c, GpRegs);
				CMPi(r, c);
			}
		}
		else
		{
			findRegFor2(GpRegs, lhs, rA, rhs, rB);
			Register ra = rA->reg;
			Register rb = rB->reg;
			CMP(ra, rb);
		}
	}

	void Assembler::asm_loop(LInsp ins, NInsList& loopJumps)
	{
		(void)ins;
		JMP_long_placeholder(); // jump to SOT	
		verbose_only( if (_verbose && _outputCache) { _outputCache->removeLast(); outputf("         jmp   SOT"); } );
		
		loopJumps.add(_nIns);

		#ifdef NJ_VERBOSE
		// branching from this frag to ourself.
		if (_frago->core()->config.show_stats)
		LDi(argRegs[1], int((Fragment*)_thisfrag));
		#endif

		assignSavedParams();

		// restore first parameter, the only one we use
		LInsp state = _thisfrag->lirbuf->state;
		findSpecificRegFor(state, argRegs[state->imm8()]); 
	}	

	void Assembler::asm_fcond(LInsp ins)
	{
		// only want certain regs 
		Register r = prepResultReg(ins, AllowableFlagRegs);
		asm_setcc(r, ins);
#ifdef NJ_ARM_VFP
		SETE(r);
#else
		// SETcc only sets low 8 bits, so extend 
		MOVZX8(r,r);
		SETNP(r);
#endif
		asm_fcmp(ins);
	}
				
	void Assembler::asm_cond(LInsp ins)
	{
		// only want certain regs 
		LOpcode op = ins->opcode();			
		Register r = prepResultReg(ins, AllowableFlagRegs);
		// SETcc only sets low 8 bits, so extend 
		MOVZX8(r,r);
		if (op == LIR_eq)
			SETE(r);
		else if (op == LIR_ov)
			SETO(r);
		else if (op == LIR_cs)
			SETC(r);
		else if (op == LIR_lt)
			SETL(r);
		else if (op == LIR_le)
			SETLE(r);
		else if (op == LIR_gt)
			SETG(r);
		else if (op == LIR_ge)
			SETGE(r);
		else if (op == LIR_ult)
			SETB(r);
		else if (op == LIR_ule)
			SETBE(r);
		else if (op == LIR_ugt)
			SETA(r);
		else // if (op == LIR_uge)
			SETAE(r);
		asm_cmp(ins);
	}
	
	void Assembler::asm_arith(LInsp ins)
	{
		LOpcode op = ins->opcode();			
		LInsp lhs = ins->oprnd1();
		LInsp rhs = ins->oprnd2();

		Register rb = UnknownReg;
		RegisterMask allow = GpRegs;
		bool forceReg = (op == LIR_mul || !rhs->isconst());

#ifdef NANOJIT_ARM
		// Arm can't do an immediate op with immediates
		// outside of +/-255 (for AND) r outside of
		// 0..255 for others.
		if (!forceReg)
		{
			if (rhs->isconst() && !isU8(rhs->constval()))
				forceReg = true;
		}
#endif

		if (lhs != rhs && forceReg)
		{
			if ((rb = asm_binop_rhs_reg(ins)) == UnknownReg) {
				rb = findRegFor(rhs, allow);
			}
			allow &= ~rmask(rb);
		}
		else if ((op == LIR_add||op == LIR_addp) && lhs->isop(LIR_alloc) && rhs->isconst()) {
			// add alloc+const, use lea
			Register rr = prepResultReg(ins, allow);
			int d = findMemFor(lhs) + rhs->constval();
			LEA(rr, d, FP);
		}

		Register rr = prepResultReg(ins, allow);
		Reservation* rA = getresv(lhs);
		Register ra;
		// if this is last use of lhs in reg, we can re-use result reg
		if (rA == 0 || (ra = rA->reg) == UnknownReg)
			ra = findSpecificRegFor(lhs, rr);
		// else, rA already has a register assigned.

		if (forceReg)
		{
			if (lhs == rhs)
				rb = ra;

			if (op == LIR_add || op == LIR_addp)
				ADD(rr, rb);
			else if (op == LIR_sub)
				SUB(rr, rb);
			else if (op == LIR_mul)
				MUL(rr, rb);
			else if (op == LIR_and)
				AND(rr, rb);
			else if (op == LIR_or)
				OR(rr, rb);
			else if (op == LIR_xor)
				XOR(rr, rb);
			else if (op == LIR_lsh)
				SHL(rr, rb);
			else if (op == LIR_rsh)
				SAR(rr, rb);
			else if (op == LIR_ush)
				SHR(rr, rb);
			else
				NanoAssertMsg(0, "Unsupported");
		}
		else
		{
			int c = rhs->constval();
			if (op == LIR_add || op == LIR_addp) {
				{
					ADDi(rr, c); 
				}
			} else if (op == LIR_sub) {
				{
					SUBi(rr, c); 
				}
			} else if (op == LIR_and)
				ANDi(rr, c);
			else if (op == LIR_or)
				ORi(rr, c);
			else if (op == LIR_xor)
				XORi(rr, c);
			else if (op == LIR_lsh)
				SHLi(rr, c);
			else if (op == LIR_rsh)
				SARi(rr, c);
			else if (op == LIR_ush)
				SHRi(rr, c);
			else
				NanoAssertMsg(0, "Unsupported");
		}

		if ( rr != ra ) 
			MR(rr,ra);
	}
	
	void Assembler::asm_neg_not(LInsp ins)
	{
		LOpcode op = ins->opcode();			
		Register rr = prepResultReg(ins, GpRegs);

		LIns* lhs = ins->oprnd1();
		Reservation *rA = getresv(lhs);
		// if this is last use of lhs in reg, we can re-use result reg
		Register ra;
		if (rA == 0 || (ra=rA->reg) == UnknownReg)
			ra = findSpecificRegFor(lhs, rr);
		// else, rA already has a register assigned.

		if (op == LIR_not)
			NOT(rr); 
		else
			NEG(rr); 

		if ( rr != ra ) 
			MR(rr,ra); 
	}
				
	void Assembler::asm_ld(LInsp ins)
	{
		LOpcode op = ins->opcode();			
		LIns* base = ins->oprnd1();
		LIns* disp = ins->oprnd2();
		Register rr = prepResultReg(ins, GpRegs);
		int d = disp->constval();
		Register ra = getBaseReg(base, d, GpRegs);
		if (op == LIR_ldcb)
			LD8Z(rr, d, ra);
		else
			LD(rr, d, ra); 
	}

	void Assembler::asm_cmov(LInsp ins)
	{
		LOpcode op = ins->opcode();			
		LIns* condval = ins->oprnd1();
		NanoAssert(condval->isCmp());

		LIns* values = ins->oprnd2();

		NanoAssert(values->opcode() == LIR_2);
		LIns* iftrue = values->oprnd1();
		LIns* iffalse = values->oprnd2();

		NanoAssert(op == LIR_qcmov || (!iftrue->isQuad() && !iffalse->isQuad()));
		
		const Register rr = prepResultReg(ins, GpRegs);

		// this code assumes that neither LD nor MR nor MRcc set any of the condition flags.
		// (This is true on Intel, is it true on all architectures?)
		const Register iffalsereg = findRegFor(iffalse, GpRegs & ~rmask(rr));
		if (op == LIR_cmov) {
			switch (condval->opcode())
			{
				// note that these are all opposites...
				case LIR_eq:	MRNE(rr, iffalsereg);	break;
				case LIR_ov:    MRNO(rr, iffalsereg);   break;
				case LIR_cs:    MRNC(rr, iffalsereg);   break;
				case LIR_lt:	MRGE(rr, iffalsereg);	break;
				case LIR_le:	MRG(rr, iffalsereg);	break;
				case LIR_gt:	MRLE(rr, iffalsereg);	break;
				case LIR_ge:	MRL(rr, iffalsereg);	break;
				case LIR_ult:	MRAE(rr, iffalsereg);	break;
				case LIR_ule:	MRA(rr, iffalsereg);	break;
				case LIR_ugt:	MRBE(rr, iffalsereg);	break;
				case LIR_uge:	MRB(rr, iffalsereg);	break;
				debug_only( default: NanoAssert(0); break; )
			}
		} else if (op == LIR_qcmov) {
			NanoAssert(0);
		}
		/*const Register iftruereg =*/ findSpecificRegFor(iftrue, rr);
		asm_cmp(condval);
	}
				
	void Assembler::asm_qhi(LInsp ins)
	{
		Register rr = prepResultReg(ins, GpRegs);
		LIns *q = ins->oprnd1();
		int d = findMemFor(q);
		LD(rr, d+4, FP);
	}

	void Assembler::asm_param(LInsp ins)
	{
		uint32_t a = ins->imm8();
		uint32_t kind = ins->imm8b();
		if (kind == 0) {
			// ordinary param
			AbiKind abi = _thisfrag->lirbuf->abi;
			uint32_t abi_regcount = abi == ABI_FASTCALL ? 2 : abi == ABI_THISCALL ? 1 : 0;
			if (a < abi_regcount) {
				// incoming arg in register
				prepResultReg(ins, rmask(argRegs[a]));
			} else {
				// incoming arg is on stack, and EBP points nearby (see genPrologue)
				Register r = prepResultReg(ins, GpRegs);
				int d = (a - abi_regcount) * sizeof(intptr_t) + 8;
				LD(r, d, FP); 
			}
		}
		else {
			// saved param
			prepResultReg(ins, rmask(savedRegs[a]));
		}
	}

	void Assembler::asm_short(LInsp ins)
	{
		Register rr = prepResultReg(ins, GpRegs);
		int32_t val = ins->imm16();
		if (val == 0)
			XOR(rr,rr);
		else
			LDi(rr, val);
	}

	void Assembler::asm_int(LInsp ins)
	{
		Register rr = prepResultReg(ins, GpRegs);
		int32_t val = ins->imm32();
		if (val == 0)
			XOR(rr,rr);
		else
			LDi(rr, val);
	}

	void Assembler::asm_quad(LInsp ins)
	{
    	Reservation *rR = getresv(ins);
		Register rr = rR->reg;
		if (rr != UnknownReg)
		{
			// @todo -- add special-cases for 0 and 1
			_allocator.retire(rr);
			rR->reg = UnknownReg;
			NanoAssert((rmask(rr) & FpRegs) != 0);

			const double d = ins->constvalf();
            const uint64_t q = ins->constvalq();
			if (rmask(rr) & XmmRegs) {
				if (q == 0.0) {
                    // test (int64)0 since -0.0 == 0.0
					SSE_XORPDr(rr, rr);
				} else if (d == 1.0) {
					// 1.0 is extremely frequent and worth special-casing!
					static const double k_ONE = 1.0;
					LDSDm(rr, &k_ONE);
				} else {
					findMemFor(ins);
					const int d = disp(rR);
					SSE_LDQ(rr, d, FP);
				}
			} else {
				if (q == 0.0) {
                    // test (int64)0 since -0.0 == 0.0
					FLDZ();
				} else if (d == 1.0) {
					FLD1();
				} else {
					findMemFor(ins);
					int d = disp(rR);
					FLDQ(d,FP);
				}
			}
		}

		// @todo, if we used xor, ldsd, fldz, etc above, we don't need mem here
		int d = disp(rR);
		freeRsrcOf(ins, false);
		if (d)
		{
			const int32_t* p = (const int32_t*) (ins-2);
			STi(FP,d+4,p[1]);
			STi(FP,d,p[0]);
		}
	}
	
	void Assembler::asm_qlo(LInsp ins)
	{
		LIns *q = ins->oprnd1();
		Reservation *resv = getresv(ins);
		Register rr = resv->reg;
		if (rr == UnknownReg) {
			// store quad in spill loc
			int d = disp(resv);
			freeRsrcOf(ins, false);
			Register qr = findRegFor(q, XmmRegs);
			SSE_MOVDm(d, FP, qr);
		} else {
			freeRsrcOf(ins, false);
			Register qr = findRegFor(q, XmmRegs);
			SSE_MOVD(rr,qr);
		}
	}

	void Assembler::asm_nongp_copy(Register r, Register s)
	{
		// we will need this for VFP support
		(void)r; (void)s;
		NanoAssert(false);
	}

	Register Assembler::asm_binop_rhs_reg(LInsp ins)
	{
		return UnknownReg;
	}

    /**
     * copy 64 bits: (rd+dd) <- (rs+ds)
     */
    void Assembler::asm_mmq(Register rd, int dd, Register rs, int ds)
    {
        // value is either a 64bit struct or maybe a float
        // that isn't live in an FPU reg.  Either way, don't
        // put it in an FPU reg just to load & store it.
		// get a scratch reg
		Register t = registerAlloc(GpRegs & ~(rmask(rd)|rmask(rs)));
		_allocator.addFree(t);
		ST(rd, dd+4, t);
		LD(t, ds+4, rs);
		ST(rd, dd, t);
		LD(t, ds, rs);
    }

	void Assembler::asm_pusharg(LInsp p)
	{
		// arg goes on stack
		Reservation* rA = getresv(p);
		if (rA == 0)
		{
			Register ra = findRegFor(p, GpRegs);
			ST(SP,0,ra);
		}
		else if (rA->reg == UnknownReg)
		{
			ST(SP,0,Scratch);
			LD(Scratch,disp(rA),FP);
		}
		else
		{
			ST(SP,0,rA->reg);
		}
	}

	void Assembler::nativePageReset()
	{
			_nPool = 0;
			_nSlot = 0;
			_nExitPool = 0;
			_nExitSlot = 0;
	}

	void Assembler::nativePageSetup()
	{
		if (!_nIns)		 _nIns	   = pageAlloc();
		if (!_nExitIns)  _nExitIns = pageAlloc(true);
		//fprintf(stderr, "assemble onto %x exits into %x\n", (int)_nIns, (int)_nExitIns);
	
		if (!_nPool) {
			_nSlot = _nPool = (int*)_nIns;

			// Make original pool at end of page. Currently
			// we are pointing off the end of the original page,
			// so back up 1+NJ_CPOOL_SIZE
			_nPool = (int*)((int)_nIns - (sizeof(int32_t)*NJ_CPOOL_SIZE));
            
			// _nSlot points at last slot in pool (fill upwards)
			_nSlot = _nPool + (NJ_CPOOL_SIZE-1);

			// Move _nIns to the top of the pool
			_nIns = (NIns*)_nPool;

			// no branch needed since this follows the epilogue
        }
	}

	void Assembler::flushCache(NIns* n1, NIns* n2) {
#if defined(UNDER_CE)
 		// we changed the code, so we need to do this (sadly)
		FlushInstructionCache(GetCurrentProcess(), NULL, NULL);
#elif defined(AVMPLUS_LINUX)
		// Just need to clear this one page (not even the whole page really)
		//Page *page = (Page*)pageTop(_nIns);
		register unsigned long _beg __asm("a1") = (unsigned long)(n1);
		register unsigned long _end __asm("a2") = (unsigned long)(n2);
		register unsigned long _flg __asm("a3") = 0;
		register unsigned long _swi __asm("r7") = 0xF0002;
		__asm __volatile ("swi 0 	@ sys_cacheflush" : "=r" (_beg) : "0" (_beg), "r" (_end), "r" (_flg), "r" (_swi));
#endif
	}

	NIns* Assembler::asm_adjustBranch(NIns* at, NIns* target)
	{
		NIns* save = _nIns;
		NIns* was =  (NIns*) (((((*(at+2))&0x7ff)<<12) | (((*(at+1))&0x7ff)<<1)) + (at-2+2));

		_nIns = at + 2;
		BL(target);

		flushCache(_nIns, _nIns+2);

#ifdef AVMPLUS_PORTING_API
		// XXX save.._nIns+2? really?
		NanoJIT_PortAPI_FlushInstructionCache(save, _nIns+2);
#endif
		
		_nIns = save;

		return was;
	}

	void Assembler::STi(Register b, int32_t d, int32_t v)
	{
		ST(b, d, Scratch);
		LDi(Scratch, v);
	}

	bool isB11(NIns *target, NIns *cur)
	{
		NIns *br_base = (cur-1)+2;
		int br_off = int(target) - int(br_base);
		return (-(1<<11) <= br_off && br_off < (1<<11));
	}

	void Assembler::underrunProtect(int bytes)
	{
		NanoAssertMsg(bytes<=LARGEST_UNDERRUN_PROT, "constant LARGEST_UNDERRUN_PROT is too small"); 

		// perhaps bytes + sizeof(PageHeader)/sizeof(NIns) + 4 ?
		intptr_t u = bytes + 4;
		if (!samepage(_nIns-u, _nIns-1)) {
			NIns* target = _nIns;
			_nIns = pageAlloc(_inExit);
			// might be able to do a B instead of BL (save an instruction)
			if (isB11(target, _nIns))
			{
				NIns *br_base = (_nIns-1)+2;
				int br_off = int(target) - int(br_base);
				*(--_nIns) = (NIns)(0xE000 | ((br_off>>1)&0x7FF));
			}
			else
			{
				int offset = int(target)-int(_nIns-2+2);
				*(--_nIns) = (NIns)(0xF800 | ((offset>>1)&0x7FF) );
				*(--_nIns) = (NIns)(0xF000 | ((offset>>12)&0x7FF) );
			}
		}
	}

	bool isB22(NIns *target, NIns *cur)
	{
		int offset = int(target)-int(cur-2+2);
		return (-(1<<22) <= offset && offset < (1<<22));
	}

	void Assembler::BL(NIns* target)
	{
		underrunProtect(4);
		NanoAssert(isB22(target,_nIns));
		int offset = int(target)-int(_nIns-2+2);
		*(--_nIns) = (NIns)(0xF800 | ((offset>>1)&0x7FF) );
		*(--_nIns) = (NIns)(0xF000 | ((offset>>12)&0x7FF) );
		asm_output("bl %X offset=%d",(int)target, offset);
	}


	void Assembler::B(NIns *target)
	{
		underrunProtect(2);
		NanoAssert(isB11(target,_nIns));
		NIns *br_base = (_nIns-1)+2;
		int br_off = int(target) - int(br_base);
		NanoAssert(-(1<<11) <= br_off && br_off < (1<<11));
		*(--_nIns) = (NIns)(0xE000 | ((br_off>>1)&0x7FF));
		asm_output("b %X offset=%d", (int)target, br_off);
	}

	void Assembler::JMP(NIns *target)
	{
		underrunProtect(4);
		if (isB11(target,_nIns))
			B(target);
		else
			BL(target);
	}

	void Assembler::PUSH_mask(RegisterMask mask)
	{
		NanoAssert((mask&(0xff|rmask(LR)))==mask);
		underrunProtect(2);
		if (mask & rmask(LR)) {
			mask &= ~rmask(LR);
			mask |= rmask(R8);
		}
		*(--_nIns) = (NIns)(0xB400 | mask);
		asm_output("push {%x}", mask);
	}

	void Assembler::POPr(Register r)
	{
		underrunProtect(2);
		NanoAssert(((unsigned)r)<8 || r == PC);
		if (r == PC)
			r = R8;
		*(--_nIns) = (NIns)(0xBC00 | (1<<(r)));
		asm_output("pop {%s}",gpn(r));
	}

	void Assembler::POP_mask(RegisterMask mask)
	{
		NanoAssert((mask&(0xff|rmask(PC)))==mask);
		underrunProtect(2);
		if (mask & rmask(PC)) {
			mask &= ~rmask(PC);
			mask |= rmask(R8);
		}
		*(--_nIns) = (NIns)(0xBC00 | mask);
		asm_output("pop {%x}", mask);
	}

	void Assembler::MOVi(Register r, int32_t v)
	{
		NanoAssert(isU8(v));
		underrunProtect(2);
		*(--_nIns) = (NIns)(0x2000 | r<<8 | v);
		asm_output("mov %s,#%d",gpn(r),v);
	}

	void Assembler::LDi(Register r, int32_t v)
	{
		if (isU8(v)) {
			MOVi(r,v);
		} else if (isU8(-v)) {
			NEG(r);
			MOVi(r,-v);
		} else {
			underrunProtect(2);
			LD32_nochk(r, v);
		}
	}

	void Assembler::B_cond(int c, NIns *target)
	{
		#ifdef NJ_VERBOSE
		static const char *ccname[] = { "eq","ne","hs","lo","mi","pl","vs","vc","hi","ls","ge","lt","gt","le","al","nv" };
		#endif

		underrunProtect(6);
		int tt = int(target) - int(_nIns-1+2);
		if (tt < (1<<8) && tt >= -(1<<8))	{
			*(--_nIns) = (NIns)(0xD000 | ((c)<<8) | (tt>>1)&0xFF );
			asm_output("b%s %X offset=%d", ccname[c], target, tt);
		} else {
			NIns *skip = _nIns;
			BL(target);
			c ^= 1;
			*(--_nIns) = (NIns)(0xD000 | c<<8 | 1 );
			asm_output("b%s %X", ccname[c], skip);
		}
	}

	void Assembler::STR_sp(int32_t offset, Register reg)
	{
		NanoAssert((offset&3)==0);// require natural alignment
		int32_t off = offset>>2;
		NanoAssert(isU8(off));
		underrunProtect(2);
		*(--_nIns) = (NIns)(0x9000 | ((reg)<<8) | off );
		asm_output("str %s, %d(%s)", gpn(reg), offset, gpn(SP));
	}

	void Assembler::STR_index(Register base, Register off, Register reg)
	{
		underrunProtect(2);
		*(--_nIns) = (NIns)(0x5000 | (off<<6) | (base<<3) | (reg));
		asm_output("str %s,(%s+%s)",gpn(reg),gpn(base),gpn(off));
	}

	void Assembler::STR_m(Register base, int32_t offset, Register reg)
	{
		NanoAssert(offset >= 0 && offset < 128 && (offset&3)==0);
		underrunProtect(2);
		int32_t off = offset>>2;
		*(--_nIns) = (NIns)(0x6000 | off<<6 | base<<3 | reg);
		asm_output("str %s,%d(%s)", gpn(reg), offset, gpn(base)); 
	}

	void Assembler::LDMIA(Register base, RegisterMask regs)
	{
		underrunProtect(2);
		NanoAssert((regs&rmask(base))==0 && isU8(regs));
		*(--_nIns) = (NIns)(0xC800 | base<<8 | regs);
		asm_output("ldmia %s!,{%x}", gpn(base), regs);
	}

	void Assembler::STMIA(Register base, RegisterMask regs)
	{
		underrunProtect(2);
		NanoAssert((regs&rmask(base))==0 && isU8(regs));
		*(--_nIns) = (NIns)(0xC000 | base<<8 | regs);
		asm_output("stmia %s!,{%x}", gpn(base), regs);
	}

	void Assembler::ST(Register base, int32_t offset, Register reg)
	{
		NanoAssert((offset&3)==0);// require natural alignment
		int off = offset>>2;
		if (base==SP) {
			STR_sp(offset, reg);
		} else if ((offset)<0) {										
			STR_index(base, Scratch, reg);
			NEG(Scratch);
			if (offset < -255) {
				NanoAssert(offset >= -1020);
				SHLi(Scratch, 2);
				MOVi(Scratch, -off);
			}
			else {
				MOVi(Scratch, -offset);
			}
		} else {
			underrunProtect(6);
			if (off<32) {
				STR_m(base, offset, reg);
			}
			else {
				STR_index(base, Scratch, reg);
				if (offset > 255) {
					SHLi(Scratch, 2);
					MOVi(Scratch, off);
				}
				else {
					MOVi(Scratch, offset);
				}
			}
		}
	}

	void Assembler::ADDi8(Register r, int32_t i)
	{
		underrunProtect(2);
		NanoAssert(isU8(i));
		*(--_nIns) = (NIns)(0x3000 | r<<8 | i);
		asm_output("add %s,#%d", gpn(r), i);
	}

	void Assembler::ADDi(Register r, int32_t i)
	{
		if (i < 0 && i != 0x80000000) {
			SUBi(r, -i);
		}
		else if (r == SP) {
			NanoAssert((i&3)==0 && i >= 0 && i < (1<<9));
			underrunProtect(2);
			*(--_nIns) = (NIns)(0xB000 | i>>2);
			asm_output("add %s,#%d", gpn(SP), i);
		}
		else if (isU8(i)) {
			ADDi8(r,i);
		}
		else if (i >= 0 && i <= (255+255)) {
			ADDi8(r,i-255);
			ADDi8(r,255);
		}
		else {
			ADD(r, Scratch);
			LDi(Scratch, i);
		}
	}

	void Assembler::SUBi8(Register r, int32_t i)
	{
		underrunProtect(2);
		NanoAssert(isU8(i));
		*(--_nIns) = (NIns)(0x3800 | r<<8 | i);
		asm_output("sub %s,#%d", gpn(r), i);
	}

	void Assembler::SUBi(Register r, int32_t i)
	{
		if (i < 0 && i != 0x80000000) {
			ADDi(r, -i);
		}
		else if (r == SP) {
			NanoAssert((i&3)==0 && i >= 0 && i < (1<<9));
			underrunProtect(2);
			*(--_nIns) = (NIns)(0xB080 | i>>2);
			asm_output("sub %s,#%d", gpn(SP), i);
		}
		else if (isU8(i)) {
			SUBi8(r,i);
		}
		else if (i >= 0 && i <= (255+255)) {
			SUBi8(r,i-255);
			SUBi8(r,255);
		}
		else {
			SUB(r, Scratch);
			LDi(Scratch, i);
		}
	}

	void Assembler::CALL(const CallInfo *ci)
	{
        intptr_t addr = ci->_address;
		if (isB22((NIns*)addr, _nIns)) {
			int offset = int(addr)-int(_nIns-2+2);
			*(--_nIns) = (NIns)(0xF800 | ((offset>>1)&0x7FF) );
			*(--_nIns) = (NIns)(0xF000 | ((offset>>12)&0x7FF) );
			asm_output("call %08X:%s", addr, ci->_name);
		}
		else
		{
			underrunProtect(2*(10));
		
			if ( (((int(_nIns))&0xFFFF)%4) != 0)
				 *(--_nIns) = (NIns)0;

			*(--_nIns) = (NIns)(0xF800 | (((-14)&0xFFF)>>1) );
			*(--_nIns) = (NIns)(0xF000 | (((-14)>>12)&0x7FF) );

			*(--_nIns) = (NIns)(0x4600 | (1<<7) | (Scratch<<3) | (IP&7));
			*(--_nIns) = (NIns)0;
			*(--_nIns) = (short)((addr) >> 16);
			*(--_nIns) = (short)((addr) & 0xFFFF);
			*(--_nIns) = (NIns)(0x4700 | (IP<<3));
			*(--_nIns) = (NIns)(0xE000 | (4>>1));
			*(--_nIns) = (NIns)(0x4800 | (Scratch<<8) | (1));
			asm_output("call %08X:%s", addr, ci->_name);
		}
	}

	void Assembler::LD32_nochk(Register r, int32_t imm)
	{
		// Can we reach the current slot/pool?
		int offset = (int)(_nSlot) - (int)(_nIns);
		if ((offset>=NJ_MAX_CPOOL_OFFSET || offset<0) ||
			(_nSlot < _nPool))
		{
			// cant reach, or no room
			// need a new pool
			
			// Make sure we have space for a pool and the LDR
			underrunProtect(sizeof(int32_t)*NJ_CPOOL_SIZE+1);

			NIns* skip = _nIns;

			_nPool = (int*)(((int)_nIns - (sizeof(int32_t)*NJ_CPOOL_SIZE)) &~3);
			_nSlot = _nPool + (NJ_CPOOL_SIZE-1);
			_nIns = (NIns*)_nPool;

			// jump over the pool
			B(skip);
			//*(--_nIns) = (NIns)( COND_AL | (0x5<<25) | (NJ_CPOOL_SIZE-1) );
		}

		*(_nSlot--) = (int)imm;

		NIns *data = (NIns*)(_nSlot+1);;

		int data_off = int(data) - (int(_nIns+1)&~3);
		*(--_nIns) = (NIns)(0x4800 | r<<8 | data_off>>2);
		asm_output("ldr %s,%d(PC) [%X]",gpn(r),data_off,(int)data);
	}
    #endif /* FEATURE_NANOJIT */
}
