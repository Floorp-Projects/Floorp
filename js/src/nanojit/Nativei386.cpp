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

#ifdef _MAC
// for MakeDataExecutable
#include <CoreServices/CoreServices.h>
#endif

#include "nanojit.h"

namespace nanojit
{
	#ifdef FEATURE_NANOJIT

	#ifdef NJ_VERBOSE
		const char *regNames[] = {
			"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
			"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
			"f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7"
		};
	#endif

    const Register Assembler::argRegs[] = { ECX, EDX };
    const Register Assembler::retRegs[] = { EAX, EDX };

	void Assembler::nInit(AvmCore* core)
	{
        sse2 = core->use_sse2();
		// CMOVcc is actually available on most PPro+ chips (except for a few
		// oddballs like Via C3) but for now tie to SSE2 detection
		has_cmov = sse2;
        OSDep::getDate();
	}

	NIns* Assembler::genPrologue(RegisterMask needSaving)
	{
		/**
		 * Prologue
		 */
		uint32_t stackNeeded = 4 * _activation.highwatermark;
		uint32_t savingCount = 0;

		for(Register i=FirstReg; i <= LastReg; i = nextreg(i))
			if (needSaving&rmask(i)) 
				savingCount++;

		// so for alignment purposes we've pushed  return addr, fp, and savingCount registers
		uint32_t stackPushed = 4 * (3+savingCount);
		uint32_t aligned = alignUp(stackNeeded + stackPushed, NJ_ALIGN_STACK);
		uint32_t amt = aligned - stackPushed;

		if (amt) 
			SUBi(SP, amt);

		verbose_only( verbose_outputf("        %p:",_nIns); )
		verbose_only( verbose_output("        patch entry:"); )
        NIns *patchEntry = _nIns;
		MR(FP, SP);
		PUSHr(FP); // push ebp twice to align frame on 8bytes
		PUSHr(FP);

		for(Register i=FirstReg; i <= LastReg; i = nextreg(i))
			if (needSaving&rmask(i))
				PUSHr(i);

        // As of late 2006, Linux GCC aligns the stack to a 16-byte
        // boundary in 'main', and keeps it that way, so there's no
        // need to align it explicitly:
        // http://gcc.gnu.org/ml/gcc-patches/2006-09/msg00298.html
        // The same is true on Darwin:
        // http://gcc.gnu.org/ml/gcc-patches/2007-06/msg00505.html
        #if ! defined(DARWIN) && ! defined(__GNUC__)
		// dynamically align the stack
		PUSHr(FP);//fake returnaddr.
		ANDi(SP, -NJ_ALIGN_STACK);
		MR(FP,SP);
		PUSHr(FP);
		#endif

		return patchEntry;
	}

	GuardRecord * Assembler::nFragExit(LInsp guard)
	{
		SideExit *exit = guard->exit();
		bool trees = _frago->core()->config.tree_opt;
        Fragment *frag = exit->target;
        GuardRecord *lr = 0;
		bool destKnown = (frag && frag->fragEntry);
		if (destKnown && !trees)
		{
			// already exists, emit jump now.  no patching required.
			JMP(frag->fragEntry);
            lr = 0;
		}
		else
		{
			// target doesn't exit yet.  emit jump to epilog, and set up to patch later.
			lr = placeGuardRecord(guard);
            JMP_long(_epilogue);
			lr->jmp = _nIns;
#if 0			
			// @todo optimization ; is it worth it? It means we can remove the loop over outbound in Fragment.link() 
			// for trees we need the patch entry on the incoming fragment so we can unhook it later if needed
			if (tress && destKnown)
				patch(lr);
#endif
		}
		// first restore ESP from EBP, undoing SUBi(SP,amt) from genPrologue
        MR(SP,FP);


        #ifdef NJ_VERBOSE
        if (_frago->core()->config.show_stats) {
			// load EDX (arg1) with Fragment *fromFrag, target fragment
			// will make use of this when calling fragenter().
            int fromfrag = int((Fragment*)_thisfrag);
            LDi(argRegs[1], fromfrag);
        }
        #endif

		// return value is GuardRecord*
        LDi(EAX, int(lr));

		// if/when we patch this exit to jump over to another fragment,
		// that fragment will need its parameters set up just like ours.
        LInsp param0 = _thisfrag->param0;
		Register state = findSpecificRegFor(param0, Register(param0->imm8()));

        // update InterpState
        
        if (exit->rp_adj)
            ADDmi((int32_t)offsetof(avmplus::InterpState, rp), state, exit->rp_adj);

        if (exit->sp_adj)
            ADDmi((int32_t)offsetof(avmplus::InterpState, sp), state, exit->sp_adj);

        if (exit->ip_adj)
			ADDmi((int32_t)offsetof(avmplus::InterpState, ip), state, exit->ip_adj);

        if (exit->f_adj)
            ADDmi((int32_t)offsetof(avmplus::InterpState, f), state, exit->f_adj);

        return lr;
	}

