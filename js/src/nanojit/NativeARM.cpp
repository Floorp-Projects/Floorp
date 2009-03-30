/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 *   Vladimir Vukicevic <vladimir@pobox.com>
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
#include <signal.h>
#include <setjmp.h>
#include <asm/unistd.h>
extern "C" void __clear_cache(char *BEG, char *END);
#endif

// assume EABI, except under CE
#ifdef UNDER_CE
#undef NJ_ARM_EABI
#else
#define NJ_ARM_EABI
#endif

#ifdef FEATURE_NANOJIT

namespace nanojit
{

#ifdef NJ_VERBOSE
const char* regNames[] = {"r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","fp","ip","sp","lr","pc",
                          "d0","d1","d2","d3","d4","d5","d6","d7","s14"};
const char* condNames[] = {"eq","ne","cs","cc","mi","pl","vs","vc","hi","ls","ge","lt","gt","le",""/*al*/,"nv"};
const char* shiftNames[] = { "lsl", "lsl", "lsr", "lsr", "asr", "asr", "ror", "ror" };
#endif

const Register Assembler::argRegs[] = { R0, R1, R2, R3 };
const Register Assembler::retRegs[] = { R0, R1 };
const Register Assembler::savedRegs[] = { R4, R5, R6, R7, R8, R9, R10 };

void
Assembler::nInit(AvmCore*)
{
}

NIns*
Assembler::genPrologue()
{
    /**
     * Prologue
     */

    // NJ_RESV_OFFSET is space at the top of the stack for us
    // to use for parameter passing (8 bytes at the moment)
    uint32_t stackNeeded = STACK_GRANULARITY * _activation.highwatermark + NJ_STACK_OFFSET;
    uint32_t savingCount = 2;

    uint32_t savingMask = rmask(FP) | rmask(LR);

    if (!_thisfrag->lirbuf->explicitSavedRegs) {
        for (int i = 0; i < NumSavedRegs; ++i)
            savingMask |= rmask(savedRegs[i]);
        savingCount += NumSavedRegs;
    }

    // so for alignment purposes we've pushed return addr and fp
    uint32_t stackPushed = STACK_GRANULARITY * savingCount;
    uint32_t aligned = alignUp(stackNeeded + stackPushed, NJ_ALIGN_STACK);
    int32_t amt = aligned - stackPushed;

    // Make room on stack for what we are doing
    if (amt)
        SUBi(SP, SP, amt);

    verbose_only( verbose_outputf("         %p:",_nIns); )
    verbose_only( verbose_output("         patch entry"); )
    NIns *patchEntry = _nIns;

    MOV(FP, SP);
    PUSH_mask(savingMask);
    return patchEntry;
}

void
Assembler::nFragExit(LInsp guard)
{
    SideExit* exit = guard->record()->exit;
    Fragment *frag = exit->target;
    GuardRecord *lr;

    if (frag && frag->fragEntry) {
        JMP_far(frag->fragEntry);
        lr = 0;
    } else {
        // target doesn't exit yet.  emit jump to epilog, and set up to patch later.
        lr = guard->record();

        // jump to the epilogue; JMP_far will insert an extra dummy insn for later
        // patching.
        JMP_far(_epilogue);

        // stick the jmp pointer to the start of the sequence
        lr->jmp = _nIns;
    }

    // pop the stack frame first
    MOV(SP, FP);

#ifdef NJ_VERBOSE
    if (_frago->core()->config.show_stats) {
        // load R1 with Fragment *fromFrag, target fragment
        // will make use of this when calling fragenter().
        int fromfrag = int((Fragment*)_thisfrag);
        LDi(argRegs[1], fromfrag);
    }
#endif

    // return value is GuardRecord*; note that this goes into
    // R2, not R0 -- genEpilogue will move it into R0.  Otherwise
    // we want R0 to have the original value that it had at the
    // start of trace.
    LDi(R2, int(lr));
}

NIns*
Assembler::genEpilogue()
{
    BX(LR); // return

    RegisterMask savingMask = rmask(FP) | rmask(LR);

    if (!_thisfrag->lirbuf->explicitSavedRegs)
        for (int i = 0; i < NumSavedRegs; ++i)
            savingMask |= rmask(savedRegs[i]);

    POP_mask(savingMask); // regs

    MOV(SP,FP);

    // this is needed if we jump here from nFragExit
    MOV(R0,R2); // return LinkRecord*

    return _nIns;
}

/* gcc/linux use the ARM EABI; Windows CE uses the legacy abi.
 *
 * Under EABI:
 * - doubles are 64-bit aligned both in registers and on the stack.
 *   If the next available argument register is R1, it is skipped
 *   and the double is placed in R2:R3.  If R0:R1 or R2:R3 are not
 *   available, the double is placed on the stack, 64-bit aligned.
 * - 32-bit arguments are placed in registers and 32-bit aligned
 *   on the stack.
 *
 * Under legacy ABI:
 * - doubles are placed in subsequent arg registers; if the next
 *   available register is r3, the low order word goes into r3
 *   and the high order goes on the stack.
 * - 32-bit arguments are placed in the next available arg register,
 * - both doubles and 32-bit arguments are placed on stack with 32-bit
 *   alignment.
 */

void
Assembler::asm_arg(ArgSize sz, LInsp p, Register r)
{
    // should never be called -- the ARM-specific longer form of
    // asm_arg is used on ARM.
    NanoAssert(0);
}

/*
 * asm_arg will update r and stkd to indicate where the next
 * argument should go.  If r == UnknownReg, then the argument
 * is placed on the stack at stkd, and stkd is updated.
 *
 * Note that this currently doesn't actually use stkd on input,
 * except for figuring out alignment; it always pushes to SP.
 * See TODO in asm_call.
 */
void
Assembler::asm_arg(ArgSize sz, LInsp arg, Register& r, int& stkd)
{
    if (sz == ARGSIZE_F) {
#ifdef NJ_ARM_EABI
        NanoAssert(r == UnknownReg || r == R0 || r == R2);

        // if we're about to put this on the stack, make sure the
        // stack is 64-bit aligned
        if (r == UnknownReg && (stkd&7) != 0) {
            SUBi(SP, SP, 4);
            stkd += 4;
        }
#endif

        Reservation* argRes = getresv(arg);

        // handle qjoin first; won't ever show up if VFP is available
        if (arg->isop(LIR_qjoin)) {
            asm_arg(ARGSIZE_LO, arg->oprnd1(), r, stkd);
            asm_arg(ARGSIZE_LO, arg->oprnd2(), r, stkd);
        } else if (!argRes || argRes->reg == UnknownReg || !AvmCore::config.vfp) {
            // if we don't have a register allocated,
            // or we're not vfp, just read from memory.
            if (arg->isop(LIR_quad)) {
                const int32_t* p = (const int32_t*) (arg-2);

                // XXX use some load-multiple action here from our const pool?
                for (int k = 0; k < 2; k++) {
                    if (r != UnknownReg) {
                        asm_ld_imm(r, *p++);
                        r = nextreg(r);
                        if (r == R4)
                            r = UnknownReg;
                    } else {
                        STR_preindex(IP, SP, -4);
                        asm_ld_imm(IP, *p++);
                        stkd += 4;
                    }
                }
            } else {
                int d = findMemFor(arg);

                for (int k = 0; k < 2; k++) {
                    if (r != UnknownReg) {
                        LDR(r, FP, d + k*4);
                        r = nextreg(r);
                        if (r == R4)
                            r = UnknownReg;
                    } else {
                        STR_preindex(IP, SP, -4);
                        LDR(IP, FP, d + k*4);
                        stkd += 4;
                    }
                }
            }
        } else {
            // handle the VFP with-register case
            Register sr = argRes->reg;
            if (r != UnknownReg && r < R3) {
                FMRRD(r, nextreg(r), sr);

                // make sure the next register is correct on return
                if (r == R0)
                    r = R2;
                else
                    r = UnknownReg;
            } else if (r == R3) {
                // legacy ABI only
                STR_preindex(IP, SP, -4);
                FMRDL(IP, sr);
                FMRDH(r, sr);
                stkd += 4;

                r = UnknownReg;
            } else {
                FSTD(sr, SP, 0);
                SUB(SP, SP, 8);
                stkd += 8;
                r = UnknownReg;
            }
        }
    } else if (sz == ARGSIZE_LO) {
        if (r != UnknownReg) {
            if (arg->isconst()) {
                asm_ld_imm(r, arg->constval());
            } else {
                Reservation* argRes = getresv(arg);
                if (argRes) {
                    if (argRes->reg == UnknownReg) {
                        // load it into the arg reg
                        int d = findMemFor(arg);
                        if (arg->isop(LIR_alloc)) {
                            asm_add_imm(r, FP, d);
                        } else {
                            LDR(r, FP, d);
                        }
                    } else {
                        MOV(r, argRes->reg);
                    }
                } else {
                    findSpecificRegFor(arg, r);
                }
            }

            if (r < R3)
                r = nextreg(r);
            else
                r = UnknownReg;
        } else {
            int d = findMemFor(arg);
            STR_preindex(IP, SP, -4);
            LDR(IP, FP, d);
            stkd += 4;
        }
    } else {
        NanoAssert(0);
    }
}

void
Assembler::asm_call(LInsp ins)
{
    const CallInfo* call = ins->callInfo();
    Reservation *callRes = getresv(ins);

    uint32_t atypes = call->_argtypes;
    uint32_t roffset = 0;

    // skip return type
    ArgSize rsize = (ArgSize)(atypes & 3);

    atypes >>= 2;

    // if we're using VFP, and the return type is a double,
    // it'll come back in R0/R1.  We need to either place it
    // in the result fp reg, or store it.

    if (AvmCore::config.vfp && rsize == ARGSIZE_F) {
        NanoAssert(ins->opcode() == LIR_fcall);
        NanoAssert(callRes);

        Register rr = callRes->reg;
        int d = disp(callRes);
        freeRsrcOf(ins, rr != UnknownReg);

        if (rr != UnknownReg) {
            NanoAssert(IsFpReg(rr));
            FMDRR(rr,R0,R1);
        } else {
            NanoAssert(d);
            STR(R0, FP, d+0);
            STR(R1, FP, d+4);
        }
    }

    // make the call
    BL((NIns*)(call->_address));

    ArgSize sizes[MAXARGS];
    uint32_t argc = call->get_sizes(sizes);

    Register r = R0;
    int stkd = 0;

    // XXX TODO we should go through the args and figure out how much
    // stack space we'll need, allocate it up front, and then do
    // SP-relative stores using stkd instead of doing STR_preindex for
    // every stack write like we currently do in asm_arg.

    for(uint32_t i = 0; i < argc; i++) {
        uint32_t j = argc - i - 1;
        ArgSize sz = sizes[j];
        LInsp arg = ins->arg(j);

        NanoAssert(r < R4 || r == UnknownReg);

#ifdef NJ_ARM_EABI
        if (sz == ARGSIZE_F) {
            if (r == R1)
                r = R2;
            else if (r == R3)
                r = UnknownReg;
        }
#endif

        asm_arg(sz, arg, r, stkd);
    }
}

void
Assembler::nMarkExecute(Page* page, int flags)
{
    NanoAssert(sizeof(Page) == NJ_PAGE_SIZE);
#ifdef UNDER_CE
    static const DWORD kProtFlags[4] = {
        PAGE_READONLY,          // 0
        PAGE_READWRITE,         // PAGE_WRITE
        PAGE_EXECUTE_READ,      // PAGE_EXEC
        PAGE_EXECUTE_READWRITE  // PAGE_EXEC|PAGE_WRITE
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

Register
Assembler::nRegisterAllocFromSet(int set)
{
    // Note: The clz instruction only works on armv5 and up.
#if defined(UNDER_CE)
    Register r;
    r = (Register)_CountLeadingZeros(set);
    r = (Register)(31-r);
    _allocator.free &= ~rmask(r);
    return r;
#elif defined(__ARMCC__)
    register int i;
    __asm { clz i,set }
    Register r = Register(31-i);
    _allocator.free &= ~rmask(r);
    return r;
#else
    // need to implement faster way
    int i=0;
    while (!(set & rmask((Register)i)))
        i ++;
    _allocator.free &= ~rmask((Register)i);
    return (Register) i;
#endif
}

void
Assembler::nRegisterResetAll(RegAlloc& a)
{
    // add scratch registers to our free list for the allocator
    a.clear();
    a.used = 0;
    a.free =
        rmask(R0) | rmask(R1) | rmask(R2) | rmask(R3) | rmask(R4) |
        rmask(R5) | rmask(R6) | rmask(R7) | rmask(R8) | rmask(R9) |
        rmask(R10);
    if (AvmCore::config.vfp)
        a.free |= FpRegs;

    debug_only(a.managed = a.free);
}

NIns*
Assembler::nPatchBranch(NIns* at, NIns* target)
{
    // Patch the jump in a loop, as emitted by JMP_far.
    // Figure out which, and do the right thing.

    NIns* was = 0;

    if (at[0] == (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | (4) )) {
        // this needed to be emitted with a 32-bit immediate.
        was = (NIns*) at[1];
    } else {
        // nope, just a regular PC-relative B; calculate the destination address
        // based on at and the offset.
        NanoAssert((at[0] & 0xff000000) == (COND_AL | (0xA<<24)));
        was = (NIns*) (((intptr_t)at + 8) + (intptr_t)((at[0] & 0xffffff) << 2));
    }

    // let's see how we have to emit it
    intptr_t offs = PC_OFFSET_FROM(target, at);
    if (isS24(offs>>2)) {
        // great, just stick it in at[0]
        at[0] = (NIns)( COND_AL | (0xA<<24) | ((offs >> 2) & 0xffffff) );
        // and reset at[1] for good measure
        at[1] = BKPT_insn;
    } else {
        at[0] = (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | (4) );
        at[1] = (NIns)(target);
    }
    VALGRIND_DISCARD_TRANSLATIONS(at, 2*sizeof(NIns));

#if defined(UNDER_CE)
    // we changed the code, so we need to do this (sadly)
    FlushInstructionCache(GetCurrentProcess(), NULL, NULL);
#elif defined(AVMPLUS_LINUX)
    __clear_cache((char*)at, (char*)(at+3));
#endif

#ifdef AVMPLUS_PORTING_API
    NanoJIT_PortAPI_FlushInstructionCache(at, at+3);
#endif

    return was;
}

RegisterMask
Assembler::hint(LIns* i, RegisterMask allow /* = ~0 */)
{
    uint32_t op = i->opcode();
    int prefer = ~0;

    if (op==LIR_call || op==LIR_fcall)
        prefer = rmask(R0);
    else if (op == LIR_callh)
        prefer = rmask(R1);
    else if (op == LIR_param)
        prefer = rmask(imm2register(i->imm8()));

    if (_allocator.free & allow & prefer)
        allow &= prefer;
    return allow;
}

void
Assembler::asm_qjoin(LIns *ins)
{
    int d = findMemFor(ins);
    NanoAssert(d);
    LIns* lo = ins->oprnd1();
    LIns* hi = ins->oprnd2();

    Register r = findRegFor(hi, GpRegs);
    STR(r, FP, d+4);

    // okay if r gets recycled.
    r = findRegFor(lo, GpRegs);
    STR(r, FP, d);
    freeRsrcOf(ins, false); // if we had a reg in use, emit a ST to flush it to mem
}

void
Assembler::asm_store32(LIns *value, int dr, LIns *base)
{
    Reservation *rA, *rB;
    Register ra, rb;
    if (base->isop(LIR_alloc)) {
        rb = FP;
        dr += findMemFor(base);
        ra = findRegFor(value, GpRegs);
    } else {
        findRegFor2(GpRegs, value, rA, base, rB);
        ra = rA->reg;
        rb = rB->reg;
    }
    STR(ra, rb, dr);
}

void
Assembler::asm_restore(LInsp i, Reservation *resv, Register r)
{
    if (i->isop(LIR_alloc)) {
        asm_add_imm(r, FP, disp(resv));
    }
#if 0
    /* This seriously regresses crypto-aes (by about 50%!), with or
     * without the S8/U8 check (which ensures that we can do this
     * const load in one instruction).  I have no idea why, because a
     * microbenchmark of const mov vs. loading from memory shows that
     * the mov is faster, though not by much.
     */
    else if (i->isconst() && (isS8(i->constval()) || isU8(i->constval()))) {
        if (!resv->arIndex)
            reserveFree(i);
        asm_ld_imm(r, i->constval());
    }
#endif
    else {
        int d = findMemFor(i);
        if (IsFpReg(r)) {
            if (isS8(d >> 2)) {
                FLDD(r, FP, d);
            } else {
                FLDD(r, IP, 0);
                ADDi(IP, FP, d);
            }
        } else {
            LDR(r, FP, d);
        }
    }

    verbose_only(
        if (_verbose)
            outputf("        restore %s",_thisfrag->lirbuf->names->formatRef(i));
    )
}

void
Assembler::asm_spill(Register rr, int d, bool pop, bool quad)
{
    (void) pop;
    (void) quad;
    if (d) {
        if (IsFpReg(rr)) {
            if (isS8(d >> 2)) {
                FSTD(rr, FP, d);
            } else {
                FSTD(rr, IP, 0);
                ADDi(IP, FP, d);
            }
        } else {
            STR(rr, FP, d);
        }
    }
}

void
Assembler::asm_load64(LInsp ins)
{
    ///asm_output("<<< load64");

    LIns* base = ins->oprnd1();
    int offset = ins->oprnd2()->constval();

    Reservation *resv = getresv(ins);
    Register rr = resv->reg;
    int d = disp(resv);

    freeRsrcOf(ins, false);

    if (AvmCore::config.vfp) {
        Register rb = findRegFor(base, GpRegs);

        NanoAssert(rb != UnknownReg);
        NanoAssert(rr == UnknownReg || IsFpReg(rr));

        if (rr != UnknownReg) {
            if (!isS8(offset >> 2) || (offset&3) != 0) {
                FLDD(rr,IP,0);
                ADDi(IP, rb, offset);
            } else {
                FLDD(rr,rb,offset);
            }
        } else {
            asm_mmq(FP, d, rb, offset);
        }

        // *(FP+dr) <- *(rb+db)
    } else {
        NanoAssert(resv->reg == UnknownReg && d != 0);
        Register rb = findRegFor(base, GpRegs);
        asm_mmq(FP, d, rb, offset);
    }

    //asm_output(">>> load64");
}

void
Assembler::asm_store64(LInsp value, int dr, LInsp base)
{
    //asm_output("<<< store64 (dr: %d)", dr);

    if (AvmCore::config.vfp) {
        //Reservation *valResv = getresv(value);
        Register rb = findRegFor(base, GpRegs);

        if (value->isconstq()) {
            const int32_t* p = (const int32_t*) (value-2);

            underrunProtect(LD32_size*2 + 8);

            // XXX use another reg, get rid of dependency
            STR(IP, rb, dr);
            LD32_nochk(IP, p[0]);
            STR(IP, rb, dr+4);
            LD32_nochk(IP, p[1]);

            return;
        }

        Register rv = findRegFor(value, FpRegs);

        NanoAssert(rb != UnknownReg);
        NanoAssert(rv != UnknownReg);

        Register baseReg = rb;
        intptr_t baseOffset = dr;

        if (!isS8(dr)) {
            baseReg = IP;
            baseOffset = 0;
        }

        FSTD(rv, baseReg, baseOffset);

        if (!isS8(dr)) {
            ADDi(IP, rb, dr);
        }

        // if it's a constant, make sure our baseReg/baseOffset location
        // has the right value
        if (value->isconstq()) {
            const int32_t* p = (const int32_t*) (value-2);

            underrunProtect(4*4);
            asm_quad_nochk(rv, p);
        }
    } else {
        int da = findMemFor(value);
        Register rb = findRegFor(base, GpRegs);
        asm_mmq(rb, dr, FP, da);
    }

    //asm_output(">>> store64");
}

// stick a quad into register rr, where p points to the two
// 32-bit parts of the quad, optinally also storing at FP+d
void
Assembler::asm_quad_nochk(Register rr, const int32_t* p)
{
    // We're not going to use a slot, because it might be too far
    // away.  Instead, we're going to stick a branch in the stream to
    // jump over the constants, and then load from a short PC relative
    // offset.

    // stream should look like:
    //    branch A
    //    p[0]
    //    p[1]
    // A: FLDD PC-16

    FLDD(rr, PC, -16);

    *(--_nIns) = (NIns) p[1];
    *(--_nIns) = (NIns) p[0];

    JMP_nochk(_nIns+2);
}

void
Assembler::asm_quad(LInsp ins)
{
    //asm_output(">>> asm_quad");

    Reservation *res = getresv(ins);
    int d = disp(res);
    Register rr = res->reg;

    NanoAssert(d || rr != UnknownReg);

    const int32_t* p = (const int32_t*) (ins-2);

    freeRsrcOf(ins, false);

    if (AvmCore::config.vfp &&
        rr != UnknownReg)
    {
        if (d)
            FSTD(rr, FP, d);

        underrunProtect(4*4);
        asm_quad_nochk(rr, p);
    } else {
        STR(IP, FP, d+4);
        asm_ld_imm(IP, p[1]);
        STR(IP, FP, d);
        asm_ld_imm(IP, p[0]);
    }

    //asm_output("<<< asm_quad");
}

void
Assembler::asm_nongp_copy(Register r, Register s)
{
    if ((rmask(r) & FpRegs) && (rmask(s) & FpRegs)) {
        // fp->fp
        FCPYD(r, s);
    } else if ((rmask(r) & GpRegs) && (rmask(s) & FpRegs)) {
        // fp->gp
        // who's doing this and why?
        NanoAssert(0);
        // FMRS(r, loSingleVfp(s));
    } else {
        NanoAssert(0);
    }
}

Register
Assembler::asm_binop_rhs_reg(LInsp)
{
    return UnknownReg;
}

/**
 * copy 64 bits: (rd+dd) <- (rs+ds)
 */
void
Assembler::asm_mmq(Register rd, int dd, Register rs, int ds)
{
    // value is either a 64bit struct or maybe a float
    // that isn't live in an FPU reg.  Either way, don't
    // put it in an FPU reg just to load & store it.

    // Don't use this with PC-relative loads; the registerAlloc might
    // end up spilling a reg (and thus the offset could end up being
    // bogus)!
    NanoAssert(rs != PC);

    // use both IP and a second scratch reg
    Register t = registerAlloc(GpRegs & ~(rmask(rd)|rmask(rs)));
    _allocator.addFree(t);

    // XXX maybe figure out if we can use LDRD/STRD -- hard to
    // ensure right register allocation
    STR(IP, rd, dd+4);
    STR(t, rd, dd);
    LDR(IP, rs, ds+4);
    LDR(t, rs, ds);
}

void
Assembler::nativePageReset()
{
    _nSlot = 0;
    _nExitSlot = 0;
}

void
Assembler::nativePageSetup()
{
    if (!_nIns)      _nIns     = pageAlloc();
    if (!_nExitIns)  _nExitIns = pageAlloc(true);
    //fprintf(stderr, "assemble onto %x exits into %x\n", (int)_nIns, (int)_nExitIns);

    if (!_nSlot)
    {
        // This needs to be done or the samepage macro gets confused; pageAlloc
        // gives us a pointer to just past the end of the page.
        _nIns--;
        _nExitIns--;

        // constpool starts at top of page and goes down,
        // code starts at bottom of page and moves up
        _nSlot = pageDataStart(_nIns); //(int*)(&((Page*)pageTop(_nIns))->lir[0]);
    }
}

// Note: underrunProtect should not touch any registers, even IP; it
// might need to allocate a new page in the middle of an IP-using
// sequence.
void
Assembler::underrunProtect(int bytes)
{
    NanoAssertMsg(bytes<=LARGEST_UNDERRUN_PROT, "constant LARGEST_UNDERRUN_PROT is too small"); 
    intptr_t u = bytes + sizeof(PageHeader)/sizeof(NIns) + 8;
    if ( (samepage(_nIns,_nSlot) && (((intptr_t)_nIns-u) <= intptr_t(_nSlot+1))) ||
         (!samepage((intptr_t)_nIns-u,_nIns)) )
    {
        NIns* target = _nIns;

        _nIns = pageAlloc(_inExit);

        // XXX _nIns at this point points to one past the end of
        // the page, intended to be written into using *(--_nIns).
        // However, (guess) something seems to be storing the value
        // of _nIns as is, and then later generating a jump to a bogus
        // address.  So pre-decrement to ensure that it's always
        // valid; we end up skipping using the last instruction this
        // way.
        _nIns--;

        // Update slot, either to _nIns (if decremented above), or
        // _nIns-1 once the above bug is fixed/found.
        _nSlot = pageDataStart(_nIns);

        // If samepage() is used on _nIns and _nSlot, it'll fail, since _nIns
        // points to one past the end of the page right now.  Assume that
        // JMP_nochk won't ever try to write to _nSlot, and so won't ever
        // check samepage().  See B_cond_chk macro.
        JMP_nochk(target);
    } else if (!_nSlot) {
        // make sure that there's always a slot pointer
        _nSlot = pageDataStart(_nIns);
    }
}

void
Assembler::JMP_far(NIns* addr)
{
    // we may have to stick an immediate into the stream, so always
    // reserve space
    underrunProtect(8);

    intptr_t offs = PC_OFFSET_FROM(addr,_nIns-2);

    if (isS24(offs>>2)) {
        BKPT_nochk();
        *(--_nIns) = (NIns)( COND_AL | (0xA<<24) | ((offs>>2) & 0xFFFFFF) );

        asm_output("b %p", addr);
    } else {
        // the address
        *(--_nIns) = (NIns)((addr));
        // ldr pc, [pc - #4] // load the address into pc, reading it from [pc-4] (e.g.,
        // the next instruction)
        *(--_nIns) = (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | (4));

        asm_output("b %p (32-bit)", addr);
    }
}

void
Assembler::BL(NIns* addr)
{
    intptr_t offs = PC_OFFSET_FROM(addr,_nIns-1);

    //fprintf (stderr, "BL: 0x%x (offs: %d [%x]) @ 0x%08x\n", addr, offs, offs, (intptr_t)(_nIns-1));

    // try to do this with a single S24 call
    if (isS24(offs>>2)) {
        underrunProtect(4);

        // recompute offset in case underrunProtect had to allocate a new page.
        offs = PC_OFFSET_FROM(addr,_nIns-1);
        *(--_nIns) = (NIns)( COND_AL | (0xB<<24) | ((offs>>2) & 0xFFFFFF) );

        asm_output("bl %p", addr);
    } else {
        underrunProtect(12);

        // the address
        *(--_nIns) = (NIns)((addr));
        // ldr pc, [pc - #4] // load the address into ip, reading it from [pc-4]
        *(--_nIns) = (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | (4));
        // add lr, pc, #4    // set lr to be past the address that we wrote
        *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | (PC<<16) | (LR<<12) | (4) );

        asm_output("bl %p (32-bit)", addr);
    }
}

void
Assembler::LD32_nochk(Register r, int32_t imm)
{
    if (imm == 0) {
        EOR(r, r, r);
        return;
    }

    if (AvmCore::config.v6t2) {
        // We can just emit a movw/movt pair
        // the movt is only necessary if the high 16 bits are nonzero
        if (((imm >> 16) & 0xFFFF) != 0)
            MOVT(r, (imm >> 16) & 0xFFFF);
        MOVW(r, imm & 0xFFFF);
        return;
    }

    // We should always reach the const pool, since it's on the same page (<4096);
    // if we can't, someone didn't underrunProtect enough.

    *(++_nSlot) = (int)imm;

    //fprintf (stderr, "wrote slot(2) %p with %08x, jmp @ %p\n", _nSlot, (intptr_t)imm, _nIns-1);

    int offset = PC_OFFSET_FROM(_nSlot,_nIns-1);

    NanoAssert(isS12(offset) && (offset < 0));

    asm_output("  (%d(PC) = 0x%x)", offset, imm);

    LDR_nochk(r,PC,offset);
}

void
Assembler::asm_ldr_chk(Register d, Register b, int32_t off, bool chk)
{
    if (IsFpReg(d)) {
        FLDD_chk(d,b,off,chk);
        return;
    }

    if (off > -4096 && off < 4096) {
        if (chk) underrunProtect(4);
        *(--_nIns) = (NIns)( COND_AL | ((off < 0 ? 0x51 : 0x59)<<20) | (b<<16) | (d<<12) | ((off < 0 ? -off : off)&0xFFF) );
    } else {
        if (chk) underrunProtect(4+LD32_size);
        NanoAssert(b != IP);
        *(--_nIns) = (NIns)( COND_AL | (0x79<<20) | (b<<16) | (d<<12) | IP );
        LD32_nochk(IP, off);
    }

    asm_output("ldr %s, [%s, #%d]",gpn(d),gpn(b),(off));
}

void
Assembler::asm_ld_imm(Register d, int32_t imm)
{
    if (imm == 0) {
        EOR(d, d, d);
    } else if (isS8(imm) || isU8(imm)) {
        underrunProtect(4);
        if (imm < 0)
            *(--_nIns) = (NIns)( COND_AL | 0x3E<<20 | d<<12 | (imm^0xFFFFFFFF)&0xFF );
        else
            *(--_nIns) = (NIns)( COND_AL | 0x3B<<20 | d<<12 | imm&0xFF );
        asm_output("ld  %s,0x%x",gpn(d), imm);
    } else {
        underrunProtect(LD32_size);
        LD32_nochk(d, imm);
    }
}

// Branch to target address _t with condition _c, doing underrun
// checks (_chk == 1) or skipping them (_chk == 0).
//
// If the jump fits in a relative jump (+/-32MB), emit that.
// If the jump is unconditional, emit the dest address inline in
// the instruction stream and load it into pc.
// If the jump has a condition, but noone's mucked with _nIns and our _nSlot
// pointer is valid, stick the constant in the slot and emit a conditional
// load into pc.
// Otherwise, emit the conditional load into pc from a nearby constant,
// and emit a jump to jump over it it in case the condition fails.
//
// NB: JMP_nochk depends on this not calling samepage() when _c == AL
void
Assembler::B_cond_chk(ConditionCode _c, NIns* _t, bool _chk)
{
    int32_t offs = PC_OFFSET_FROM(_t,_nIns-1);
    //fprintf(stderr, "B_cond_chk target: 0x%08x offset: %d @0x%08x\n", _t, offs, _nIns-1);

    // optimistically check if this will fit in 24 bits
    if (isS24(offs>>2)) {
        if (_chk) underrunProtect(4);
        // recalculate the offset, because underrunProtect may have
        // moved _nIns to a new page
        offs = PC_OFFSET_FROM(_t,_nIns-1);
    }

    if (isS24(offs>>2)) {
        // the underrunProtect for this was done above
        *(--_nIns) = (NIns)( ((_c)<<28) | (0xA<<24) | (((offs)>>2) & 0xFFFFFF) );
    } else if (_c == AL) {
        if(_chk) underrunProtect(8);
        *(--_nIns) = (NIns)(_t);
        *(--_nIns) = (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | 0x4 );
    } else if (samepage(_nIns,_nSlot)) {
        if(_chk) underrunProtect(8);
        *(++_nSlot) = (NIns)(_t);
        offs = PC_OFFSET_FROM(_nSlot,_nIns-1);
        NanoAssert(offs < 0);
        *(--_nIns) = (NIns)( ((_c)<<28) | (0x51<<20) | (PC<<16) | (PC<<12) | ((-offs) & 0xFFFFFF) );
    } else {
        if(_chk) underrunProtect(12);
        *(--_nIns) = (NIns)(_t);
        *(--_nIns) = (NIns)( COND_AL | (0xA<<24) | ((-4)>>2) & 0xFFFFFF );
        *(--_nIns) = (NIns)( ((_c)<<28) | (0x51<<20) | (PC<<16) | (PC<<12) | 0x0 );
    }

    asm_output("b%s %p", condNames[_c], (void*)(_t));
}

void
Assembler::asm_add_imm(Register rd, Register rn, int32_t imm, int stat)
{
    int rot = 16;
    uint32_t immval;
    bool pos;

    if (imm >= 0) {
        immval = (uint32_t) imm;
        pos = true;
    } else {
        immval = (uint32_t) (-imm);
        pos = false;
    }

    while (immval > 255 &&
           immval && ((immval & 0x3) == 0))
    {
        immval >>= 2;
        rot--;
    }

    rot &= 0xf;

    if (immval < 256) {
        if (pos) {
            ALUi_rot(AL, add, stat, rd, rn, immval, rot);
        } else {
            ALUi_rot(AL, sub, stat, rd, rn, immval, rot);
        }
   } else {
        // add scratch to rn, after loading the value into scratch.
        // make sure someone isn't trying to use IP as an operand
        NanoAssert(rn != IP);
        ALUr(AL, add, stat, rd, rn, IP);
        asm_ld_imm(IP, imm);
    }
}

void
Assembler::asm_sub_imm(Register rd, Register rn, int32_t imm, int stat)
{
    if (imm > -256 && imm < 256) {
        if (imm >= 0)
            ALUi(AL, sub, stat, rd, rn, imm);
        else
            ALUi(AL, add, stat, rd, rn, -imm);
    } else if (imm >= 0) {
        if (imm <= 510) {
            /* between 0 and 510, inclusive */
            int rem = imm - 255;
            NanoAssert(rem < 256);
            ALUi(AL, sub, stat, rd, rn, rem & 0xff);
            ALUi(AL, sub, stat, rd, rn, 0xff);
        } else {
            /* more than 510 */
            NanoAssert(rn != IP);
            ALUr(AL, sub, stat, rd, rn, IP);
            asm_ld_imm(IP, imm);
        }
    } else {
        if (imm >= -510) {
            /* between -510 and -1, inclusive */
            int rem = -imm - 255;
            ALUi(AL, add, stat, rd, rn, rem & 0xff);
            ALUi(AL, add, stat, rd, rn, 0xff);
        } else {
            /* less than -510 */
            NanoAssert(rn != IP);
            ALUr(AL, add, stat, rd, rn, IP);
            asm_ld_imm(IP, -imm);
        }
    }
}

/*
 * VFP
 */

void
Assembler::asm_i2f(LInsp ins)
{
    Register rr = prepResultReg(ins, FpRegs);
    Register srcr = findRegFor(ins->oprnd1(), GpRegs);

    // todo: support int value in memory, as per x86
    NanoAssert(srcr != UnknownReg);

    FSITOD(rr, FpSingleScratch);
    FMSR(FpSingleScratch, srcr);
}

void
Assembler::asm_u2f(LInsp ins)
{
    Register rr = prepResultReg(ins, FpRegs);
    Register sr = findRegFor(ins->oprnd1(), GpRegs);

    // todo: support int value in memory, as per x86
    NanoAssert(sr != UnknownReg);

    FUITOD(rr, FpSingleScratch);
    FMSR(FpSingleScratch, sr);
}

void
Assembler::asm_fneg(LInsp ins)
{
    LInsp lhs = ins->oprnd1();
    Register rr = prepResultReg(ins, FpRegs);

    Reservation* rA = getresv(lhs);
    Register sr;

    if (!rA || rA->reg == UnknownReg)
        sr = findRegFor(lhs, FpRegs);
    else
        sr = rA->reg;

    FNEGD(rr, sr);
}

void
Assembler::asm_fop(LInsp ins)
{
    LInsp lhs = ins->oprnd1();
    LInsp rhs = ins->oprnd2();
    LOpcode op = ins->opcode();

    NanoAssert(op >= LIR_fadd && op <= LIR_fdiv);

    // rr = ra OP rb

    Register rr = prepResultReg(ins, FpRegs);

    Register ra = findRegFor(lhs, FpRegs);
    Register rb = (rhs == lhs) ? ra : findRegFor(rhs, FpRegs);

    // XXX special-case 1.0 and 0.0

    if (op == LIR_fadd)
        FADDD(rr,ra,rb);
    else if (op == LIR_fsub)
        FSUBD(rr,ra,rb);
    else if (op == LIR_fmul)
        FMULD(rr,ra,rb);
    else //if (op == LIR_fdiv)
        FDIVD(rr,ra,rb);
}

void
Assembler::asm_fcmp(LInsp ins)
{
    LInsp lhs = ins->oprnd1();
    LInsp rhs = ins->oprnd2();
    LOpcode op = ins->opcode();

    NanoAssert(op >= LIR_feq && op <= LIR_fge);

    Register ra = findRegFor(lhs, FpRegs);
    Register rb = findRegFor(rhs, FpRegs);

    FMSTAT();
    FCMPD(ra, rb);
}

Register
Assembler::asm_prep_fcall(Reservation*, LInsp)
{
    // We have nothing to do here; we do it all in asm_call.
    return UnknownReg;
}

NIns*
Assembler::asm_branch(bool branchOnFalse, LInsp cond, NIns* targ, bool isfar)
{
    // ignore isfar -- we figure this out on our own.
    // XXX noone actually uses the far param in nj anyway... (always false)
    (void)isfar;

    NIns* at = 0;
    LOpcode condop = cond->opcode();
    NanoAssert(cond->isCond());

    if (condop >= LIR_feq && condop <= LIR_fge)
    {
        ConditionCode cc = NV;

        if (branchOnFalse) {
            switch (condop) {
                case LIR_feq: cc = NE; break;
                case LIR_flt: cc = PL; break;
                case LIR_fgt: cc = LE; break;
                case LIR_fle: cc = HI; break;
                case LIR_fge: cc = LT; break;
                default: NanoAssert(0); break;
            }
        } else {
            switch (condop) {
                case LIR_feq: cc = EQ; break;
                case LIR_flt: cc = MI; break;
                case LIR_fgt: cc = GT; break;
                case LIR_fle: cc = LS; break;
                case LIR_fge: cc = GE; break;
                default: NanoAssert(0); break;
            }
        }

        B_cond(cc, targ);
        asm_output("b(%d) 0x%08x", cc, (unsigned int) targ);

        NIns *at = _nIns;
        asm_fcmp(cond);
        return at;
    }

    // produce the branch
    if (branchOnFalse) {
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
    } else // op == LIR_xt
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

void
Assembler::asm_cmp(LIns *cond)
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
    if (rhs->isconst()) {
        int c = rhs->constval();
        if (c == 0 && cond->isop(LIR_eq)) {
            Register r = findRegFor(lhs, GpRegs);
            TEST(r,r);
            // No 64-bit immediates so fall-back to below
        } else if (!rhs->isQuad()) {
            Register r = getBaseReg(lhs, c, GpRegs);
            asm_cmpi(r, c);
        } else {
            NanoAssert(0);
        }
    } else {
        findRegFor2(GpRegs, lhs, rA, rhs, rB);
        Register ra = rA->reg;
        Register rb = rB->reg;
        CMP(ra, rb);
    }
}

void
Assembler::asm_cmpi(Register r, int32_t imm)
{
    if (imm < 0) {
        if (imm > -256) {
            ALUi(AL, cmn, 1, 0, r, -imm);
        } else {
            CMP(r, IP);
            asm_ld_imm(IP, imm);
        }
    } else {
        if (imm < 256) {
            ALUi(AL, cmp, 1, 0, r, imm);
        } else {
            CMP(r, IP);
            asm_ld_imm(IP, imm);
        }
    }
}

void
Assembler::asm_loop(LInsp ins, NInsList& loopJumps)
{
    // XXX asm_loop should be in Assembler.cpp!

    JMP_far(0);
    loopJumps.add(_nIns);

    // If the target we are looping to is in a different fragment, we have to restore
    // SP since we will target fragEntry and not loopEntry.
    if (ins->record()->exit->target != _thisfrag)
        MOV(SP,FP);
}

void
Assembler::asm_fcond(LInsp ins)
{
    // only want certain regs
    Register r = prepResultReg(ins, AllowableFlagRegs);

    switch (ins->opcode()) {
        case LIR_feq: SET(r,EQ,NE); break;
        case LIR_flt: SET(r,MI,PL); break;
        case LIR_fgt: SET(r,GT,LE); break;
        case LIR_fle: SET(r,LS,HI); break;
        case LIR_fge: SET(r,GE,LT); break;
        default: NanoAssert(0); break;
    }

    asm_fcmp(ins);
}

void
Assembler::asm_cond(LInsp ins)
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

void
Assembler::asm_arith(LInsp ins)
{
    LOpcode op = ins->opcode();
    LInsp lhs = ins->oprnd1();
    LInsp rhs = ins->oprnd2();

    Register rb = UnknownReg;
    RegisterMask allow = GpRegs;
    bool forceReg = (op == LIR_mul || !rhs->isconst());

    // Arm can't do an immediate op with immediates
    // outside of +/-255 (for AND) r outside of
    // 0..255 for others.
    if (!forceReg) {
        if (rhs->isconst() && !isU8(rhs->constval()))
            forceReg = true;
    }

    if (lhs != rhs && forceReg) {
        if ((rb = asm_binop_rhs_reg(ins)) == UnknownReg) {
            rb = findRegFor(rhs, allow);
        }
        allow &= ~rmask(rb);
    } else if ((op == LIR_add||op == LIR_addp) && lhs->isop(LIR_alloc) && rhs->isconst()) {
        // add alloc+const, rr wants the address of the allocated space plus a constant
        Register rr = prepResultReg(ins, allow);
        int d = findMemFor(lhs) + rhs->constval();
        asm_add_imm(rr, FP, d);
    }

    Register rr = prepResultReg(ins, allow);
    Reservation* rA = getresv(lhs);
    Register ra;
    // if this is last use of lhs in reg, we can re-use result reg
    if (rA == 0 || (ra = rA->reg) == UnknownReg)
        ra = findSpecificRegFor(lhs, rr);
    // else, rA already has a register assigned.
    NanoAssert(ra != UnknownReg);

    if (forceReg) {
        if (lhs == rhs)
            rb = ra;

        if (op == LIR_add || op == LIR_addp)
            ADDs(rr, ra, rb, 1);
        else if (op == LIR_sub)
            SUB(rr, ra, rb);
        else if (op == LIR_mul)
            MUL(rr, rb);
        else if (op == LIR_and)
            AND(rr, ra, rb);
        else if (op == LIR_or)
            ORR(rr, ra, rb);
        else if (op == LIR_xor)
            EOR(rr, ra, rb);
        else if (op == LIR_lsh)
            SHL(rr, ra, rb);
        else if (op == LIR_rsh)
            SAR(rr, ra, rb);
        else if (op == LIR_ush)
            SHR(rr, ra, rb);
        else
            NanoAssertMsg(0, "Unsupported");
    } else {
        int c = rhs->constval();
        if (op == LIR_add || op == LIR_addp)
            ADDi(rr, ra, c);
        else if (op == LIR_sub)
            SUBi(rr, ra, c);
        else if (op == LIR_and)
            ANDi(rr, ra, c);
        else if (op == LIR_or)
            ORRi(rr, ra, c);
        else if (op == LIR_xor)
            EORi(rr, ra, c);
        else if (op == LIR_lsh)
            SHLi(rr, ra, c);
        else if (op == LIR_rsh)
            SARi(rr, ra, c);
        else if (op == LIR_ush)
            SHRi(rr, ra, c);
        else
            NanoAssertMsg(0, "Unsupported");
    }
}

void
Assembler::asm_neg_not(LInsp ins)
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
    NanoAssert(ra != UnknownReg);

    if (op == LIR_not)
        MVN(rr, ra);
    else
        RSBS(rr, ra);
}

void
Assembler::asm_ld(LInsp ins)
{
    LOpcode op = ins->opcode();
    LIns* base = ins->oprnd1();
    LIns* disp = ins->oprnd2();
    Register rr = prepResultReg(ins, GpRegs);
    int d = disp->constval();
    Register ra = getBaseReg(base, d, GpRegs);

    // these will always be 4-byte aligned
    if (op == LIR_ld || op == LIR_ldc) {
        LD(rr, d, ra);
        return;
    }

    // these will be 2 or 4-byte aligned
    if (op == LIR_ldcs) {
        LDRH(rr, d, ra);
        return;
    }

    // aaand this is just any byte.
    if (op == LIR_ldcb) {
        LDRB(rr, d, ra);
        return;
    }

    NanoAssertMsg(0, "Unsupported instruction in asm_ld");
}

void
Assembler::asm_cmov(LInsp ins)
{
    NanoAssert(ins->opcode() == LIR_cmov);
    LIns* condval = ins->oprnd1();
    NanoAssert(condval->isCmp());

    LIns* values = ins->oprnd2();

    NanoAssert(values->opcode() == LIR_2);
    LIns* iftrue = values->oprnd1();
    LIns* iffalse = values->oprnd2();

    NanoAssert(!iftrue->isQuad() && !iffalse->isQuad());

    const Register rr = prepResultReg(ins, GpRegs);

    // this code assumes that neither LD nor MR nor MRcc set any of the condition flags.
    // (This is true on Intel, is it true on all architectures?)
    const Register iffalsereg = findRegFor(iffalse, GpRegs & ~rmask(rr));
    switch (condval->opcode()) {
        // note that these are all opposites...
        case LIR_eq:    MOVNE(rr, iffalsereg);  break;
        case LIR_ov:    MOVVC(rr, iffalsereg);  break;
        case LIR_cs:    MOVNC(rr, iffalsereg);  break;
        case LIR_lt:    MOVGE(rr, iffalsereg);  break;
        case LIR_le:    MOVGT(rr, iffalsereg);  break;
        case LIR_gt:    MOVLE(rr, iffalsereg);  break;
        case LIR_ge:    MOVLT(rr, iffalsereg);  break;
        case LIR_ult:   MOVCS(rr, iffalsereg);  break;
        case LIR_ule:   MOVHI(rr, iffalsereg);  break;
        case LIR_ugt:   MOVLS(rr, iffalsereg);  break;
        case LIR_uge:   MOVCC(rr, iffalsereg);  break;
        default: debug_only( NanoAssert(0) );   break;
    }
    /*const Register iftruereg =*/ findSpecificRegFor(iftrue, rr);
    asm_cmp(condval);
}

void
Assembler::asm_qhi(LInsp ins)
{
    Register rr = prepResultReg(ins, GpRegs);
    LIns *q = ins->oprnd1();
    int d = findMemFor(q);
    LD(rr, d+4, FP);
}

void
Assembler::asm_qlo(LInsp ins)
{
    Register rr = prepResultReg(ins, GpRegs);
    LIns *q = ins->oprnd1();
    int d = findMemFor(q);
    LD(rr, d, FP);

#if 0
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
#endif
}


void
Assembler::asm_param(LInsp ins)
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
    } else {
        // saved param
        prepResultReg(ins, rmask(savedRegs[a]));
    }
}

void
Assembler::asm_short(LInsp ins)
{
    Register rr = prepResultReg(ins, GpRegs);
    int32_t val = ins->imm16();
    if (val == 0)
        EOR(rr,rr,rr);
    else
        LDi(rr, val);
}

void
Assembler::asm_int(LInsp ins)
{
    Register rr = prepResultReg(ins, GpRegs);
    int32_t val = ins->imm32();
    if (val == 0)
        EOR(rr,rr,rr);
    else
        LDi(rr, val);
}

}

#endif /* FEATURE_NANOJIT */
