/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
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
 *   Mozilla TraceMonkey Team
 *   Asko Tontti <atontti@cc.hut.fi>
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

#ifdef _MSC_VER
    // disable some specific warnings which are normally useful, but pervasive in the code-gen macros
    #pragma warning(disable:4310) // cast truncates constant value
#endif

namespace nanojit
{
    #if defined FEATURE_NANOJIT && defined NANOJIT_IA32

    #ifdef NJ_VERBOSE
        const char *regNames[] = {
            "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
            "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
            "f0"
        };
    #endif

    #define TODO(x) do{ verbose_only(outputf(#x);) NanoAssertMsgf(false, "%s", #x); } while(0)

    const Register Assembler::argRegs[] = { ECX, EDX };
    const Register Assembler::retRegs[] = { EAX, EDX };
    const Register Assembler::savedRegs[] = { EBX, ESI, EDI };

    const static uint8_t max_abi_regs[] = {
        2, /* ABI_FASTCALL */
        1, /* ABI_THISCALL */
        0, /* ABI_STDCALL */
        0  /* ABI_CDECL */
    };


    void Assembler::nInit(AvmCore* core)
    {
        (void) core;
        VMPI_getDate();
    }

    void Assembler::nBeginAssembly() {
        max_stk_args = 0;
    }

    NIns* Assembler::genPrologue()
    {
        /**
         * Prologue
         */
        uint32_t stackNeeded = max_stk_args + STACK_GRANULARITY * _activation.tos;

        uint32_t stackPushed =
            STACK_GRANULARITY + // returnaddr
            STACK_GRANULARITY; // ebp

        uint32_t aligned = alignUp(stackNeeded + stackPushed, NJ_ALIGN_STACK);
        uint32_t amt = aligned - stackPushed;

        // Reserve stackNeeded bytes, padded
        // to preserve NJ_ALIGN_STACK-byte alignment.
        if (amt)
        {
            SUBi(SP, amt);
        }

        verbose_only( outputAddr=true; asm_output("[frag entry]"); )
        NIns *fragEntry = _nIns;
        MR(FP, SP); // Establish our own FP.
        PUSHr(FP); // Save caller's FP.

        return fragEntry;
    }

    void Assembler::nFragExit(LInsp guard)
    {
        SideExit *exit = guard->record()->exit;
        bool trees = config.tree_opt;
        Fragment *frag = exit->target;
        GuardRecord *lr = 0;
        bool destKnown = (frag && frag->fragEntry);

        // Generate jump to epilog and initialize lr.
        // If the guard is LIR_xtbl, use a jump table with epilog in every entry
        if (guard->isop(LIR_xtbl)) {
            lr = guard->record();
            Register r = EDX;
            SwitchInfo* si = guard->record()->exit->switchInfo;
            if (!_epilogue)
                _epilogue = genEpilogue();
            emitJumpTable(si, _epilogue);
            JMP_indirect(r);
            LEAmi4(r, si->table, r);
        } else {
            // If the guard already exists, use a simple jump.
            if (destKnown && !trees) {
                JMP(frag->fragEntry);
                lr = 0;
            } else {  // Target doesn't exist. Jump to an epilogue for now. This can be patched later.
                if (!_epilogue)
                    _epilogue = genEpilogue();
                lr = guard->record();
                JMP_long(_epilogue);
                lr->jmp = _nIns;
            }
        }

        // profiling for the exit
        verbose_only(
           if (_logc->lcbits & LC_FragProfile) {
              INCLi( &guard->record()->profCount );
           }
        )

        // Restore ESP from EBP, undoing SUBi(SP,amt) in the prologue
        MR(SP,FP);

        // return value is GuardRecord*
        LDi(EAX, int(lr));
    }

    NIns *Assembler::genEpilogue()
    {
        RET();
        POPr(FP); // Restore caller's FP.

        return  _nIns;
    }

    void Assembler::asm_call(LInsp ins)
    {
        const CallInfo* call = ins->callInfo();
        // must be signed, not unsigned
        uint32_t iargs = call->count_iargs();
        int32_t fargs = call->count_args() - iargs;

        bool indirect = call->isIndirect();
        if (indirect) {
            // target arg isn't pushed, its consumed in the call
            iargs --;
        }

        AbiKind abi = call->_abi;
        uint32_t max_regs = max_abi_regs[abi];
        if (max_regs > iargs)
            max_regs = iargs;

        int32_t istack = iargs-max_regs;  // first 2 4B args are in registers
        int32_t extra = 0;
        const int32_t pushsize = 4*istack + 8*fargs; // actual stack space used

#if _MSC_VER
        // msc only provides 4-byte alignment, anything more than 4 on windows
        // x86-32 requires dynamic ESP alignment in prolog/epilog and static
        // esp-alignment here.
        uint32_t align = 4;//NJ_ALIGN_STACK;
#else
        uint32_t align = NJ_ALIGN_STACK;
#endif

        if (pushsize) {
            if (config.fixed_esp) {
                // In case of fastcall, stdcall and thiscall the callee cleans up the stack,
                // and since we reserve max_stk_args words in the prolog to call functions
                // and don't adjust the stack pointer individually for each call we have
                // to undo here any changes the callee just did to the stack.
                if (abi != ABI_CDECL)
                    SUBi(SP, pushsize);
            } else {
                // stack re-alignment
                // only pop our adjustment amount since callee pops args in FASTCALL mode
                extra = alignUp(pushsize, align) - pushsize;
                if (call->_abi == ABI_CDECL) {
                    // with CDECL only, caller pops args
                    ADDi(SP, extra+pushsize);
                } else if (extra > 0) {
                    ADDi(SP, extra);
                }
            }
        }

        NanoAssert(ins->isop(LIR_pcall) || ins->isop(LIR_fcall));
        if (!indirect) {
            CALL(call);
        }
        else {
            // indirect call.  x86 Calling conventions don't use EAX as an
            // argument, and do use EAX as a return value.  We need a register
            // for the address to call, so we use EAX since it will always be
            // available
            CALLr(call, EAX);
        }

        // make sure fpu stack is empty before call (restoreCallerSaved)
        NanoAssert(_allocator.isFree(FST0));
        // note: this code requires that ref arguments (ARGSIZE_Q)
        // be one of the first two arguments
        // pre-assign registers to the first N 4B args based on the calling convention
        uint32_t n = 0;

        ArgSize sizes[MAXARGS];
        uint32_t argc = call->get_sizes(sizes);
        int32_t stkd = 0;
        
        if (indirect) {
            argc--;
            asm_arg(ARGSIZE_P, ins->arg(argc), EAX, stkd);
            if (!config.fixed_esp) 
                stkd = 0;
        }

        for(uint32_t i=0; i < argc; i++)
        {
            uint32_t j = argc-i-1;
            ArgSize sz = sizes[j];
            Register r = UnknownReg;
            if (n < max_regs && sz != ARGSIZE_F) {
                r = argRegs[n++]; // tell asm_arg what reg to use
            }
            asm_arg(sz, ins->arg(j), r, stkd);
            if (!config.fixed_esp) 
                stkd = 0;
        }

        if (config.fixed_esp) {
            if (pushsize > max_stk_args)
                max_stk_args = pushsize;
        } else if (extra > 0) {
            SUBi(SP, extra);
        }
    }