    NIns *Assembler::genEpilogue(RegisterMask restore)
    {
        RET();

		#ifndef DARWIN
		// undo dynamic alignment
		POP(FP);
		MR(SP,FP);
		#endif

		for (Register i=UnknownReg; i >= FirstReg; i = prevreg(i))
			if (restore&rmask(i)) { POP(i); } 

		POP(FP);
		POP(FP);
        return  _nIns;
    }
	
	void Assembler::nArgEmitted(const CallInfo* call, uint32_t stackSlotCount, uint32_t iargs, uint32_t fargs)
	{
		// see if we have finished emitting all args.  If so then make sure the 
		// new stack pointer is NJ_ALIGN_STACK aligned
		const uint32_t istack = call->count_iargs();
		const uint32_t fstack = call->count_args() - istack;
		//printf("call %s iargs %d fargs %d istack %d fstack %d\n",call->_name,iargs,fargs,istack,fstack);
		AvmAssert(iargs <= istack);
		AvmAssert(fargs <= fstack);
		if (iargs == istack && fargs == fstack)
		{
			const int32_t size = 4*stackSlotCount;
			const int32_t extra = alignUp(size, NJ_ALIGN_STACK) - size; 
			if (extra > 0)
				SUBi(SP, extra);
		}
	}
	
	void Assembler::nPostCallCleanup(const CallInfo* call)
	{
		// must be signed, not unsigned
		int32_t istack = call->count_iargs();
		int32_t fstack = call->count_args() - istack;

		istack -= 2;  // first 2 4B args are in registers
		if (istack <= 0)
		{
			istack = 0;
			if (fstack == 0)
				return;  // only using ECX/EDX nothing passed on the stack so no cleanup needed
		}

		const int32_t size = 4*istack + 8*fstack; // actual stack space used
		NanoAssert( size > 0 );
		
		const int32_t extra = alignUp(size, NJ_ALIGN_STACK) - (size); 

		// stack re-alignment 
		// only pop our adjustment amount since callee pops args in FASTCALL mode
		if (extra > 0)
			{ ADDi(SP, extra); }
	}
	
	void Assembler::nMarkExecute(Page* page, int32_t count, bool enable)
	{
		#ifdef _MAC
			MakeDataExecutable(page, count*NJ_PAGE_SIZE);
		#else
			(void)page;
			(void)count;
		#endif
			(void)enable;
	}
			
	Register Assembler::nRegisterAllocFromSet(int set)
	{
		Register r;
		RegAlloc &regs = _allocator;
	#ifdef WIN32
		_asm
		{
			mov ecx, regs
			bsf eax, set					// i = first bit set
			btr RegAlloc::free[ecx], eax	// free &= ~rmask(i)
			mov r, eax
		}
	#else
		asm(
			"bsf	%1, %%eax\n\t"
			"btr	%%eax, %2\n\t"
			"movl	%%eax, %0\n\t"
			: "=m"(r) : "m"(set), "m"(regs.free) : "%eax", "memory" );
	#endif /* WIN32 */
		return r;
	}

	void Assembler::nRegisterResetAll(RegAlloc& a)
	{
		// add scratch registers to our free list for the allocator
		a.clear();
		a.used = 0;
		a.free = SavedRegs | ScratchRegs;
        if (!sse2)
            a.free &= ~XmmRegs;
		debug_only( a.managed = a.free; )
	}

