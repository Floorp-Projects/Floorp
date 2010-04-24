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
 *   leon.sha@sun.com
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

#include <sys/mman.h>
#include <errno.h>
#include "nanojit.h"

namespace nanojit
{
#ifdef FEATURE_NANOJIT

#ifdef NJ_VERBOSE
    const char *regNames[] = {
        "%g0", "%g1", "%g2", "%g3", "%g4", "%g5", "%g6", "%g7",
        "%o0", "%o1", "%o2", "%o3", "%o4", "%o5", "%sp", "%o7",
        "%l0", "%l1", "%l2", "%l3", "%l4", "%l5", "%l6", "%l7",
        "%i0", "%i1", "%i2", "%i3", "%i4", "%i5", "%fp", "%i7",
        "%f0", "%f1", "%f2", "%f3", "%f4", "%f5", "%f6", "%f7",
        "%f8", "%f9", "%f10", "%f11", "%f12", "%f13", "%f14", "%f15",
        "%f16", "%f17", "%f18", "%f19", "%f20", "%f21", "%f22", "%f23",
        "%f24", "%f25", "%f26", "%f27", "%f28", "%f29", "%f30", "%f31"
    };
#endif

    const Register Assembler::argRegs[] = { I0, I1, I2, I3, I4, I5 };
    const Register Assembler::retRegs[] = { O0 };
    const Register Assembler::savedRegs[] = { L1 };

    static const int kLinkageAreaSize = 68;
    static const int kcalleeAreaSize = 80; // The max size.

#define BIT_ROUND_UP(v,q)      ( (((uintptr_t)v)+(q)-1) & ~((q)-1) )
#define TODO(x) do{ verbose_only(outputf(#x);) NanoAssertMsgf(false, "%s", #x); } while(0)

    void Assembler::nInit(AvmCore* core)
    {
        has_cmov = true;
    }

    void Assembler::nBeginAssembly() {
    }

    NIns* Assembler::genPrologue()
    {
        /**
         * Prologue
         */
        underrunProtect(16);
        uint32_t stackNeeded = STACK_GRANULARITY * _activation.stackSlotsNeeded();
        uint32_t frameSize = stackNeeded + kcalleeAreaSize + kLinkageAreaSize;
        frameSize = BIT_ROUND_UP(frameSize, 8);

        if (frameSize <= 4096)
            SUBI(FP, frameSize, SP);
        else {
            SUB(FP, G1, SP);
            ORI(G1, frameSize & 0x3FF, G1);
            SETHI(frameSize, G1);
        }

        verbose_only(
        if (_logc->lcbits & LC_Assembly) {
            outputf("        %p:",_nIns);
            outputf("        patch entry:");
        })
        NIns *patchEntry = _nIns;

        // The frame size in SAVE is faked. We will still re-caculate SP later.
        // We can use 0 here but it is not good for debuggers.
        SAVEI(SP, -148, SP);

        // align the entry point
        asm_align_code();

        return patchEntry;
    }

    void Assembler::asm_align_code() {
        while(uintptr_t(_nIns) & 15) {
            NOP();
        }
    }

    void Assembler::nFragExit(LInsp guard)
    {
        SideExit* exit = guard->record()->exit;
        Fragment *frag = exit->target;
        GuardRecord *lr;
        if (frag && frag->fragEntry)
            {
                JMP(frag->fragEntry);
                lr = 0;
            }
        else
            {
                // Target doesn't exit yet. Emit jump to epilog, and set up to patch later.
                if (!_epilogue)
                    _epilogue = genEpilogue();
                lr = guard->record();
                JMP_long((intptr_t)_epilogue);
                lr->jmp = _nIns;
            }

        // return value is GuardRecord*
        SET32(int(lr), O0);
    }

    NIns *Assembler::genEpilogue()
    {
        underrunProtect(12);
        RESTORE(G0, G0, G0); //restore
        JMPLI(I7, 8, G0); //ret
        ORI(O0, 0, I0);
        return  _nIns;
    }