    Register Assembler::nRegisterAllocFromSet(RegisterMask set)
    {
        Register r;
        RegAlloc &regs = _allocator;
    #ifdef WIN32
        _asm
        {
            mov ecx, regs
            bsf eax, set                    // i = first bit set
            btr RegAlloc::free[ecx], eax    // free &= ~rmask(i)
            mov r, eax
        }
    #else
        asm(
            "bsf    %1, %%eax\n\t"
            "btr    %%eax, %2\n\t"
            "movl   %%eax, %0\n\t"
            : "=m"(r) : "m"(set), "m"(regs.free) : "%eax", "memory" );
    #endif /* WIN32 */
        return r;
    }

    void Assembler::nRegisterResetAll(RegAlloc& a)
    {
        // add scratch registers to our free list for the allocator
        a.clear();
        a.free = SavedRegs | ScratchRegs;
        if (!config.sse2)
            a.free &= ~XmmRegs;
        debug_only( a.managed = a.free; )
    }

    void Assembler::nPatchBranch(NIns* branch, NIns* targ)
    {
        intptr_t offset = intptr_t(targ) - intptr_t(branch);
        if (branch[0] == JMP32) {
            *(int32_t*)&branch[1] = offset - 5;
        } else if (branch[0] == JCC32) {
            *(int32_t*)&branch[2] = offset - 6;
        } else
            NanoAssertMsg(0, "Unknown branch type in nPatchBranch");
    }

    RegisterMask Assembler::hint(LIns* i, RegisterMask allow)
    {
        uint32_t op = i->opcode();
        int prefer = allow;
        if (op == LIR_icall) {
            prefer &= rmask(retRegs[0]);
        }
        else if (op == LIR_fcall) {
            prefer &= rmask(FST0);
        }
        else if (op == LIR_param) {
            if (i->paramKind() == 0) {
                uint32_t max_regs = max_abi_regs[_thisfrag->lirbuf->abi];
                if (i->paramArg() < max_regs)
                    prefer &= rmask(argRegs[i->paramArg()]);
            } else {
                if (i->paramArg() < NumSavedRegs)
                    prefer &= rmask(savedRegs[i->paramArg()]);
            }
        }
        else if (op == LIR_callh || (op == LIR_rsh && i->oprnd1()->opcode()==LIR_callh)) {
            prefer &= rmask(retRegs[1]);
        }
        else if (i->isCmp()) {
            prefer &= AllowableFlagRegs;
        }
        else if (i->isconst()) {
            prefer &= ScratchRegs;
        }
        return (_allocator.free & prefer) ? prefer : allow;
    }

    void Assembler::asm_qjoin(LIns *ins)
    {
        int d = findMemFor(ins);
        AvmAssert(d);
        LIns* lo = ins->oprnd1();
        LIns* hi = ins->oprnd2();

        Register rr = ins->getReg();
        if (isKnownReg(rr) && (rmask(rr) & FpRegs))
            evict(rr, ins);

        if (hi->isconst())
        {
            STi(FP, d+4, hi->imm32());
        }
        else
        {
            Register r = findRegFor(hi, GpRegs);
            ST(FP, d+4, r);
        }

        if (lo->isconst())
        {
            STi(FP, d, lo->imm32());
        }
        else
        {
            // okay if r gets recycled.
            Register r = findRegFor(lo, GpRegs);
            ST(FP, d, r);
        }

        freeRsrcOf(ins, false); // if we had a reg in use, emit a ST to flush it to mem
    }

    void Assembler::asm_load(int d, Register r)
    {
        if (rmask(r) & FpRegs)
        {
            if (rmask(r) & XmmRegs) {
                SSE_LDQ(r, d, FP);
            } else {
                FLDQ(d, FP);
            }
        }
        else
        {
            LD(r, d, FP);
        }
    }

    // WARNING: the code generated by this function must not affect the
    // condition codes.  See asm_cmp().
    void Assembler::asm_restore(LInsp i, Register r)
    {
        uint32_t arg;
        uint32_t abi_regcount;
        if (i->isop(LIR_alloc)) {
            LEA(r, disp(i), FP);
        }
        else if (i->isconst()) {
            if (!i->getArIndex()) {
                i->markAsClear();
            }
            LDi(r, i->imm32());
        }
        else if (i->isop(LIR_param) && i->paramKind() == 0 &&
            (arg = i->paramArg()) >= (abi_regcount = max_abi_regs[_thisfrag->lirbuf->abi])) {
            // incoming arg is on stack, can restore it from there instead of spilling
            if (!i->getArIndex()) {
                i->markAsClear();
            }
            // compute position of argument relative to ebp.  higher argument
            // numbers are at higher positive offsets.  the first abi_regcount
            // arguments are in registers, rest on stack.  +8 accomodates the
            // return address and saved ebp value.  assuming abi_regcount == 0:
            //    low-addr  ebp
            //    [frame...][saved-ebp][return-addr][arg0][arg1]...
            int d = (arg - abi_regcount) * sizeof(intptr_t) + 8;
            LD(r, d, FP);
        }
        else {
            int d = findMemFor(i);
            asm_load(d,r);
        }
    }

    void Assembler::asm_store32(LIns *value, int dr, LIns *base)
    {
        if (value->isconst())
        {
            Register rb = getBaseReg(LIR_sti, base, dr, GpRegs);
            int c = value->imm32();
            STi(rb, dr, c);
        }
        else
        {
            // make sure what is in a register
            Register ra, rb;
            if (base->isop(LIR_alloc)) {
                rb = FP;
                dr += findMemFor(base);
                ra = findRegFor(value, GpRegs);
            } else if (base->isconst()) {
                // absolute address
                dr += base->imm32();
                ra = findRegFor(value, GpRegs);
                rb = UnknownReg;
            } else {
                findRegFor2(GpRegs, value, ra, base, rb);
            }
            ST(rb, dr, ra);
        }
    }

