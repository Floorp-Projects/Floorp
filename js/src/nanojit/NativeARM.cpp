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
#include <asm/unistd.h>
extern "C" void __clear_cache(char *BEG, char *END);
#endif

#ifdef FEATURE_NANOJIT

namespace nanojit
{

#ifdef NJ_VERBOSE
const char* regNames[] = {"r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r10","FP","IP","SP","LR","PC",
                          "d0","d1","d2","d3","d4","d5","d6","d7","s14"};
#endif

const Register Assembler::argRegs[] = { R0, R1, R2, R3 };
const Register Assembler::retRegs[] = { R0, R1 };

void
Assembler::nInit(AvmCore*)
{
    // all ARMs have conditional move
    has_cmov = true;
}

NIns*
Assembler::genPrologue(RegisterMask needSaving)
{
    /**
     * Prologue
     */

    // NJ_RESV_OFFSET is space at the top of the stack for us
    // to use for parameter passing (8 bytes at the moment)
    uint32_t stackNeeded = 4 * _activation.highwatermark + NJ_STACK_OFFSET;
    uint32_t savingCount = 0;

    uint32_t savingMask = 0;
    savingCount = 9; //R4-R10,R11,LR
    savingMask = SavedRegs | rmask(FRAME_PTR);
    (void)needSaving;

    // so for alignment purposes we've pushed  return addr, fp, and savingCount registers
    uint32_t stackPushed = 4 * (2+savingCount);
    uint32_t aligned = alignUp(stackNeeded + stackPushed, NJ_ALIGN_STACK);
    int32_t amt = aligned - stackPushed;

    // Make room on stack for what we are doing
    if (amt)
        SUBi(SP, amt); 

    verbose_only( verbose_outputf("         %p:",_nIns); )
    verbose_only( verbose_output("         patch entry"); )
    NIns *patchEntry = _nIns;

    MR(FRAME_PTR, SP);
    PUSH_mask(savingMask|rmask(LR));
    return patchEntry;
}

void
Assembler::nFragExit(LInsp guard)
{
    SideExit* exit = guard->exit();
    Fragment *frag = exit->target;
    GuardRecord *lr;

    if (frag && frag->fragEntry) {
        JMP(frag->fragEntry);
        lr = 0;
    } else {
        // target doesn't exit yet.  emit jump to epilog, and set up to patch later.
        lr = placeGuardRecord(guard);

        // we need to know that there's an extra immediate value available
        // for us; always force a far jump here.
        BL_far(_epilogue);

        // stick the jmp pointer to the start of the sequence
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

NIns*
Assembler::genEpilogue(RegisterMask restore)
{
    BX(LR); // return
    MR(R0,R2); // return LinkRecord*
    RegisterMask savingMask = restore | rmask(FRAME_PTR) | rmask(LR);
    POP_mask(savingMask); // regs
    return _nIns;
}
    
void
Assembler::asm_call(LInsp ins)
{
    const CallInfo* call = callInfoFor(ins->fid());
    Reservation *callRes = getresv(ins);

    uint32_t atypes = call->_argtypes;
    uint32_t roffset = 0;

    // skip return type
#ifdef NJ_ARM_VFP
    ArgSize rsize = (ArgSize)(atypes & 3);
#endif
    atypes >>= 2;

    // we need to detect if we have arg0 as LO followed by arg1 as F;
    // in that case, we need to skip using r1 -- the F needs to be
    // loaded in r2/r3, at least according to the ARM EABI and gcc 4.2's
    // generated code.
    bool arg0IsInt32FollowedByFloat = false;
    while ((atypes & 3) != ARGSIZE_NONE) {
        if (((atypes >> 2) & 3) == ARGSIZE_LO &&
            ((atypes >> 0) & 3) == ARGSIZE_F &&
            ((atypes >> 4) & 3) == ARGSIZE_NONE)
        {
            arg0IsInt32FollowedByFloat = true;
            break;
        }
        atypes >>= 2;
    }

#ifdef NJ_ARM_VFP
    if (rsize == ARGSIZE_F) {
        NanoAssert(ins->opcode() == LIR_fcall);
        NanoAssert(callRes);

        //fprintf (stderr, "call ins: %p callRes: %p reg: %d ar: %d\n", ins, callRes, callRes->reg, callRes->arIndex);

        Register rr = callRes->reg;
        int d = disp(callRes);
        freeRsrcOf(ins, rr != UnknownReg);

        if (rr != UnknownReg) {
            NanoAssert(IsFpReg(rr));
            FMDRR(rr,R0,R1);
        } else {
            NanoAssert(d);
            //fprintf (stderr, "call ins d: %d\n", d);
            STMIA(Scratch, 1<<R0 | 1<<R1);
            arm_ADDi(Scratch, FP, d);
        }
    }
#endif

    CALL(call);

    ArgSize sizes[10];
    uint32_t argc = call->get_sizes(sizes);
    for(uint32_t i = 0; i < argc; i++) {
        uint32_t j = argc - i - 1;
        ArgSize sz = sizes[j];
        LInsp arg = ins->arg(j);
        // pre-assign registers R0-R3 for arguments (if they fit)

        Register r = (i + roffset) < 4 ? argRegs[i+roffset] : UnknownReg;
#ifdef NJ_ARM_VFP
        if (sz == ARGSIZE_F) {
            if (r == R0 || r == R2) {
                roffset++;
            } else if (r == R1) {
                r = R2;
                roffset++;
            } else {
                r = UnknownReg;
            }

            // XXX move this into asm_farg
            Register sr = findRegFor(arg, FpRegs);

            if (r != UnknownReg) {
                // stick it into our scratch fp reg, and then copy into the base reg
                //fprintf (stderr, "FMRRD: %d %d <- %d\n", r, nextreg(r), sr);
                FMRRD(r, nextreg(r), sr);
            } else {
                asm_pusharg(arg);
            }
        } else {
            asm_arg(sz, arg, r);
        }
#else
        NanoAssert(sz == ARGSIZE_LO || sz == ARGSIZE_Q);
        asm_arg(sz, arg, r);
#endif

        if (i == 0 && arg0IsInt32FollowedByFloat)
            roffset = 1;
    }
}
    
void
Assembler::nMarkExecute(Page* page, int32_t count, bool enable)
{
#ifdef UNDER_CE
    DWORD dwOld;
    VirtualProtect(page, NJ_PAGE_SIZE, PAGE_EXECUTE_READWRITE, &dwOld);
#endif
#ifdef AVMPLUS_PORTING_API
    NanoJIT_PortAPI_MarkExecutable(page, (void*)((int32_t)page+count));
#endif
    (void)page;
    (void)count;
    (void)enable;
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
    a.free = rmask(R0) | rmask(R1) | rmask(R2) | rmask(R3) | rmask(R4) | rmask(R5) | FpRegs;
    debug_only(a.managed = a.free);
}

void
Assembler::nPatchBranch(NIns* branch, NIns* target)
{
    // Patch the jump in a loop

    // This is ALWAYS going to be a long branch (using the BL instruction)
    // Which is really 2 instructions, so we need to modify both
    // XXX -- this is B, not BL, at least on non-Thumb..

    int32_t offset = PC_OFFSET_FROM(target, branch);

    //printf("---patching branch at 0x%08x to location 0x%08x (%d-0x%08x)\n", branch, target, offset, offset);

    // We have 2 words to work with here -- if offset is in range of a 24-bit
    // relative jump, emit that; otherwise, we do a pc-relative load into pc.
    if (isS24(offset)) {
        // ARM goodness, using unconditional B
        *branch = (NIns)( COND_AL | (0xA<<24) | ((offset>>2) & 0xFFFFFF) );
    } else {
        // LDR pc,[pc]
        *branch++ = (NIns)( COND_AL | (0x51<<20) | (PC<<16) | (PC<<12) | ( 0x004 ) );
        *branch = (NIns)target;
    }
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
    AvmAssert(d);
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
    // make sure what is in a register
    Reservation *rA, *rB;
    findRegFor2(GpRegs, value, rA, base, rB);
    Register ra = rA->reg;
    Register rb = rB->reg;
    STR(ra, rb, dr);
}

void
Assembler::asm_restore(LInsp i, Reservation *resv, Register r)
{
    (void)resv;
    int d = findMemFor(i);

    if (IsFpReg(r)) {
        if (isS8(d >> 2)) {
            FLDD(r, FP, d);
        } else {
            FLDD(r, Scratch, 0);
            arm_ADDi(Scratch, FP, d);
        }
    } else {
        LDR(r, FP, d);
    }

    verbose_only(
        if (_verbose)
            outputf("        restore %s",_thisfrag->lirbuf->names->formatRef(i));
    )
}

void
Assembler::asm_spill(LInsp i, Reservation *resv, bool pop)
{
    (void)i;
    (void)pop;
    //fprintf (stderr, "resv->arIndex: %d\n", resv->arIndex);
    if (resv->arIndex) {
        int d = disp(resv);
        // save to spill location
        Register rr = resv->reg;
        if (IsFpReg(rr)) {
            if (isS8(d >> 2)) {
                FSTD(rr, FP, d);
            } else {
                FSTD(rr, Scratch, 0);
                arm_ADDi(Scratch, FP, d);
            }
        } else {
            STR(rr, FP, d);
        }

        verbose_only(if (_verbose){
                outputf("        spill %s",_thisfrag->lirbuf->names->formatRef(i));
            }
        )
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

#ifdef NJ_ARM_VFP
    Register rb = findRegFor(base, GpRegs);

    NanoAssert(rb != UnknownReg);
    NanoAssert(rr == UnknownReg || IsFpReg(rr));

    if (rr != UnknownReg) {
        if (!isS8(offset >> 2) || (offset&3) != 0) {
            underrunProtect(LD32_size + 8);
            FLDD(rr,Scratch,0);
            ADD(Scratch, rb);
            LD32_nochk(Scratch, offset);
        } else {
            FLDD(rr,rb,offset);
        }
    } else {
        asm_mmq(FP, d, rb, offset);
    }

    // *(FP+dr) <- *(rb+db)
#else
    NanoAssert(resv->reg == UnknownReg && d != 0);
    Register rb = findRegFor(base, GpRegs);
    asm_mmq(FP, d, rb, offset);
#endif

    //asm_output(">>> load64");
}

void
Assembler::asm_store64(LInsp value, int dr, LInsp base)
{
    //asm_output1("<<< store64 (dr: %d)", dr);

#ifdef NJ_ARM_VFP
    Reservation *valResv = getresv(value);

    Register rb = findRegFor(base, GpRegs);
    Register rv = findRegFor(value, FpRegs);

    NanoAssert(rb != UnknownReg);
    NanoAssert(rv != UnknownReg);

    Register baseReg = rb;
    intptr_t baseOffset = dr;

    if (!isS8(dr)) {
        baseReg = Scratch;
        baseOffset = 0;
    }

    FSTD(rv, baseReg, baseOffset);

    if (!isS8(dr)) {
        underrunProtect(4 + LD32_size);
        ADD(Scratch, rb);
        LD32_nochk(Scratch, dr);
    }

    // if it's a constant, make sure our baseReg/baseOffset location
    // has the right value
    if (value->isconstq()) {
        const int32_t* p = (const int32_t*) (value-2);

        underrunProtect(12 + LD32_size);

        asm_quad_nochk(rv, p);
    }
#else
    int da = findMemFor(value);
    Register rb = findRegFor(base, GpRegs);
    asm_mmq(rb, dr, FP, da);
#endif
    //asm_output(">>> store64");
}

// stick a quad into register rr, where p points to the two
// 32-bit parts of the quad, optinally also storing at FP+d
void
Assembler::asm_quad_nochk(Register rr, const int32_t* p)
{
    *(++_nSlot) = p[0];
    *(++_nSlot) = p[1];

    intptr_t constAddr = (intptr_t) (_nSlot-1);
    intptr_t realOffset = PC_OFFSET_FROM(constAddr, _nIns-1);
    intptr_t offset = realOffset;
    Register baseReg = PC;

    //int32_t *q = (int32_t*) constAddr;
    //fprintf (stderr, "asm_quad_nochk: rr = %d cAddr: 0x%x quad: %08x:%08x q: %f @0x%08x\n", rr, constAddr, p[0], p[1], *(double*)q, _nIns);

    // for FLDD, we only get a left-shifted 8-bit offset
    if (!isS8(realOffset >> 2)) {
        offset = 0;
        baseReg = Scratch;
    }

    FLDD(rr, baseReg, offset);

    if (!isS8(realOffset >> 2))
        LD32_nochk(Scratch, constAddr);
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

#ifdef NJ_ARM_VFP
    freeRsrcOf(ins, false);

    // XXX We probably want nochk versions of FLDD/FSTD
    underrunProtect(16 + LD32_size);

    // grab a register to do the load into if we don't have one already;
    // XXX -- maybe do a mmq in this case?  We're going to use our
    // D7 register that's never allocated (since it's the one we use
    // for int-to-double conversions), so we don't have to worry about
    // spilling something in a fp reg.
    if (rr == UnknownReg)
        rr = D7;

    if (d)
        FSTD(rr, FP, d);

    asm_quad_nochk(rr, p);
#else
    freeRsrcOf(ins, false);
    if (d) {
        underrunProtect(LD32_size * 2 + 8);
        STR(Scratch, FP, d+4);
        LD32_nochk(Scratch, p[1]);
        STR(Scratch, FP, d);
        LD32_nochk(Scratch, p[0]);
    }
#endif

    //asm_output("<<< asm_quad");
}

bool
Assembler::asm_qlo(LInsp ins, LInsp q)
{
    (void)ins; (void)q;
    return false;
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
Assembler::asm_binop_rhs_reg(LInsp ins)
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
    // get a scratch reg
    Register t = registerAlloc(GpRegs & ~(rmask(rd)|rmask(rs)));
    _allocator.addFree(t);
    // XXX use LDM,STM
    STR(t, rd, dd+4);
    LDR(t, rs, ds+4);
    STR(t, rd, dd);
    LDR(t, rs, ds);
}

void
Assembler::asm_pusharg(LInsp arg)
{
    Reservation* argRes = getresv(arg);
    bool quad = arg->isQuad();
    intptr_t stack_growth = quad ? 8 : 4;

    Register ra;

    if (argRes)
        ra = argRes->reg;
    else
        ra = findRegFor(arg, quad ? FpRegs : GpRegs);

    if (ra == UnknownReg) {
        STR(Scratch, SP, 0);
        LDR(Scratch, FP, disp(argRes));
    } else {
        if (!quad) {
            Register ra = findRegFor(arg, GpRegs);
            STR(ra, SP, 0);
        } else {
            Register ra = findRegFor(arg, FpRegs);
            FSTD(ra, SP, 0);
        }
    }

    SUBi(SP, stack_growth);
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

NIns*
Assembler::asm_adjustBranch(NIns* at, NIns* target)
{
    // This always got emitted as a BL_far sequence; at points
    // to the first of 4 instructions.  Ensure that we're where
    // we think we were..
    NanoAssert(at[1] == (NIns)( COND_AL | OP_IMM | (1<<23) | (PC<<16) | (LR<<12) | (4) ));
    NanoAssert(at[2] == (NIns)( COND_AL | (0x9<<21) | (0xFFF<<8) | (1<<4) | (IP) ));

    NIns* was = (NIns*) at[3];

    //fprintf (stderr, "Adjusting branch @ 0x%8x: 0x%x -> 0x%x\n", at+3, at[3], target);

    at[3] = (NIns)target;

#if defined(UNDER_CE)
    // we changed the code, so we need to do this (sadly)
    FlushInstructionCache(GetCurrentProcess(), NULL, NULL);
#elif defined(AVMPLUS_LINUX)
    __clear_cache((char*)at, (char*)(at+4));
#endif

#ifdef AVMPLUS_PORTING_API
    NanoJIT_PortAPI_FlushInstructionCache(at, at+4);
#endif

    return was;
}

void
Assembler::underrunProtect(int bytes)
{
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
Assembler::BL_far(NIns* addr)
{
    // we have to stick an immediate into the stream and make lr
    // point to the right spot before branching
    underrunProtect(16);

    // TODO use a slot in const pool for address, but emit single insn
    // for branch if offset fits

    // the address
    *(--_nIns) = (NIns)((addr));
    // bx ip             // branch to the address we loaded earlier
    *(--_nIns) = (NIns)( COND_AL | (0x9<<21) | (0xFFF<<8) | (1<<4) | (IP) );
    // add lr, [pc + #4] // set lr to be past the address that we wrote
    *(--_nIns) = (NIns)( COND_AL | OP_IMM | (1<<23) | (PC<<16) | (LR<<12) | (4) );
    // ldr ip, [pc + #4] // load the address into ip, reading it from [pc+4]
    *(--_nIns) = (NIns)( COND_AL | (0x59<<20) | (PC<<16) | (IP<<12) | (4));

    //fprintf (stderr, "BL_far sequence @ 0x%08x\n", _nIns);

    asm_output1("bl %p (32-bit)", addr);
}

void
Assembler::BL(NIns* addr)
{
    intptr_t offs = PC_OFFSET_FROM(addr,_nIns-1);

    //fprintf (stderr, "BL: 0x%x (offs: %d [%x]) @ 0x%08x\n", addr, offs, offs, (intptr_t)(_nIns-1));

    if (isS24(offs)) {
        // try to do this with a single S24 call;
        // recompute offset in case underrunProtect had to allocate a new page
        underrunProtect(4);
        offs = PC_OFFSET_FROM(addr,_nIns-1);
    }

    if (isS24(offs)) {
        // already did underrunProtect above
        *(--_nIns) = (NIns)( COND_AL | (0xB<<24) | (((offs)>>2) & 0xFFFFFF) );
        asm_output1("bl %p", addr);
    } else {
        BL_far(addr);
    }
}

void
Assembler::CALL(const CallInfo *ci)
{
    intptr_t addr = ci->_address;

    BL((NIns*)addr);
    asm_output1("   (call %s)", ci->_name);
}

void
Assembler::LD32_nochk(Register r, int32_t imm)
{
    // We should always reach the const pool, since it's on the same page (<4096);
    // if we can't, someone didn't underrunProtect enough.

    *(++_nSlot) = (int)imm;

    //fprintf (stderr, "wrote slot(2) %p with %08x, jmp @ %p\n", _nSlot, (intptr_t)imm, _nIns-1);

    int offset = PC_OFFSET_FROM(_nSlot,_nIns-1);

    NanoAssert(isS12(offset) && (offset < 0));

    asm_output2("  (%d(PC) = 0x%x)", offset, imm);

    LDR_nochk(r,PC,offset);
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
    int32 offs = PC_OFFSET_FROM(_t,_nIns-1);
    //fprintf(stderr, "B_cond_chk target: 0x%08x offset: %d @0x%08x\n", _t, offs, _nIns-1);
    if (isS24(offs)) {
        if (_chk) underrunProtect(4);
        offs = PC_OFFSET_FROM(_t,_nIns-1);
    }

    if (isS24(offs)) {
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

    asm_output2("%s %p", _c == AL ? "jmp" : "b(cnd)", (void*)(_t));
}

void
Assembler::asm_add_imm(Register rd, Register rn, int32_t imm)
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

    while (immval && ((immval & 0x3) == 0)) {
        immval >>= 2;
        rot--;
    }

    rot &= 0xf;

    if (immval < 256) {
        underrunProtect(4);
        if (pos)
            *(--_nIns) = (NIns)( COND_AL | OP_IMM | OP_STAT | (1<<23) | (rn<<16) | (rd<<12) | (rot << 8) | immval );
        else
            *(--_nIns) = (NIns)( COND_AL | OP_IMM | OP_STAT | (1<<22) | (rn<<16) | (rd<<12) | (rot << 8) | immval );
        asm_output3("add %s,%s,%d",gpn(rd),gpn(rn),imm);
    } else {
        // add scratch to rn, after loading the value into scratch.

        // make sure someone isn't trying to use Scratch as an operand
        NanoAssert(rn != Scratch);

        *(--_nIns) = (NIns)( COND_AL | OP_STAT | (1<<23) | (rn<<16) | (rd<<12) | (Scratch));
        asm_output3("add %s,%s,%s",gpn(rd),gpn(rn),gpn(Scratch));

        LD32_nochk(Scratch, imm);
    }
}

/*
 * VFP
 */

#ifdef NJ_ARM_VFP

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

    // We can't uniquely identify fge/fle via a single bit
    // pattern (since equality and lt/gt are separate bits);
    // so convert to the single-bit variant.
    if (op == LIR_fge) {
        Register temp = ra;
        ra = rb;
        rb = temp;
        op = LIR_flt;
    } else if (op == LIR_fle) {
        Register temp = ra;
        ra = rb;
        rb = temp;
        op = LIR_fgt;
    }

    // There is no way to test for an unordered result using
    // the conditional form of an instruction; the encoding (C=1 V=1)
    // ends up having overlaps with a few other tests.  So, test for
    // the explicit mask.
    uint8_t mask = 0x0;
    
    // NZCV
    // for a valid ordered result, V is always 0 from VFP
    if (op == LIR_feq)
        // ZC // cond EQ (both equal and "not less than"
        mask = 0x6;
    else if (op == LIR_flt)
        // N  // cond MI
        mask = 0x8;
    else if (op == LIR_fgt)
        // C  // cond CS
        mask = 0x2;
    else
        NanoAssert(0);
/*
    // these were converted into gt and lt above.
    if (op == LIR_fle)
        // NZ // cond LE
        mask = 0xC;
    else if (op == LIR_fge)
        // ZC // cond fail?
        mask = 0x6;
*/

    // TODO XXX could do this as fcmpd; fmstat; tstvs rX, #0 the tstvs
    // would reset the status bits if V (NaN flag) is set, but that
    // doesn't work for NE.  For NE could teqvs rX, #1.  rX needs to
    // be any register that has lsb == 0, such as sp/fp/pc.
    
    // Test explicily with the full mask; if V is set, test will fail.
    // Assumption is that this will be followed up by a BEQ/BNE
    CMPi(Scratch, mask);
    // grab just the condition fields
    SHRi(Scratch, 28);
    MRS(Scratch);

    // do the comparison and get results loaded in ARM status register
    FMSTAT();
    FCMPD(ra, rb);
}

Register
Assembler::asm_prep_fcall(Reservation* rR, LInsp ins)
{
    // We have nothing to do here; we do it all in asm_call.
    return UnknownReg;
}

#endif /* NJ_ARM_VFP */

}
#endif /* FEATURE_NANOJIT */