    void Assembler::asm_call(LInsp ins)
    {
        Register retReg = ( ins->isop(LIR_fcall) ? F0 : retRegs[0] );
        deprecated_prepResultReg(ins, rmask(retReg));

        // Do this after we've handled the call result, so we don't
        // force the call result to be spilled unnecessarily.

        evictScratchRegsExcept(0);

        const CallInfo* ci = ins->callInfo();

        underrunProtect(8);
        NOP();

        ArgType argTypes[MAXARGS];
        uint32_t argc = ci->getArgTypes(argTypes);

        NanoAssert(ins->isop(LIR_pcall) || ins->isop(LIR_fcall));
        verbose_only(if (_logc->lcbits & LC_Assembly)
                     outputf("        %p:", _nIns);
                     )
        bool indirect = ci->isIndirect();
        if (!indirect) {
            CALL(ci);
        }
        else {
            argc--;
            Register r = findSpecificRegFor(ins->arg(argc), I0);
            JMPL(G0, I0, 15);
        }

        uint32_t GPRIndex = O0;
        uint32_t offset = kLinkageAreaSize; // start of parameters stack postion.

        for(int i=0; i<argc; i++)
            {
                uint32_t j = argc-i-1;
                ArgType ty = argTypes[j];
                if (ty == ARGTYPE_D) {
                    Register r = findRegFor(ins->arg(j), FpRegs);
                    GPRIndex += 2;
                    offset += 8;

                    underrunProtect(48);
                    // We might be calling a varargs function.
                    // So, make sure the GPR's are also loaded with
                    // the value, or the stack contains it.
                    if (GPRIndex-2 <= O5) {
                        LDSW32(SP, offset-8, (Register)(GPRIndex-2));
                    }
                    if (GPRIndex-1 <= O5) {
                        LDSW32(SP, offset-4, (Register)(GPRIndex-1));
                    }
                    STDF32(r, offset-8, SP);
                } else {
                    if (GPRIndex > O5) {
                        underrunProtect(12);
                        Register r = findRegFor(ins->arg(j), GpRegs);
                        STW32(r, offset, SP);
                    } else {
                        Register r = findSpecificRegFor(ins->arg(j), (Register)GPRIndex);
                    }
                    GPRIndex++;
                    offset += 4;
                }
            }
    }