	void Assembler::nPatchBranch(NIns* branch, NIns* location)
	{
		uint32_t offset = location - branch;
		if (branch[0] == JMPc)
			*(uint32_t*)&branch[1] = offset - 5;
		else
			*(uint32_t*)&branch[2] = offset - 6;
	}

	RegisterMask Assembler::hint(LIns* i, RegisterMask allow)
	{
		uint32_t op = i->opcode();
		int prefer = allow;
		if (op == LIR_call)
			prefer &= rmask(EAX);
		else if (op == LIR_param)
			prefer &= rmask(Register(i->imm8()));
        else if (op == LIR_callh || op == LIR_rsh && i->oprnd1()->opcode()==LIR_callh)
            prefer &= rmask(EDX);
		else if (i->isCmp())
			prefer &= AllowableFlagRegs;
        else if (i->isconst())
            prefer &= ScratchRegs;
		return (_allocator.free & prefer) ? prefer : allow;
	}

    void Assembler::asm_qjoin(LIns *ins)
    {
		int d = findMemFor(ins);
		AvmAssert(d);
		LIns* lo = ins->oprnd1();
		LIns* hi = ins->oprnd2();

        Reservation *resv = getresv(ins);
        Register rr = resv->reg;

        if (rr != UnknownReg && (rmask(rr) & FpRegs))
            evict(rr);

        if (hi->isconst())
		{
			STi(FP, d+4, hi->constval());
		}
		else
		{
			Register r = findRegFor(hi, GpRegs);
			ST(FP, d+4, r);
		}

        if (lo->isconst())
		{
			STi(FP, d, lo->constval());
		}
		else
		{
			// okay if r gets recycled.
			Register r = findRegFor(lo, GpRegs);
			ST(FP, d, r);
		}

        freeRsrcOf(ins, false);	// if we had a reg in use, emit a ST to flush it to mem
    }

	void Assembler::asm_restore(LInsp i, Reservation *resv, Register r)
	{
        if (i->isconst())
        {
            if (!resv->arIndex) {
                reserveFree(i);
            }
            LDi(r, i->constval());
        }
        else
        {
            int d = findMemFor(i);
            if (rmask(r) & FpRegs)
		    {
                if (rmask(r) & XmmRegs) {
                    LDQ(r, d, FP);
                } else {
			        FLDQ(d, FP); 
                }
            }
            else
		    {
			    LD(r, d, FP);
		    }
			verbose_only(if (_verbose) {
				outputf("        restore %s", _thisfrag->lirbuf->names->formatRef(i));
			})
        }
	}

    void Assembler::asm_store32(LIns *value, int dr, LIns *base)
    {
        if (value->isconst())
        {
			Register rb = findRegFor(base, GpRegs);
            int c = value->constval();
			STi(rb, dr, c);
        }
        else
        {
		    // make sure what is in a register
		    Reservation *rA, *rB;
		    findRegFor2(GpRegs, value, rA, base, rB);
		    Register ra = rA->reg;
		    Register rb = rB->reg;
		    ST(rb, dr, ra);
        }
    }

	void Assembler::asm_spill(LInsp i, Reservation *resv, bool pop)
	{
		(void)i;
		int d = disp(resv);
		Register rr = resv->reg;
		if (d)
		{
			// save to spill location
            if (rmask(rr) & FpRegs)
			{
                if (rmask(rr) & XmmRegs) {
                    STQ(d, FP, rr);
                } else {
					FSTQ((pop?1:0), d, FP);
                }
			}
			else
			{
				ST(FP, d, rr);
			}
			verbose_only(if (_verbose) {
				outputf("        spill %s",_thisfrag->lirbuf->names->formatRef(i));
			})
		}
		else if (pop && (rmask(rr) & x87Regs))
		{
			// pop the fpu result since it isn't used
			FSTP(FST0);
		}
	}

	void Assembler::asm_load64(LInsp ins)
	{
		LIns* base = ins->oprnd1();
		int db = ins->oprnd2()->constval();
		Reservation *resv = getresv(ins);
		int dr = disp(resv);
		Register rr = resv->reg;

		if (rr != UnknownReg && rmask(rr) & XmmRegs)
		{
			freeRsrcOf(ins, false);
			Register rb = findRegFor(base, GpRegs);
			LDQ(rr, db, rb);
		}
		else
		{
			Register rb = findRegFor(base, GpRegs);
			resv->reg = UnknownReg;

			// don't use an fpu reg to simply load & store the value.
			if (dr)
				asm_mmq(FP, dr, rb, db);

			freeRsrcOf(ins, false);

			if (rr != UnknownReg)
			{
				NanoAssert(rmask(rr)&FpRegs);
				_allocator.retire(rr);
				FLDQ(db, rb);
			}
		}
	}