    void Assembler::asm_spill(Register rr, int d, bool pop, bool quad)
    {
        (void)quad;
        if (d)
        {
            // save to spill location
            if (rmask(rr) & FpRegs)
            {
                if (rmask(rr) & XmmRegs) {
                    SSE_STQ(d, FP, rr);
                } else {
                    FSTQ((pop?1:0), d, FP);
                }
            }
            else
            {
                ST(FP, d, rr);
            }
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
        int db = ins->disp();
        Register rr = ins->getReg();

        if (isKnownReg(rr) && rmask(rr) & XmmRegs)
        {
            freeRsrcOf(ins, false);
            Register rb = getBaseReg(ins->opcode(), base, db, GpRegs);
            SSE_LDQ(rr, db, rb);
        }
        else
        {
            int dr = disp(ins);
            Register rb;
            if (base->isop(LIR_alloc)) {
                rb = FP;
                db += findMemFor(base);
            } else {
                rb = findRegFor(base, GpRegs);
            }
            ins->setReg(UnknownReg);

            // don't use an fpu reg to simply load & store the value.
            if (dr)
                asm_mmq(FP, dr, rb, db);

            freeRsrcOf(ins, false);

            if (isKnownReg(rr))
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
            Register rb;
            if (base->isop(LIR_alloc)) {
                rb = FP;
                dr += findMemFor(base);
            } else {
                rb = findRegFor(base, GpRegs);
            }
            STi(rb, dr+4, value->imm64_1());
            STi(rb, dr,   value->imm64_0());
            return;
        }

        if (value->isop(LIR_ldq) || value->isop(LIR_ldqc) || value->isop(LIR_qjoin))
        {
            // value is 64bit struct or int64_t, or maybe a double.
            // it may be live in an FPU reg.  Either way, don't
            // put it in an FPU reg just to load & store it.

            // a) if we know it's not a double, this is right.
            // b) if we guarded that its a double, this store could be on
            // the side exit, copying a non-double.
            // c) maybe its a double just being stored.  oh well.

            if (config.sse2) {
                Register rv = findRegFor(value, XmmRegs);
                Register rb;
                if (base->isop(LIR_alloc)) {
                    rb = FP;
                    dr += findMemFor(base);
                } else {
                    rb = findRegFor(base, GpRegs);
                }
                SSE_STQ(dr, rb, rv);
                return;
            }

            int da = findMemFor(value);
            Register rb;
            if (base->isop(LIR_alloc)) {
                rb = FP;
                dr += findMemFor(base);
            } else {
                rb = findRegFor(base, GpRegs);
            }
            asm_mmq(rb, dr, FP, da);
            return;
        }

        Register rb;
        if (base->isop(LIR_alloc)) {
            rb = FP;
            dr += findMemFor(base);
        } else {
            rb = findRegFor(base, GpRegs);
        }

        // if value already in a reg, use that, otherwise
        // try to get it into XMM regs before FPU regs.
        bool pop = value->isUnusedOrHasUnknownReg();
        Register rv = ( pop
                      ? findRegFor(value, config.sse2 ? XmmRegs : FpRegs)
                      : value->getReg() );

        if (rmask(rv) & XmmRegs) {
            SSE_STQ(dr, rb, rv);
        } else {
            FSTQ(pop?1:0, dr, rb);
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
        if (config.sse2)
        {
            // use SSE to load+store 64bits
            Register t = registerAllocTmp(XmmRegs);
            SSE_STQ(dd, rd, t);
            SSE_LDQ(t, ds, rs);
        }
        else
        {
            // get a scratch reg
            Register t = registerAllocTmp(GpRegs & ~(rmask(rd)|rmask(rs)));
            ST(rd, dd+4, t);
            LD(t, ds+4, rs);
            ST(rd, dd, t);
            LD(t, ds, rs);
        }
    }

    NIns* Assembler::asm_branch(bool branchOnFalse, LInsp cond, NIns* targ)
    {
        LOpcode condop = cond->opcode();
        NanoAssert(cond->isCond());

        // Handle float conditions separately.
        if (condop >= LIR_feq && condop <= LIR_fge) {
            return asm_fbranch(branchOnFalse, cond, targ);
        }

        if (branchOnFalse) {
            // op == LIR_xf
            switch (condop) {
            case LIR_ov:    JNO(targ);      break;
            case LIR_eq:    JNE(targ);      break;
            case LIR_lt:    JNL(targ);      break;
            case LIR_le:    JNLE(targ);     break;
            case LIR_gt:    JNG(targ);      break;
            case LIR_ge:    JNGE(targ);     break;
            case LIR_ult:   JNB(targ);      break;
            case LIR_ule:   JNBE(targ);     break;
            case LIR_ugt:   JNA(targ);      break;
            case LIR_uge:   JNAE(targ);     break;
            default:        NanoAssert(0);  break;
            }
        } else {
            // op == LIR_xt
            switch (condop) {
            case LIR_ov:    JO(targ);       break;
            case LIR_eq:    JE(targ);       break;
            case LIR_lt:    JL(targ);       break;
            case LIR_le:    JLE(targ);      break;
            case LIR_gt:    JG(targ);       break;
            case LIR_ge:    JGE(targ);      break;
            case LIR_ult:   JB(targ);       break;
            case LIR_ule:   JBE(targ);      break;
            case LIR_ugt:   JA(targ);       break;
            case LIR_uge:   JAE(targ);      break;
            default:        NanoAssert(0);  break;
            }
        }
        NIns* at = _nIns;
        asm_cmp(cond);
        return at;
    }

    void Assembler::asm_switch(LIns* ins, NIns* exit)
    {
        LIns* diff = ins->oprnd1();
        findSpecificRegFor(diff, EDX);
        JMP(exit);
    }

    void Assembler::asm_jtbl(LIns* ins, NIns** table)
    {
        Register indexreg = findRegFor(ins->oprnd1(), GpRegs);
        JMP_indexed(indexreg, 2, table);
    }

    // This generates a 'test' or 'cmp' instruction for a condition, which
    // causes the condition codes to be set appropriately.  It's used with
    // conditional branches, conditional moves, and when generating
    // conditional values.  For example:
    //
    //   LIR:   eq1 = eq a, 0
    //   LIR:   xf1: xf eq1 -> ...
    //   asm:       test edx, edx       # generated by this function
    //   asm:       je ...
    //
    // If this is the only use of eq1, then on entry 'cond' is *not* marked as
    // used, and we do not allocate a register for it.  That's because its
    // result ends up in the condition codes rather than a normal register.
    // This doesn't get recorded in the regstate and so the asm code that
    // consumes the result (eg. a conditional branch like 'je') must follow
    // shortly after.
    //
    // If eq1 is instead used again later, we will also generate code
    // (eg. in asm_cond()) to compute it into a normal register, something
    // like this:
    //
    //   LIR:   eq1 = eq a, 0
    //   LIR:       test edx, edx
    //   asm:       sete ebx
    //   asm:       movzx ebx, ebx        
    //
    // In this case we end up computing the condition twice, but that's ok, as
    // it's just as short as testing eq1's value in the code generated for the
    // guard.
    //
    // WARNING: Because the condition code update is not recorded in the
    // regstate, this function cannot generate any code that will affect the
    // condition codes prior to the generation of the test/cmp, because any
    // such code will be run after the test/cmp but before the instruction
    // that consumes the condition code.  And because this function calls
    // findRegFor() before the test/cmp is generated, and findRegFor() calls
    // asm_restore(), that means that asm_restore() cannot generate code which
    // affects the condition codes.
    //
    void Assembler::asm_cmp(LIns *cond)
    {
        LOpcode condop = cond->opcode();

        // LIR_ov recycles the flags set by arithmetic ops
        if (condop == LIR_ov)
            return;

        LInsp lhs = cond->oprnd1();
        LInsp rhs = cond->oprnd2();

        NanoAssert((!lhs->isQuad() && !rhs->isQuad()) || (lhs->isQuad() && rhs->isQuad()));

        // Not supported yet.
        NanoAssert(!lhs->isQuad() && !rhs->isQuad());

        // ready to issue the compare
        if (rhs->isconst())
        {
            int c = rhs->imm32();
            if (c == 0 && cond->isop(LIR_eq)) {
                Register r = findRegFor(lhs, GpRegs);
                TEST(r,r);
            } else if (!rhs->isQuad()) {
                Register r = getBaseReg(condop, lhs, c, GpRegs);
                CMPi(r, c);
            }
        }
        else
        {
            Register ra, rb;
            findRegFor2(GpRegs, lhs, ra, rhs, rb);
            CMP(ra, rb);
        }
    }

    void Assembler::asm_fcond(LInsp ins)
    {
        LOpcode opcode = ins->opcode();

        // only want certain regs
        Register r = prepResultReg(ins, AllowableFlagRegs);

        // SETcc only sets low 8 bits, so extend
        MOVZX8(r,r);

        if (config.sse2) {
            // LIR_flt and LIR_fgt are handled by the same case because
            // asm_fcmp() converts LIR_flt(a,b) to LIR_fgt(b,a).  Likewise
            // for LIR_fle/LIR_fge.
            switch (opcode) {
            case LIR_feq:   SETNP(r);       break;
            case LIR_flt:
            case LIR_fgt:   SETA(r);        break;
            case LIR_fle:
            case LIR_fge:   SETAE(r);       break;
            default:        NanoAssert(0);  break;
            }
        } else {
            SETNP(r);
        }
        asm_fcmp(ins);
    }

    void Assembler::asm_cond(LInsp ins)
    {
        // only want certain regs
        LOpcode op = ins->opcode();
        Register r = prepResultReg(ins, AllowableFlagRegs);
        // SETcc only sets low 8 bits, so extend
        MOVZX8(r,r);
        switch (op) {
        case LIR_ov:    SETO(r);        break;
        case LIR_eq:    SETE(r);        break;
        case LIR_lt:    SETL(r);        break;
        case LIR_le:    SETLE(r);       break;
        case LIR_gt:    SETG(r);        break;
        case LIR_ge:    SETGE(r);       break;
        case LIR_ult:   SETB(r);        break;
        case LIR_ule:   SETBE(r);       break;
        case LIR_ugt:   SETA(r);        break;
        case LIR_uge:   SETAE(r);       break;
        default:        NanoAssert(0);  break;
        }
        asm_cmp(ins);
    }

    void Assembler::asm_arith(LInsp ins)
    {
        LOpcode op = ins->opcode();
        LInsp lhs = ins->oprnd1();

        if (op == LIR_mod) {
            asm_div_mod(ins);
            return;
        }

        LInsp rhs = ins->oprnd2();

        bool forceReg;
        RegisterMask allow = GpRegs;
        Register rb = UnknownReg;

        switch (op) {
        case LIR_div:
            // Nb: if the div feeds into a mod it will be handled by
            // asm_div_mod() rather than here.
            forceReg = true;
            rb = findRegFor(rhs, (GpRegs ^ (rmask(EAX)|rmask(EDX))));
            allow = rmask(EAX);
            evictIfActive(EDX);
            break;
        case LIR_mul:
            forceReg = true;
            break;
        case LIR_lsh:
        case LIR_rsh:
        case LIR_ush:
            forceReg = !rhs->isconst();
            if (forceReg) {
                rb = findSpecificRegFor(rhs, ECX);
                allow &= ~rmask(rb);
            }
            break;
        case LIR_add:
        case LIR_addp:
            if (lhs->isop(LIR_alloc) && rhs->isconst()) {
                // add alloc+const, use lea
                Register rr = prepResultReg(ins, allow);
                int d = findMemFor(lhs) + rhs->imm32();
                LEA(rr, d, FP);
                return;
            }
            /* fall through */
        default:
            forceReg = !rhs->isconst();
            break;
        }

        // if we need a register for the rhs and don't have one yet, get it
        if (forceReg && lhs != rhs && !isKnownReg(rb)) {
            rb = findRegFor(rhs, allow);
            allow &= ~rmask(rb);
        }

        Register rr = prepResultReg(ins, allow);
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a register assigned.
        Register ra = ( lhs->isUnusedOrHasUnknownReg()
                      ? findSpecificRegForUnallocated(lhs, rr)
                      : lhs->getReg() );

        if (forceReg)
        {
            if (lhs == rhs)
                rb = ra;

            switch (op) {
            case LIR_add:
            case LIR_addp:
                ADD(rr, rb);
                break;
            case LIR_sub:
                SUB(rr, rb);
                break;
            case LIR_mul:
                MUL(rr, rb);
                break;
            case LIR_and:
                AND(rr, rb);
                break;
            case LIR_or:
                OR(rr, rb);
                break;
            case LIR_xor:
                XOR(rr, rb);
                break;
            case LIR_lsh:
                SHL(rr, rb);
                break;
            case LIR_rsh:
                SAR(rr, rb);
                break;
            case LIR_ush:
                SHR(rr, rb);
                break;
            case LIR_div:
                DIV(rb);
                CDQ();
                break;
            default:
                NanoAssertMsg(0, "Unsupported");
            }
        }
        else
        {
            int c = rhs->imm32();
            switch (op) {
            case LIR_addp:
                // this doesn't set cc's, only use it when cc's not required.
                LEA(rr, c, ra);
                ra = rr; // suppress mov
                break;
            case LIR_add:
                ADDi(rr, c);
                break;
            case LIR_sub:
                SUBi(rr, c);
                break;
            case LIR_and:
                ANDi(rr, c);
                break;
            case LIR_or:
                ORi(rr, c);
                break;
            case LIR_xor:
                XORi(rr, c);
                break;
            case LIR_lsh:
                SHLi(rr, c);
                break;
            case LIR_rsh:
                SARi(rr, c);
                break;
            case LIR_ush:
                SHRi(rr, c);
                break;
            default:
                NanoAssertMsg(0, "Unsupported");
                break;
            }
        }

        if ( rr != ra )
            MR(rr,ra);
    }

    // This is called when we have a mod(div(divLhs, divRhs)) sequence.
    void Assembler::asm_div_mod(LInsp mod)
    {
        LInsp div = mod->oprnd1();

        // LIR_mod expects the LIR_div to be near (no interference from the register allocator)

        NanoAssert(mod->isop(LIR_mod));
        NanoAssert(div->isop(LIR_div));

        LInsp divLhs = div->oprnd1();
        LInsp divRhs = div->oprnd2();

        prepResultReg(mod, rmask(EDX));
        prepResultReg(div, rmask(EAX));

        Register rDivRhs = findRegFor(divRhs, (GpRegs ^ (rmask(EAX)|rmask(EDX))));

        Register rDivLhs = ( divLhs->isUnusedOrHasUnknownReg()
                           ? findSpecificRegFor(divLhs, EAX)
                           : divLhs->getReg() );

        DIV(rDivRhs);
        CDQ();     // sign-extend EAX into EDX:EAX

        if ( EAX != rDivLhs )
            MR(EAX, rDivLhs);
    }

    void Assembler::asm_neg_not(LInsp ins)
    {
        LOpcode op = ins->opcode();
        Register rr = prepResultReg(ins, GpRegs);

        LIns* lhs = ins->oprnd1();
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a register assigned.
        Register ra = ( lhs->isUnusedOrHasUnknownReg()
                      ? findSpecificRegForUnallocated(lhs, rr)
                      : lhs->getReg() );

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
        int32_t d = ins->disp();
        Register rr = prepResultReg(ins, GpRegs);

        if (base->isconst()) {
            intptr_t addr = base->imm32();
            addr += d;
            if (op == LIR_ldcb)
                LD8Zdm(rr, addr);
            else if (op == LIR_ldcs)
                LD16Zdm(rr, addr);
            else
                LDdm(rr, addr);
            return;
        }

        /* Search for add(X,Y) */
        if (base->opcode() == LIR_piadd) {
            int scale = 0;
            LIns *lhs = base->oprnd1();
            LIns *rhs = base->oprnd2();

            /* See if we can bypass any SHLs, by searching for
             * add(X, shl(Y,Z)) -> mov r, [X+Y*Z]
             */
            if (rhs->opcode() == LIR_pilsh && rhs->oprnd2()->isconst()) {
                scale = rhs->oprnd2()->imm32();
                if (scale >= 1 && scale <= 3)
                    rhs = rhs->oprnd1();
                else
                    scale = 0;
            }

            /* Does LHS have a register yet? If not, re-use the result reg.
             * @todo -- If LHS is const, we could eliminate a register use.
             */
            Register rleft = ( lhs->isUnusedOrHasUnknownReg()
                             ? findSpecificRegForUnallocated(lhs, rr)
                             : lhs->getReg() );

            /* Does RHS have a register yet? If not, try to re-use the result reg. */
            Register rright = ( rr != rleft && rhs->isUnusedOrHasUnknownReg()
                              ? findSpecificRegForUnallocated(rhs, rr)
                              : findRegFor(rhs, GpRegs & ~(rmask(rleft))) );

            if (op == LIR_ldcb)
                LD8Zsib(rr, d, rleft, rright, scale);
            else if (op == LIR_ldcs)
                LD16Zsib(rr, d, rleft, rright, scale);
            else
                LDsib(rr, d, rleft, rright, scale);

            return;
        }

        Register ra = getBaseReg(op, base, d, GpRegs);
        if (op == LIR_ldcb)
            LD8Z(rr, d, ra);
        else if (op == LIR_ldcs)
            LD16Z(rr, d, ra);
        else
            LD(rr, d, ra);
    }

    void Assembler::asm_cmov(LInsp ins)
    {
        LOpcode op = ins->opcode();
        LIns* condval = ins->oprnd1();
        LIns* iftrue  = ins->oprnd2();
        LIns* iffalse = ins->oprnd3();

        NanoAssert(condval->isCmp());
        NanoAssert(op == LIR_qcmov || (!iftrue->isQuad() && !iffalse->isQuad()));

        const Register rr = prepResultReg(ins, GpRegs);

        // this code assumes that neither LD nor MR nor MRcc set any of the condition flags.
        // (This is true on Intel, is it true on all architectures?)
        const Register iffalsereg = findRegFor(iffalse, GpRegs & ~rmask(rr));
        if (op == LIR_cmov) {
            switch (condval->opcode())
            {
                // note that these are all opposites...
                case LIR_eq:    MRNE(rr, iffalsereg);   break;
                case LIR_ov:    MRNO(rr, iffalsereg);   break;
                case LIR_lt:    MRGE(rr, iffalsereg);   break;
                case LIR_le:    MRG(rr, iffalsereg);    break;
                case LIR_gt:    MRLE(rr, iffalsereg);   break;
                case LIR_ge:    MRL(rr, iffalsereg);    break;
                case LIR_ult:   MRAE(rr, iffalsereg);   break;
                case LIR_ule:   MRA(rr, iffalsereg);    break;
                case LIR_ugt:   MRBE(rr, iffalsereg);   break;
                case LIR_uge:   MRB(rr, iffalsereg);    break;
                default: NanoAssert(0); break;
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
        uint32_t a = ins->paramArg();
        uint32_t kind = ins->paramKind();
        if (kind == 0) {
            // ordinary param
            AbiKind abi = _thisfrag->lirbuf->abi;
            uint32_t abi_regcount = max_abi_regs[abi];
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
        Register rr = ins->getReg();
        if (isKnownReg(rr))
        {
            // @todo -- add special-cases for 0 and 1
            _allocator.retire(rr);
            ins->setReg(UnknownReg);
            NanoAssert((rmask(rr) & FpRegs) != 0);

            const double d = ins->imm64f();
            const uint64_t q = ins->imm64();
            if (rmask(rr) & XmmRegs) {
                if (q == 0.0) {
                    // test (int64)0 since -0.0 == 0.0
                    SSE_XORPDr(rr, rr);
                } else if (d == 1.0) {
                    // 1.0 is extremely frequent and worth special-casing!
                    static const double k_ONE = 1.0;
                    LDSDm(rr, &k_ONE);
                } else if (d && d == (int)d) {
                    // can fit in 32bits? then use cvt which is faster
                    Register gr = registerAllocTmp(GpRegs);
                    SSE_CVTSI2SD(rr, gr);
                    SSE_XORPDr(rr,rr);  // zero rr to ensure no dependency stalls
                    LDi(gr, (int)d);
                } else {
                    findMemFor(ins);
                    const int d = disp(ins);
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
                    int d = disp(ins);
                    FLDQ(d,FP);
                }
            }
        }

        // @todo, if we used xor, ldsd, fldz, etc above, we don't need mem here
        int d = disp(ins);
        freeRsrcOf(ins, false);
        if (d)
        {
            STi(FP,d+4,ins->imm64_1());
            STi(FP,d,  ins->imm64_0());
        }
    }

    void Assembler::asm_qlo(LInsp ins)
    {
        LIns *q = ins->oprnd1();

        if (!config.sse2)
        {
            Register rr = prepResultReg(ins, GpRegs);
            int d = findMemFor(q);
            LD(rr, d, FP);
        }
        else
        {
            Register rr = ins->getReg();
            if (!isKnownReg(rr)) {
                // store quad in spill loc
                int d = disp(ins);
                freeRsrcOf(ins, false);
                Register qr = findRegFor(q, XmmRegs);
                SSE_MOVDm(d, FP, qr);
            } else {
                freeRsrcOf(ins, false);
                Register qr = findRegFor(q, XmmRegs);
                SSE_MOVD(rr,qr);
            }
        }
    }

	// negateMask is used by asm_fneg.
#if defined __SUNPRO_CC
    // From Sun Studio C++ Readme: #pragma align inside namespace requires mangled names.
    // Initialize here to avoid multithreading contention issues during initialization.
    static uint32_t negateMask_temp[] = {0, 0, 0, 0, 0, 0, 0};

    static uint32_t* negateMaskInit()
    {
        uint32_t* negateMask = (uint32_t*)alignUp(negateMask_temp, 16);
        negateMask[1] = 0x80000000;
        return negateMask;
    }

    static uint32_t *negateMask = negateMaskInit();
#else
    static const AVMPLUS_ALIGN16(uint32_t) negateMask[] = {0,0x80000000,0,0};
#endif

    void Assembler::asm_fneg(LInsp ins)
    {
        if (config.sse2)
        {
            LIns *lhs = ins->oprnd1();

            Register rr = prepResultReg(ins, XmmRegs);
            Register ra;

            // if this is last use of lhs in reg, we can re-use result reg
            // else, lhs already has a register assigned.
            if (lhs->isUnusedOrHasUnknownReg()) {
                ra = findSpecificRegForUnallocated(lhs, rr);
            } else {
                ra = lhs->getReg();
                if ((rmask(ra) & XmmRegs) == 0) {
                    /* We need this case on AMD64, because it's possible that
                     * an earlier instruction has done a quadword load and reserved a
                     * GPR.  If so, ask for a new register.
                     */
                    ra = findRegFor(lhs, XmmRegs);
                }
            }

            SSE_XORPD(rr, negateMask);

            if (rr != ra)
                SSE_MOVSD(rr, ra);
        }
        else
        {
            Register rr = prepResultReg(ins, FpRegs);

            LIns* lhs = ins->oprnd1();

            // lhs into reg, prefer same reg as result

            // if this is last use of lhs in reg, we can re-use result reg
            // else, lhs already has a different reg assigned
            if (lhs->isUnusedOrHasUnknownReg())
                findSpecificRegForUnallocated(lhs, rr);

            NanoAssert(lhs->getReg()==FST0);
            // assume that the lhs is in ST(0) and rhs is on stack
            FCHS();

            // if we had more than one fpu reg, this is where
            // we would move ra into rr if rr != ra.
        }
    }

    void Assembler::asm_arg(ArgSize sz, LInsp p, Register r, int32_t& stkd)
    {
        if (sz == ARGSIZE_Q)
        {
            // ref arg - use lea
            if (isKnownReg(r))
            {
                // arg in specific reg
                int da = findMemFor(p);
                LEA(r, da, FP);
            }
            else
            {
                NanoAssert(0); // not supported
            }
        }
        else if (sz == ARGSIZE_I || sz == ARGSIZE_U)
        {
            if (isKnownReg(r)) {
                // arg goes in specific register
                if (p->isconst()) {
                    LDi(r, p->imm32());
                } else {
                    if (p->isUsed()) {
                        if (!p->hasKnownReg()) {
                            // load it into the arg reg
                            int d = findMemFor(p);
                            if (p->isop(LIR_alloc)) {
                                LEA(r, d, FP);
                            } else {
                                LD(r, d, FP);
                            }
                        } else {
                            // it must be in a saved reg
                            MR(r, p->getReg());
                        }
                    }
                    else {
                        // this is the last use, so fine to assign it
                        // to the scratch reg, it's dead after this point.
                        findSpecificRegFor(p, r);
                    }
                }
            }
            else {
                if (config.fixed_esp)
                    asm_stkarg(p, stkd);
                else
                    asm_pusharg(p);
            }
        }
        else
        {
            NanoAssert(sz == ARGSIZE_F);
            asm_farg(p, stkd);
        }
    }

    void Assembler::asm_pusharg(LInsp p)
    {
        // arg goes on stack
        if (!p->isUsed() && p->isconst())
        {
            // small const we push directly
            PUSHi(p->imm32());
        }
        else if (!p->isUsed() || p->isop(LIR_alloc))
        {
            Register ra = findRegFor(p, GpRegs);
            PUSHr(ra);
        }
        else if (!p->hasKnownReg())
        {
            PUSHm(disp(p), FP);
        }
        else
        {
            PUSHr(p->getReg());
        }
    }

    void Assembler::asm_stkarg(LInsp p, int32_t& stkd)
    {
        // arg goes on stack
        if (!p->isUsed() && p->isconst())
        {
            // small const we push directly
            STi(SP, stkd, p->imm32());
        }
        else {
            Register ra;
            if (!p->isUsed() || p->getReg() == UnknownReg || p->isop(LIR_alloc))
                ra = findRegFor(p, GpRegs & (~SavedRegs));
            else
                ra = p->getReg();
            ST(SP, stkd, ra);
        }

        stkd += sizeof(int32_t);
    }

    void Assembler::asm_farg(LInsp p, int32_t& stkd)
    {
        NanoAssert(p->isQuad());
        Register r = findRegFor(p, FpRegs);
        if (rmask(r) & XmmRegs) {
            SSE_STQ(stkd, SP, r);
        } else {
            FSTPQ(stkd, SP);
            
            //
            // 22Jul09 rickr - Enabling the evict causes a 10% slowdown on primes
            //
            // evict() triggers a very expensive fstpq/fldq pair around the store.
            // We need to resolve the bug some other way.
            //
            // see https://bugzilla.mozilla.org/show_bug.cgi?id=491084
            
            /* It's possible that the same LIns* with r=FST0 will appear in the argument list more
             * than once.  In this case FST0 will not have been evicted and the multiple pop
             * actions will unbalance the FPU stack.  A quick fix is to always evict FST0 manually.
             */
            evictIfActive(FST0);
        }
        if (!config.fixed_esp)
            SUBi(ESP,8);

        stkd += sizeof(double);
    }

    void Assembler::asm_fop(LInsp ins)
    {
        LOpcode op = ins->opcode();
        if (config.sse2)
        {
            LIns *lhs = ins->oprnd1();
            LIns *rhs = ins->oprnd2();

            RegisterMask allow = XmmRegs;
            Register rb = UnknownReg;
            if (lhs != rhs) {
                rb = findRegFor(rhs,allow);
                allow &= ~rmask(rb);
            }

            Register rr = prepResultReg(ins, allow);
            Register ra;

            // if this is last use of lhs in reg, we can re-use result reg
            if (lhs->isUnusedOrHasUnknownReg()) {
                ra = findSpecificRegForUnallocated(lhs, rr);
            } else if ((rmask(lhs->getReg()) & XmmRegs) == 0) {
                // We need this case on AMD64, because it's possible that
                // an earlier instruction has done a quadword load and reserved a
                // GPR.  If so, ask for a new register.
                ra = findRegFor(lhs, XmmRegs);
            } else {
                // lhs already has a register assigned but maybe not from the allow set
                ra = findRegFor(lhs, allow);
            }

            if (lhs == rhs)
                rb = ra;

            if (op == LIR_fadd)
                SSE_ADDSD(rr, rb);
            else if (op == LIR_fsub)
                SSE_SUBSD(rr, rb);
            else if (op == LIR_fmul)
                SSE_MULSD(rr, rb);
            else //if (op == LIR_fdiv)
                SSE_DIVSD(rr, rb);

            if (rr != ra)
                SSE_MOVSD(rr, ra);
        }
        else
        {
            // we swap lhs/rhs on purpose here, works out better
            // if you only have one fpu reg.  use divr/subr.
            LIns* rhs = ins->oprnd1();
            LIns* lhs = ins->oprnd2();
            Register rr = prepResultReg(ins, rmask(FST0));

            // make sure rhs is in memory
            int db = findMemFor(rhs);

            // lhs into reg, prefer same reg as result

            // last use of lhs in reg, can reuse rr
            // else, lhs already has a different reg assigned
            if (lhs->isUnusedOrHasUnknownReg())
                findSpecificRegForUnallocated(lhs, rr);

            NanoAssert(lhs->getReg()==FST0);
            // assume that the lhs is in ST(0) and rhs is on stack
            if (op == LIR_fadd)
                { FADD(db, FP); }
            else if (op == LIR_fsub)
                { FSUBR(db, FP); }
            else if (op == LIR_fmul)
                { FMUL(db, FP); }
            else if (op == LIR_fdiv)
                { FDIVR(db, FP); }
        }
    }

    void Assembler::asm_i2f(LInsp ins)
    {
        // where our result goes
        Register rr = prepResultReg(ins, FpRegs);
        if (rmask(rr) & XmmRegs)
        {
            // todo support int value in memory
            Register gr = findRegFor(ins->oprnd1(), GpRegs);
            SSE_CVTSI2SD(rr, gr);
            SSE_XORPDr(rr,rr);  // zero rr to ensure no dependency stalls
        }
        else
        {
            int d = findMemFor(ins->oprnd1());
            FILD(d, FP);
        }
    }

    Register Assembler::asm_prep_fcall(LInsp ins)
    {
        return prepResultReg(ins, rmask(FST0));
    }

    void Assembler::asm_u2f(LInsp ins)
    {
        // where our result goes
        Register rr = prepResultReg(ins, FpRegs);
        if (rmask(rr) & XmmRegs)
        {
            // don't call findRegFor, we want a reg we can stomp on for a very short time,
            // not a reg that will continue to be associated with the LIns
            Register gr = registerAllocTmp(GpRegs);

            // technique inspired by gcc disassembly
            // Edwin explains it:
            //
            // gr is 0..2^32-1
            //
            //     sub gr,0x80000000
            //
            // now gr is -2^31..2^31-1, i.e. the range of int, but not the same value
            // as before
            //
            //     cvtsi2sd rr,gr
            //
            // rr is now a double with the int value range
            //
            //     addsd rr, 2147483648.0
            //
            // adding back double(0x80000000) makes the range 0..2^32-1.

            static const double k_NEGONE = 2147483648.0;
            SSE_ADDSDm(rr, &k_NEGONE);

            SSE_CVTSI2SD(rr, gr);
            SSE_XORPDr(rr,rr);  // zero rr to ensure no dependency stalls

            LIns* op1 = ins->oprnd1();
            Register xr;
            if (op1->isUsed() && (xr = op1->getReg(), isKnownReg(xr)) && (rmask(xr) & GpRegs))
            {
                LEA(gr, 0x80000000, xr);
            }
            else
            {
                const int d = findMemFor(ins->oprnd1());
                SUBi(gr, 0x80000000);
                LD(gr, d, FP);
            }
        }
        else
        {
            const int disp = -8;
            const Register base = SP;
            Register gr = findRegFor(ins->oprnd1(), GpRegs);
            NanoAssert(rr == FST0);
            FILDQ(disp, base);
            STi(base, disp+4, 0);   // high 32 bits = 0
            ST(base, disp, gr);     // low 32 bits = unsigned value
        }
    }

    void Assembler::asm_nongp_copy(Register r, Register s)
    {
        if ((rmask(r) & XmmRegs) && (rmask(s) & XmmRegs)) {
            SSE_MOVSD(r, s);
        } else if ((rmask(r) & GpRegs) && (rmask(s) & XmmRegs)) {
            SSE_MOVD(r, s);
        } else {
            if (rmask(r) & XmmRegs) {
                // x87 -> xmm
                NanoAssertMsg(false, "Should not move data from GPR to XMM");
            } else {
                // xmm -> x87
                NanoAssertMsg(false, "Should not move data from GPR/XMM to x87 FPU");
            }
        }
    }

    NIns* Assembler::asm_fbranch(bool branchOnFalse, LIns *cond, NIns *targ)
    {
        NIns* at;
        LOpcode opcode = cond->opcode();

        if (config.sse2) {
            // LIR_flt and LIR_fgt are handled by the same case because
            // asm_fcmp() converts LIR_flt(a,b) to LIR_fgt(b,a).  Likewise
            // for LIR_fle/LIR_fge.
            if (branchOnFalse) {
                // op == LIR_xf
                switch (opcode) {
                case LIR_feq:   JP(targ);       break;
                case LIR_flt:
                case LIR_fgt:   JNA(targ);      break;
                case LIR_fle:
                case LIR_fge:   JNAE(targ);     break;
                default:        NanoAssert(0);  break;
                }
            } else {
                // op == LIR_xt
                switch (opcode) {
                case LIR_feq:   JNP(targ);      break;
                case LIR_flt:
                case LIR_fgt:   JA(targ);       break;
                case LIR_fle:
                case LIR_fge:   JAE(targ);      break;
                default:        NanoAssert(0);  break;
                }
            }
        } else {
            if (branchOnFalse)
                JP(targ);
            else
                JNP(targ);
        }

        at = _nIns;
        asm_fcmp(cond);

        return at;
    }

    // WARNING: This function cannot generate any code that will affect the
    // condition codes prior to the generation of the
    // ucomisd/fcompp/fcmop/fcom.  See asm_cmp() for more details.
    void Assembler::asm_fcmp(LIns *cond)
    {
        LOpcode condop = cond->opcode();
        NanoAssert(condop >= LIR_feq && condop <= LIR_fge);
        LIns* lhs = cond->oprnd1();
        LIns* rhs = cond->oprnd2();
        NanoAssert(lhs->isQuad() && rhs->isQuad());

        if (config.sse2) {
            // First, we convert (a < b) into (b > a), and (a <= b) into (b >= a).
            if (condop == LIR_flt) {
                condop = LIR_fgt;
                LIns* t = lhs; lhs = rhs; rhs = t;
            } else if (condop == LIR_fle) {
                condop = LIR_fge;
                LIns* t = lhs; lhs = rhs; rhs = t;
            }

            if (condop == LIR_feq) {
                if (lhs == rhs) {
                    // We can generate better code for LIR_feq when lhs==rhs (NaN test).

                    // ucomisd    ZPC  outcome (SETNP/JNP succeeds if P==0)
                    // -------    ---  -------
                    // UNORDERED  111  SETNP/JNP fails
                    // EQUAL      100  SETNP/JNP succeeds

                    Register r = findRegFor(lhs, XmmRegs);
                    SSE_UCOMISD(r, r);
                } else {
                    // LAHF puts the flags into AH like so:  SF:ZF:0:AF:0:PF:1:CF (aka. SZ0A_0P1C).
                    // We then mask out the bits as follows.
                    // - LIR_feq: mask == 0x44 == 0100_0100b, which extracts 0Z00_0P00 from AH.
                    int mask = 0x44;

                    // ucomisd       ZPC   lahf/test(0x44) SZP   outcome
                    // -------       ---   ---------       ---   -------
                    // UNORDERED     111   0100_0100       001   SETNP/JNP fails
                    // EQUAL         100   0100_0000       000   SETNP/JNP succeeds
                    // GREATER_THAN  000   0000_0000       011   SETNP/JNP fails 
                    // LESS_THAN     001   0000_0000       011   SETNP/JNP fails
        
                    evictIfActive(EAX);
                    Register ra, rb;
                    findRegFor2(XmmRegs, lhs, ra, rhs, rb);

                    TEST_AH(mask);
                    LAHF();
                    SSE_UCOMISD(ra, rb);
                }
            } else {
                // LIR_fgt:
                //   ucomisd       ZPC   outcome (SETA/JA succeeds if CZ==00)
                //   -------       ---   -------
                //   UNORDERED     111   SETA/JA fails
                //   EQUAL         100   SETA/JA fails
                //   GREATER_THAN  000   SETA/JA succeeds 
                //   LESS_THAN     001   SETA/JA fails 
                //
                // LIR_fge:
                //   ucomisd       ZPC   outcome (SETAE/JAE succeeds if C==0)
                //   -------       ---   -------
                //   UNORDERED     111   SETAE/JAE fails
                //   EQUAL         100   SETAE/JAE succeeds
                //   GREATER_THAN  000   SETAE/JAE succeeds
                //   LESS_THAN     001   SETAE/JAE fails

                Register ra, rb;
                findRegFor2(XmmRegs, lhs, ra, rhs, rb);
                SSE_UCOMISD(ra, rb);
            }

        } else {
            // First, we convert (a > b) into (b < a), and (a >= b) into (b <= a).
            // Note that this is the opposite of the sse2 conversion above.
            if (condop == LIR_fgt) {
                condop = LIR_flt;
                LIns* t = lhs; lhs = rhs; rhs = t;
            } else if (condop == LIR_fge) {
                condop = LIR_fle;
                LIns* t = lhs; lhs = rhs; rhs = t;
            }

            // FNSTSW_AX puts the flags into AH like so:  B:C3:TOP3:TOP2:TOP1:C2:C1:C0.
            // Furthermore, fcom/fcomp/fcompp sets C3:C2:C0 the same values
            // that Z:P:C are set by ucomisd, and the relative positions in AH
            // line up.  (Someone at Intel has a sense of humour.)  Therefore
            // we can use the same lahf/test(mask) technique as used in the
            // sse2 case above.  We could use fcomi/fcomip/fcomipp which set
            // ZPC directly and then use LAHF instead of FNSTSW_AX and make
            // this code generally more like the sse2 code, but we don't
            // because fcomi/fcomip/fcomipp/lahf aren't available on earlier
            // x86 machines.
            //
            // The masks are as follows:
            // - LIR_feq: mask == 0x44 == 0100_0100b, which extracts 0Z00_0P00 from AH.
            // - LIR_flt: mask == 0x05 == 0000_0101b, which extracts 0000_0P0C from AH.
            // - LIR_fle: mask == 0x41 == 0100_0001b, which extracts 0Z00_000C from AH.
            //
            // LIR_feq (very similar to the sse2 case above):
            //   ucomisd  C3:C2:C0   lahf/test(0x44) SZP   outcome
            //   -------  --------   ---------       ---   -------
            //   UNORDERED     111   0100_0100       001   SETNP fails
            //   EQUAL         100   0100_0000       000   SETNP succeeds
            //   GREATER_THAN  000   0000_0000       011   SETNP fails 
            //   LESS_THAN     001   0000_0000       011   SETNP fails
            //
            // LIR_flt:
            //   fcom     C3:C2:C0   lahf/test(0x05) SZP   outcome
            //   -------  --------   ---------       ---   -------
            //   UNORDERED     111   0000_0101       001   SETNP fails
            //   EQUAL         100   0000_0000       011   SETNP fails
            //   GREATER_THAN  000   0000_0000       011   SETNP fails 
            //   LESS_THAN     001   0000_0001       000   SETNP succeeds
            //
            // LIR_fle:
            //   fcom     C3:C2:C0   lahf/test(0x41) SZP   outcome
            //   -------       ---   ---------       ---   -------
            //   UNORDERED     111   0100_0001       001   SETNP fails
            //   EQUAL         100   0100_0000       000   SETNP succeeds
            //   GREATER_THAN  000   0000_0000       011   SETNP fails 
            //   LESS_THAN     001   0000_0001       010   SETNP succeeds

            int mask = 0;   // init to avoid MSVC compile warnings
            switch (condop) {
            case LIR_feq:   mask = 0x44;    break;
            case LIR_flt:   mask = 0x05;    break;
            case LIR_fle:   mask = 0x41;    break;
            default:        NanoAssert(0);  break;
            }

            evictIfActive(EAX);
            int pop = lhs->isUnusedOrHasUnknownReg();
            findSpecificRegFor(lhs, FST0);

            if (lhs == rhs) {
                // NaN test.
                // value in ST(0)
                TEST_AH(mask);
                FNSTSW_AX();
                if (pop)
                    FCOMPP();
                else
                    FCOMP();
                FLDr(FST0); // DUP
            } else {
                int d = findMemFor(rhs);
                // lhs is in ST(0) and rhs is on stack
                TEST_AH(mask);
                FNSTSW_AX();
                FCOM((pop?1:0), d, FP);
            }
        }
    }

    // Increment the 32-bit profiling counter at pCtr, without
    // changing any registers.
    verbose_only(
    void Assembler::asm_inc_m32(uint32_t* pCtr)
    {
       INCLi(pCtr);
    }
    )

    void Assembler::nativePageReset()
    {}

    void Assembler::nativePageSetup()
    {
        NanoAssert(!_inExit);
        if (!_nIns)
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
        if (!_nExitIns)
            codeAlloc(exitStart, exitEnd, _nExitIns verbose_only(, exitBytes));
    }

    // enough room for n bytes
    void Assembler::underrunProtect(int n)
    {
        NIns *eip = _nIns;
        NanoAssertMsg(n<=LARGEST_UNDERRUN_PROT, "constant LARGEST_UNDERRUN_PROT is too small");
        // This may be in a normal code chunk or an exit code chunk.
        if (eip - n < codeStart) {
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
            JMP(eip);
        }
    }

    void Assembler::asm_ret(LInsp ins)
    {
        genEpilogue();

        // Restore ESP from EBP, undoing SUBi(SP,amt) in the prologue
        MR(SP,FP);

        assignSavedRegs();
        LIns *val = ins->oprnd1();
        if (ins->isop(LIR_ret)) {
            findSpecificRegFor(val, retRegs[0]);
        } else {
            findSpecificRegFor(val, FST0);
            fpu_pop();
        }
    }

    void Assembler::asm_promote(LIns *) {
        // i2q or u2q
        TODO(asm_promote);
    }

    void Assembler::swapCodeChunks() {
        SWAP(NIns*, _nIns, _nExitIns);
        SWAP(NIns*, codeStart, exitStart);
        SWAP(NIns*, codeEnd, exitEnd);
        verbose_only( SWAP(size_t, codeBytes, exitBytes); )
    }

    #endif /* FEATURE_NANOJIT */
}