    Register Assembler::nRegisterAllocFromSet(RegisterMask set)
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
        a.clear();
        a.free = GpRegs | FpRegs;
        debug_only( a.managed = a.free; )
    }

    void Assembler::nPatchBranch(NIns* branch, NIns* location)
    {
        *(uint32_t*)&branch[0] &= 0xFFC00000;
        *(uint32_t*)&branch[0] |= ((intptr_t)location >> 10) & 0x3FFFFF;
        *(uint32_t*)&branch[1] &= 0xFFFFFC00;
        *(uint32_t*)&branch[1] |= (intptr_t)location & 0x3FF;
    }

    RegisterMask Assembler::hint(LIns* ins)
    {
        return 0;
    }

    bool Assembler::canRemat(LIns* ins)
    {
        return ins->isImmAny() || ins->isop(LIR_alloc);
    }

    void Assembler::asm_restore(LInsp i, Register r)
    {
        underrunProtect(24);
        if (i->isop(LIR_alloc)) {
            ADD(FP, L2, r);
            int32_t d = deprecated_disp(i);
            SET32(d, L2);
        }
        else if (i->isImmI()) {
            int v = i->immI();
            SET32(v, r);
        } else {
            int d = findMemFor(i);
            if (rmask(r) & FpRegs) {
                LDDF32(FP, d, r);
            } else {
                LDSW32(FP, d, r);
            }
        }
    }

    void Assembler::asm_store32(LOpcode op, LIns *value, int dr, LIns *base)
    {
        switch (op) {
            case LIR_sti:
            case LIR_stb:
                // handled by mainline code below for now
                break;
            case LIR_sts:
                NanoAssertMsg(0, "NJ_EXPANDED_LOADSTORE_SUPPORTED not yet supported for this architecture");
                return;
            default:
                NanoAssertMsg(0, "asm_store32 should never receive this LIR opcode");
                return;
        }

        underrunProtect(20);
        if (value->isImmI())
            {
                Register rb = getBaseReg(base, dr, GpRegs);
                int c = value->immI();
                switch (op) {
                case LIR_sti:
                    STW32(L2, dr, rb);
                    break;
                case LIR_stb:
                    STB32(L2, dr, rb);
                    break;
                }
                SET32(c, L2);
            }
        else
            {
                // make sure what is in a register
                Register ra, rb;
                if (base->isImmI()) {
                    // absolute address
                    dr += base->immI();
                    ra = findRegFor(value, GpRegs);
                    rb = G0;
                } else {
                    getBaseReg2(GpRegs, value, ra, GpRegs, base, rb, dr);
                }
                switch (op) {
                case LIR_sti:
                    STW32(ra, dr, rb);
                    break;
                case LIR_stb:
                    STB32(ra, dr, rb);
                    break;
                }
            }
    }

    void Assembler::asm_spill(Register rr, int d, bool pop, bool quad)
    {
        underrunProtect(24);
        (void)quad;
        NanoAssert(d);
        if (rmask(rr) & FpRegs) {
            STDF32(rr, d, FP);
        } else {
            STW32(rr, d, FP);
        }
    }

    void Assembler::asm_load64(LInsp ins)
    {
        switch (ins->opcode()) {
            case LIR_ldf:
                // handled by mainline code below for now
                break;
            case LIR_ld32f:
                NanoAssertMsg(0, "NJ_EXPANDED_LOADSTORE_SUPPORTED not yet supported for this architecture");
                return;
            default:
                NanoAssertMsg(0, "asm_load64 should never receive this LIR opcode");
                return;
        }

        underrunProtect(72);
        LIns* base = ins->oprnd1();
        int db = ins->disp();
        Register rr = ins->deprecated_getReg();

        int dr = deprecated_disp(ins);
        Register rb;
        if (base->isop(LIR_alloc)) {
            rb = FP;
            db += findMemFor(base);
        } else {
            rb = findRegFor(base, GpRegs);
        }
        ins->clearReg();

        // don't use an fpu reg to simply load & store the value.
        if (dr)
            asm_mmq(FP, dr, rb, db);

        deprecated_freeRsrcOf(ins);

        if (rr != deprecated_UnknownReg)
            {
                NanoAssert(rmask(rr)&FpRegs);
                _allocator.retire(rr);
                LDDF32(rb, db, rr);
            }
    }

    void Assembler::asm_store64(LOpcode op, LInsp value, int dr, LInsp base)
    {
        switch (op) {
            case LIR_stfi:
                // handled by mainline code below for now
                break;
            case LIR_st32f:
                NanoAssertMsg(0, "NJ_EXPANDED_LOADSTORE_SUPPORTED not yet supported for this architecture");
                return;
            default:
                NanoAssertMsg(0, "asm_store64 should never receive this LIR opcode");
                return;
        }

        underrunProtect(48);
        if (value->isImmD())
            {
                // if a constant 64-bit value just store it now rather than
                // generating a pointless store/load/store sequence
                Register rb = findRegFor(base, GpRegs);
                STW32(L2, dr+4, rb);
                SET32(value->immQorDlo(), L2);
                STW32(L2, dr, rb);
                SET32(value->immQorDhi(), L2);
                return;
            }

        if (value->isop(LIR_ldf))
            {
                // value is 64bit struct or int64_t, or maybe a double.
                // it may be live in an FPU reg.  Either way, don't
                // put it in an FPU reg just to load & store it.

                // a) if we know it's not a double, this is right.
                // b) if we guarded that its a double, this store could be on
                // the side exit, copying a non-double.
                // c) maybe its a double just being stored.  oh well.

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
        Register rv = ( !value->isInReg()
                      ? findRegFor(value, FpRegs)
                      : value->deprecated_getReg() );

        STDF32(rv, dr, rb);
    }

    /**
     * copy 64 bits: (rd+dd) <- (rs+ds)
     */
    void Assembler::asm_mmq(Register rd, int dd, Register rs, int ds)
    {
        // value is either a 64bit struct or maybe a float
        // that isn't live in an FPU reg.  Either way, don't
        // put it in an FPU reg just to load & store it.
        Register t = registerAllocTmp(GpRegs & ~(rmask(rd)|rmask(rs)));
        STW32(t, dd+4, rd);
        LDSW32(rs, ds+4, t);
        STW32(t, dd, rd);
        LDSW32(rs, ds, t);
    }

    NIns* Assembler::asm_branch(bool branchOnFalse, LInsp cond, NIns* targ)
    {
        NIns* at = 0;
        LOpcode condop = cond->opcode();
        NanoAssert(cond->isCmp());
        if (isCmpDOpcode(condop))
            {
                return asm_fbranch(branchOnFalse, cond, targ);
            }

        underrunProtect(32);
        intptr_t tt = ((intptr_t)targ - (intptr_t)_nIns + 8) >> 2;
        // !targ means that it needs patch.
        if( !(isIMM22((int32_t)tt)) || !targ ) {
            JMP_long_nocheck((intptr_t)targ);
            at = _nIns;
            NOP();
            BA(0, 5);
            tt = 4;
        }
        NOP();

        // produce the branch
        if (branchOnFalse)
            {
                if (condop == LIR_eq)
                    BNE(0, tt);
                else if (condop == LIR_lt)
                    BGE(0, tt);
                else if (condop == LIR_le)
                    BG(0, tt);
                else if (condop == LIR_gt)
                    BLE(0, tt);
                else if (condop == LIR_ge)
                    BL(0, tt);
                else if (condop == LIR_ult)
                    BCC(0, tt);
                else if (condop == LIR_ule)
                    BGU(0, tt);
                else if (condop == LIR_ugt)
                    BLEU(0, tt);
                else //if (condop == LIR_uge)
                    BCS(0, tt);
            }
        else // op == LIR_xt
            {
                if (condop == LIR_eq)
                    BE(0, tt);
                else if (condop == LIR_lt)
                    BL(0, tt);
                else if (condop == LIR_le)
                    BLE(0, tt);
                else if (condop == LIR_gt)
                    BG(0, tt);
                else if (condop == LIR_ge)
                    BGE(0, tt);
                else if (condop == LIR_ult)
                    BCS(0, tt);
                else if (condop == LIR_ule)
                    BLEU(0, tt);
                else if (condop == LIR_ugt)
                    BGU(0, tt);
                else //if (condop == LIR_uge)
                    BCC(0, tt);
            }
        asm_cmp(cond);
        return at;
    }

    void Assembler::asm_branch_xov(LOpcode, NIns* targ)
    {
        underrunProtect(32);
        intptr_t tt = ((intptr_t)targ - (intptr_t)_nIns + 8) >> 2;
        // !targ means that it needs patch.
        if( !(isIMM22((int32_t)tt)) || !targ ) {
            JMP_long_nocheck((intptr_t)targ);
            NOP();
            BA(0, 5);
            tt = 4;
        }
        NOP();

        BVS(0, tt);
    }

    void Assembler::asm_cmp(LIns *cond)
    {
        underrunProtect(12);

        LInsp lhs = cond->oprnd1();
        LInsp rhs = cond->oprnd2();

        NanoAssert(lhs->isI() && rhs->isI());

        // ready to issue the compare
        if (rhs->isImmI())
            {
                int c = rhs->immI();
                Register r = findRegFor(lhs, GpRegs);
                if (c == 0 && cond->isop(LIR_eq)) {
                    ANDCC(r, r, G0);
                }
                else {
                    SUBCC(r, L2, G0);
                    SET32(c, L2);
                }
            }
        else
            {
                Register ra, rb;
                findRegFor2(GpRegs, lhs, ra, GpRegs, rhs, rb);
                SUBCC(ra, rb, G0);
            }
    }

    void Assembler::asm_fcond(LInsp ins)
    {
        // only want certain regs
        Register r = deprecated_prepResultReg(ins, AllowableFlagRegs);
        underrunProtect(8);
        LOpcode condop = ins->opcode();
        NanoAssert(isCmpDOpcode(condop));
        if (condop == LIR_feq)
            MOVFEI(1, 0, 0, 0, r);
        else if (condop == LIR_fle)
            MOVFLEI(1, 0, 0, 0, r);
        else if (condop == LIR_flt)
            MOVFLI(1, 0, 0, 0, r);
        else if (condop == LIR_fge)
            MOVFGEI(1, 0, 0, 0, r);
        else // if (condop == LIR_fgt)
            MOVFGI(1, 0, 0, 0, r);
        ORI(G0, 0, r);
        asm_fcmp(ins);
    }

    void Assembler::asm_cond(LInsp ins)
    {
        underrunProtect(8);
        // only want certain regs
        LOpcode op = ins->opcode();
        Register r = deprecated_prepResultReg(ins, AllowableFlagRegs);

        if (op == LIR_eq)
            MOVEI(1, 1, 0, 0, r);
        else if (op == LIR_lt)
            MOVLI(1, 1, 0, 0, r);
        else if (op == LIR_le)
            MOVLEI(1, 1, 0, 0, r);
        else if (op == LIR_gt)
            MOVGI(1, 1, 0, 0, r);
        else if (op == LIR_ge)
            MOVGEI(1, 1, 0, 0, r);
        else if (op == LIR_ult)
            MOVCSI(1, 1, 0, 0, r);
        else if (op == LIR_ule)
            MOVLEUI(1, 1, 0, 0, r);
        else if (op == LIR_ugt)
            MOVGUI(1, 1, 0, 0, r);
        else // if (op == LIR_uge)
            MOVCCI(1, 1, 0, 0, r);
        ORI(G0, 0, r);
        asm_cmp(ins);
    }

    void Assembler::asm_arith(LInsp ins)
    {
        underrunProtect(28);
        LOpcode op = ins->opcode();
        LInsp lhs = ins->oprnd1();
        LInsp rhs = ins->oprnd2();

        Register rb = deprecated_UnknownReg;
        RegisterMask allow = GpRegs;
        bool forceReg = (op == LIR_mul || op == LIR_mulxov || !rhs->isImmI());

        if (lhs != rhs && forceReg)
            {
                if ((rb = asm_binop_rhs_reg(ins)) == deprecated_UnknownReg) {
                    rb = findRegFor(rhs, allow);
                }
                allow &= ~rmask(rb);
            }
        else if ((op == LIR_add || op == LIR_addxov) && lhs->isop(LIR_alloc) && rhs->isImmI()) {
            // add alloc+const, use lea
            Register rr = deprecated_prepResultReg(ins, allow);
            int d = findMemFor(lhs) + rhs->immI();
            ADD(FP, L2, rr);
            SET32(d, L2);
        }

        Register rr = deprecated_prepResultReg(ins, allow);
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a register assigned.
        Register ra = ( !lhs->isInReg()
                      ? findSpecificRegFor(lhs, rr)
                      : lhs->deprecated_getReg() );

        if (forceReg)
            {
                if (lhs == rhs)
                    rb = ra;

                if (op == LIR_add || op == LIR_addxov)
                    ADDCC(rr, rb, rr);
                else if (op == LIR_sub || op == LIR_subxov)
                    SUBCC(rr, rb, rr);
                else if (op == LIR_mul || op == LIR_mulxov)
                    MULX(rr, rb, rr);
                else if (op == LIR_and)
                    AND(rr, rb, rr);
                else if (op == LIR_or)
                    OR(rr, rb, rr);
                else if (op == LIR_xor)
                    XOR(rr, rb, rr);
                else if (op == LIR_lsh)
                    SLL(rr, rb, rr);
                else if (op == LIR_rsh)
                    SRA(rr, rb, rr);
                else if (op == LIR_ush)
                    SRL(rr, rb, rr);
                else
                    NanoAssertMsg(0, "Unsupported");
            }
        else
            {
                int c = rhs->immI();
                if (op == LIR_add || op == LIR_addxov)
                    ADDCC(rr, L2, rr);
                else if (op == LIR_sub || op == LIR_subxov)
                    SUBCC(rr, L2, rr);
                else if (op == LIR_and)
                    AND(rr, L2, rr);
                else if (op == LIR_or)
                    OR(rr, L2, rr);
                else if (op == LIR_xor)
                    XOR(rr, L2, rr);
                else if (op == LIR_lsh)
                    SLL(rr, L2, rr);
                else if (op == LIR_rsh)
                    SRA(rr, L2, rr);
                else if (op == LIR_ush)
                    SRL(rr, L2, rr);
                else
                    NanoAssertMsg(0, "Unsupported");
                SET32(c, L2);
            }

        if ( rr != ra )
            ORI(ra, 0, rr);
    }

    void Assembler::asm_neg_not(LInsp ins)
    {
        underrunProtect(8);
        LOpcode op = ins->opcode();
        Register rr = deprecated_prepResultReg(ins, GpRegs);

        LIns* lhs = ins->oprnd1();
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a register assigned.
        Register ra = ( !lhs->isInReg()
                      ? findSpecificRegFor(lhs, rr)
                      : lhs->deprecated_getReg() );

        if (op == LIR_not)
            ORN(G0, rr, rr);
        else
            SUB(G0, rr, rr);

        if ( rr != ra )
            ORI(ra, 0, rr);
    }

    void Assembler::asm_load32(LInsp ins)
    {
        underrunProtect(12);
        LOpcode op = ins->opcode();
        LIns* base = ins->oprnd1();
        int d = ins->disp();
        Register rr = deprecated_prepResultReg(ins, GpRegs);
        Register ra = getBaseReg(base, d, GpRegs);
        switch(op) {
            case LIR_ldzb:
                LDUB32(ra, d, rr);
                break;
            case LIR_ldzs:
                LDUH32(ra, d, rr);
                break;
            case LIR_ld:
                LDSW32(ra, d, rr);
                break;
            case LIR_ldsb:
            case LIR_ldss:
                NanoAssertMsg(0, "NJ_EXPANDED_LOADSTORE_SUPPORTED not yet supported for this architecture");
                return;
            default:
                NanoAssertMsg(0, "asm_load32 should never receive this LIR opcode");
                return;
        }
    }

    void Assembler::asm_cmov(LInsp ins)
    {
        underrunProtect(4);
        LOpcode op = ins->opcode();
        LIns* condval = ins->oprnd1();
        LIns* iftrue  = ins->oprnd2();
        LIns* iffalse = ins->oprnd3();

        NanoAssert(condval->isCmp());
        NanoAssert(op == LIR_cmov && iftrue->isI() && iffalse->isI());

        const Register rr = deprecated_prepResultReg(ins, GpRegs);

        // this code assumes that neither LD nor MR nor MRcc set any of the condition flags.
        // (This is true on Intel, is it true on all architectures?)
        const Register iffalsereg = findRegFor(iffalse, GpRegs & ~rmask(rr));
        if (op == LIR_cmov) {
            switch (condval->opcode()) {
                // note that these are all opposites...
            case LIR_eq:  MOVNE (iffalsereg, 1, 0, 0, rr); break;
            case LIR_lt:  MOVGE (iffalsereg, 1, 0, 0, rr); break;
            case LIR_le:  MOVG  (iffalsereg, 1, 0, 0, rr); break;
            case LIR_gt:  MOVLE (iffalsereg, 1, 0, 0, rr); break;
            case LIR_ge:  MOVL  (iffalsereg, 1, 0, 0, rr); break;
            case LIR_ult: MOVCC (iffalsereg, 1, 0, 0, rr); break;
            case LIR_ule: MOVGU (iffalsereg, 1, 0, 0, rr); break;
            case LIR_ugt: MOVLEU(iffalsereg, 1, 0, 0, rr); break;
            case LIR_uge: MOVCS (iffalsereg, 1, 0, 0, rr); break;
                debug_only( default: NanoAssert(0); break; )
                    }
        }
        /*const Register iftruereg =*/ findSpecificRegFor(iftrue, rr);
        asm_cmp(condval);
    }

    void Assembler::asm_param(LInsp ins)
    {
        uint32_t a = ins->paramArg();
        uint32_t kind = ins->paramKind();
        deprecated_prepResultReg(ins, rmask(argRegs[a]));
    }

    void Assembler::asm_immi(LInsp ins)
    {
        underrunProtect(8);
        Register rr = deprecated_prepResultReg(ins, GpRegs);
        int32_t val = ins->immI();
        if (val == 0)
            XOR(rr, rr, rr);
        else
            SET32(val, rr);
    }

    void Assembler::asm_immf(LInsp ins)
    {
        underrunProtect(64);
        Register rr = ins->deprecated_getReg();
        if (rr != deprecated_UnknownReg)
            {
                // @todo -- add special-cases for 0 and 1
                _allocator.retire(rr);
                ins->clearReg();
                NanoAssert((rmask(rr) & FpRegs) != 0);
                findMemFor(ins);
                int d = deprecated_disp(ins);
                LDDF32(FP, d, rr);
            }

        // @todo, if we used xor, ldsd, fldz, etc above, we don't need mem here
        int d = deprecated_disp(ins);
        deprecated_freeRsrcOf(ins);
        if (d)
            {
                STW32(L2, d+4, FP);
                SET32(ins->immQorDlo(), L2);
                STW32(L2, d, FP);
                SET32(ins->immQorDhi(), L2);
            }
    }

    void Assembler::asm_fneg(LInsp ins)
    {
        underrunProtect(4);
        Register rr = deprecated_prepResultReg(ins, FpRegs);
        LIns* lhs = ins->oprnd1();

        // lhs into reg, prefer same reg as result
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a different reg assigned
        Register ra = ( !lhs->isInReg()
                      ? findSpecificRegFor(lhs, rr)
                      : findRegFor(lhs, FpRegs) );

        FNEGD(ra, rr);
    }

    void Assembler::asm_fop(LInsp ins)
    {
        underrunProtect(4);
        LOpcode op = ins->opcode();
        LIns *lhs = ins->oprnd1();
        LIns *rhs = ins->oprnd2();

        RegisterMask allow = FpRegs;
        Register ra = findRegFor(lhs, FpRegs);
        Register rb = (rhs == lhs) ? ra : findRegFor(rhs, FpRegs);

        Register rr = deprecated_prepResultReg(ins, allow);

        if (op == LIR_fadd)
            FADDD(ra, rb, rr);
        else if (op == LIR_fsub)
            FSUBD(ra, rb, rr);
        else if (op == LIR_fmul)
            FMULD(ra, rb, rr);
        else //if (op == LIR_fdiv)
            FDIVD(ra, rb, rr);

    }

    void Assembler::asm_i2f(LInsp ins)
    {
        underrunProtect(32);
        // where our result goes
        Register rr = deprecated_prepResultReg(ins, FpRegs);
        int d = findMemFor(ins->oprnd1());
        FITOD(rr, rr);
        LDDF32(FP, d, rr);
    }

    void Assembler::asm_u2f(LInsp ins)
    {
        underrunProtect(72);
        // where our result goes
        Register rr = deprecated_prepResultReg(ins, FpRegs);
        Register rt = registerAllocTmp(FpRegs & ~(rmask(rr)));
        Register gr = findRegFor(ins->oprnd1(), GpRegs);
        int disp = -8;

        FABSS(rr, rr);
        FSUBD(rt, rr, rr);
        LDDF32(SP, disp, rr);
        STWI(G0, disp+4, SP);
        LDDF32(SP, disp, rt);
        STWI(gr, disp+4, SP);
        STWI(G1, disp, SP);
        SETHI(0x43300000, G1);
    }

    void Assembler::asm_f2i(LInsp ins) {
        LIns *lhs = ins->oprnd1();
        Register rr = prepareResultReg(ins, GpRegs);
        Register ra = findRegFor(lhs, FpRegs);
        int d = findMemFor(ins);
        LDSW32(FP, d, rr);
        STF32(ra, d, FP);
        FDTOI(ra, ra);
    }

    void Assembler::asm_nongp_copy(Register r, Register s)
    {
        underrunProtect(4);
        NanoAssert((rmask(r) & FpRegs) && (rmask(s) & FpRegs));
        FMOVD(s, r);
    }

    NIns * Assembler::asm_fbranch(bool branchOnFalse, LIns *cond, NIns *targ)
    {
        NIns *at = 0;
        LOpcode condop = cond->opcode();
        NanoAssert(isCmpDOpcode(condop));
        underrunProtect(32);
        intptr_t tt = ((intptr_t)targ - (intptr_t)_nIns + 8) >> 2;
        // !targ means that it needs patch.
        if( !(isIMM22((int32_t)tt)) || !targ ) {
            JMP_long_nocheck((intptr_t)targ);
            at = _nIns;
            NOP();
            BA(0, 5);
            tt = 4;
        }
        NOP();

        // produce the branch
        if (branchOnFalse)
            {
                if (condop == LIR_feq)
                    FBNE(0, tt);
                else if (condop == LIR_fle)
                    FBUG(0, tt);
                else if (condop == LIR_flt)
                    FBUGE(0, tt);
                else if (condop == LIR_fge)
                    FBUL(0, tt);
                else //if (condop == LIR_fgt)
                    FBULE(0, tt);
            }
        else // op == LIR_xt
            {
                if (condop == LIR_feq)
                    FBE(0, tt);
                else if (condop == LIR_fle)
                    FBLE(0, tt);
                else if (condop == LIR_flt)
                    FBL(0, tt);
                else if (condop == LIR_fge)
                    FBGE(0, tt);
                else //if (condop == LIR_fgt)
                    FBG(0, tt);
            }
        asm_fcmp(cond);
        return at;
    }

    void Assembler::asm_fcmp(LIns *cond)
    {
        underrunProtect(4);
        LIns* lhs = cond->oprnd1();
        LIns* rhs = cond->oprnd2();

        Register rLhs = findRegFor(lhs, FpRegs);
        Register rRhs = findRegFor(rhs, FpRegs);

        FCMPD(rLhs, rRhs);
    }

    void Assembler::nativePageReset()
    {
    }

    Register Assembler::asm_binop_rhs_reg(LInsp ins)
    {
        return deprecated_UnknownReg;
    }

    void Assembler::nativePageSetup()
    {
        NanoAssert(!_inExit);
        if (!_nIns)
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
    }

    // Increment the 32-bit profiling counter at pCtr, without
    // changing any registers.
    verbose_only(
    void Assembler::asm_inc_m32(uint32_t*)
    {
        // todo: implement this
    }
    )

    void
    Assembler::underrunProtect(int n)
    {
        NIns *eip = _nIns;
        // This may be in a normal code chunk or an exit code chunk.
        if (eip - n < codeStart) {
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
            JMP_long_nocheck((intptr_t)eip);
        }
    }

    void Assembler::asm_ret(LInsp ins)
    {
        genEpilogue();
        releaseRegisters();
        assignSavedRegs();
        LIns *val = ins->oprnd1();
        if (ins->isop(LIR_ret)) {
            findSpecificRegFor(val, retRegs[0]);
        } else {
            NanoAssert(ins->isop(LIR_fret));
            findSpecificRegFor(val, F0);
        }
    }

    void Assembler::swapCodeChunks() {
        if (!_nExitIns)
            codeAlloc(exitStart, exitEnd, _nExitIns verbose_only(, exitBytes));
        SWAP(NIns*, _nIns, _nExitIns);
        SWAP(NIns*, codeStart, exitStart);
        SWAP(NIns*, codeEnd, exitEnd);
        verbose_only( SWAP(size_t, codeBytes, exitBytes); )
    }

#endif /* FEATURE_NANOJIT */
}