	void Assembler::asm_store64(LInsp value, int dr, LInsp base)
	{
		if (value->isconstq())
		{
			// if a constant 64-bit value just store it now rather than
			// generating a pointless store/load/store sequence
			Register rb = findRegFor(base, GpRegs);
			const int32_t* p = (const int32_t*) (value-2);
			STi(rb, dr+4, p[1]);
			STi(rb, dr, p[0]);
            return;
		}

        if (value->isop(LIR_ldq) || value->isop(LIR_qjoin))
		{
			// value is 64bit struct or int64_t, or maybe a double.
			// it may be live in an FPU reg.  Either way, don't
			// put it in an FPU reg just to load & store it.

			// a) if we know it's not a double, this is right.
			// b) if we guarded that its a double, this store could be on
			// the side exit, copying a non-double.
			// c) maybe its a double just being stored.  oh well.

			if (sse2) {
                Register rv = findRegFor(value, XmmRegs);
                Register rb = findRegFor(base, GpRegs);
                STQ(dr, rb, rv);
				return;
            }

			int da = findMemFor(value);
		    Register rb = findRegFor(base, GpRegs);
		    asm_mmq(rb, dr, FP, da);
            return;
		}

		Reservation* rA = getresv(value);
		int pop = !rA || rA->reg==UnknownReg;
		Register rv = findRegFor(value, FpRegs);
		Register rb = findRegFor(base, GpRegs);

		if (rmask(rv) & XmmRegs) {
            STQ(dr, rb, rv);
		} else {
			FSTQ(pop, dr, rb);
		}
	}

    /**
     * copy 64 bits: (rd+dd) <- (rs+ds)
     */
    void Assembler::asm_mmq(Register rd, int dd, Register rs, int ds)
    {
        // value is either a 64bit struct or maybe a float
        // that isn't live in an FPU reg.  Either way, don't
        // put it in an FPU reg just to load & store it.
        if (sse2)
        {
            // use SSE to load+store 64bits
            Register t = registerAlloc(XmmRegs);
            _allocator.addFree(t);
            STQ(dd, rd, t);
            LDQ(t, ds, rs);
        }
        else
        {
            // get a scratch reg
            Register t = registerAlloc(GpRegs & ~(rmask(rd)|rmask(rs)));
            _allocator.addFree(t);
            ST(rd, dd+4, t);
            LD(t, ds+4, rs);
            ST(rd, dd, t);
            LD(t, ds, rs);
        }
    }

	void Assembler::asm_pusharg(LInsp p)
	{
		// arg goes on stack
		Reservation* rA = getresv(p);
		if (rA == 0)
		{
			if (p->isconst())
			{
				// small const we push directly
				PUSHi(p->constval());
			}
			else
			{
				Register ra = findRegFor(p, GpRegs);
				PUSHr(ra);
			}
		}
		else if (rA->reg == UnknownReg)
		{
			PUSHm(disp(rA), FP);
		}
		else
		{
			PUSHr(rA->reg);
		}
	}
	
	NIns* Assembler::asm_adjustBranch(NIns* at, NIns* target)
	{
		NIns* save = _nIns;
		NIns* was = (NIns*)( (intptr_t)*(int32_t*)(at+1)+(intptr_t)(at+5) );
		_nIns = at +5; // +5 is size of JMP
		intptr_t tt = (intptr_t)target - (intptr_t)_nIns;
		IMM32(tt);
		*(--_nIns) = JMPc;
		_nIns = save;
		return was;
	}
	
	void Assembler::nativePageReset()	{}

	void Assembler::nativePageSetup()
	{
		if (!_nIns)		 _nIns	   = pageAlloc();
		if (!_nExitIns)  _nExitIns = pageAlloc(true);
	}
	#endif /* FEATURE_NANOJIT */
}
