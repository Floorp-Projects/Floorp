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
    const Register Assembler::savedRegs[] = { I0, I1, I2, I3, I4, I5 };

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
        uint32_t stackNeeded = STACK_GRANULARITY * _activation.tos;
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
            output("        patch entry:");
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
        const CallInfo* call = ins->callInfo();

        underrunProtect(8);
        NOP();

        ArgSize sizes[MAXARGS];
        uint32_t argc = call->get_sizes(sizes);

        NanoAssert(ins->isop(LIR_pcall) || ins->isop(LIR_fcall));
        verbose_only(if (_logc->lcbits & LC_Assembly)
                     outputf("        %p:", _nIns);
                     )
        bool indirect = call->isIndirect();
        if (!indirect) {
            CALL(call);
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
                ArgSize sz = sizes[j];
                if (sz == ARGSIZE_F) {
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

    RegisterMask Assembler::hint(LIns* i, RegisterMask allow)
    {
        return allow;
    }

    void Assembler::asm_qjoin(LIns *ins)
    {
        underrunProtect(40);
        int d = findMemFor(ins);
        AvmAssert(d);
        LIns* lo = ins->oprnd1();
        LIns* hi = ins->oprnd2();

        Register rr = ins->getReg();
        if (isKnownReg(rr) && (rmask(rr) & FpRegs))
            evict(rr, ins);

        if (hi->isconst()) {
            STW32(L2, d+4, FP);
            SET32(hi->imm32(), L2);
        } else {
            Register rh = findRegFor(hi, GpRegs);
            STW32(rh, d+4, FP);
        }

        if (lo->isconst()) {
            STW32(L2, d, FP);
            SET32(lo->imm32(), L2);
        } else {
            // okay if r gets recycled.
            Register rl = findRegFor(lo, GpRegs);
            STW32(rl, d, FP);
        }

        freeRsrcOf(ins, false);    // if we had a reg in use, emit a ST to flush it to mem
    }


    void Assembler::asm_restore(LInsp i, Reservation *unused, Register r)
    {
        underrunProtect(24);
        if (i->isop(LIR_alloc)) {
            ADD(FP, L2, r);
            int32_t d = disp(i);
            SET32(d, L2);
            verbose_only(if (_logc->lcbits & LC_RegAlloc) {
                outputf("        remat %s size %d", _thisfrag->lirbuf->names->formatRef(i), i->size());
            })
        }
        else if (i->isconst()) {
            if (!i->getArIndex()) {
                i->markAsClear();
            }
            int v = i->imm32();
            SET32(v, r);
        } else {
            int d = findMemFor(i);
            if (rmask(r) & FpRegs) {
                LDDF32(FP, d, r);
            } else {
                LDSW32(FP, d, r);
            }
            verbose_only(if (_logc->lcbits & LC_RegAlloc) {
                outputf("        restore %s", _thisfrag->lirbuf->names->formatRef(i));
            })
        }
    }

    void Assembler::asm_store32(LIns *value, int dr, LIns *base)
    {
        underrunProtect(20);
        if (value->isconst())
            {
                Register rb = getBaseReg(base, dr, GpRegs);
                int c = value->imm32();
                STW32(L2, dr, rb);
                SET32(c, L2);
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
                    rb = G0;
                } else {
                    findRegFor2b(GpRegs, value, ra, base, rb);
                }
                STW32(ra, dr, rb);
            }
    }

    void Assembler::asm_spill(Register rr, int d, bool pop, bool quad)
    {
        underrunProtect(24);
        (void)quad;
        if (d) {
            if (rmask(rr) & FpRegs) {
                STDF32(rr, d, FP);
            } else {
                STW32(rr, d, FP);
            }
        }
    }

    void Assembler::asm_load64(LInsp ins)
    {
        underrunProtect(72);
        LIns* base = ins->oprnd1();
        int db = ins->disp();
        Register rr = ins->getReg();

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

        if (rr != UnknownReg)
            {
                NanoAssert(rmask(rr)&FpRegs);
                _allocator.retire(rr);
                LDDF32(rb, db, rr);
            }
    }

    void Assembler::asm_store64(LInsp value, int dr, LInsp base)
    {
        underrunProtect(48);
        if (value->isconstq())
            {
                // if a constant 64-bit value just store it now rather than
                // generating a pointless store/load/store sequence
                Register rb = findRegFor(base, GpRegs);
                STW32(L2, dr+4, rb);
                SET32(value->imm64_0(), L2);
                STW32(L2, dr, rb);
                SET32(value->imm64_1(), L2);
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
        Register rv = ( value->isUnusedOrHasUnknownReg()
                      ? findRegFor(value, FpRegs)
                      : value->getReg() );

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
        Register t = registerAlloc(GpRegs & ~(rmask(rd)|rmask(rs)));
        _allocator.addFree(t);
        STW32(t, dd+4, rd);
        LDSW32(rs, ds+4, t);
        STW32(t, dd, rd);
        LDSW32(rs, ds, t);
    }

    NIns* Assembler::asm_branch(bool branchOnFalse, LInsp cond, NIns* targ)
    {
        NIns* at = 0;
        LOpcode condop = cond->opcode();
        NanoAssert(cond->isCond());
        if (condop >= LIR_feq && condop <= LIR_fge)
            {
                return asm_jmpcc(branchOnFalse, cond, targ);
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
                else if (condop == LIR_ov)
                    BVC(0, tt);
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
                else if (condop == LIR_ov)
                    BVS(0, tt);
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

    void Assembler::asm_cmp(LIns *cond)
    {
        underrunProtect(12);
        LOpcode condop = cond->opcode();

        // LIR_ov recycles the flags set by arithmetic ops
        if (condop == LIR_ov)
            return;

        LInsp lhs = cond->oprnd1();
        LInsp rhs = cond->oprnd2();

        NanoAssert((!lhs->isQuad() && !rhs->isQuad()) || (lhs->isQuad() && rhs->isQuad()));

        NanoAssert(!lhs->isQuad() && !rhs->isQuad());

        // ready to issue the compare
        if (rhs->isconst())
            {
                int c = rhs->imm32();
                if (c == 0 && cond->isop(LIR_eq)) {
                    Register r = findRegFor(lhs, GpRegs);
                    ANDCC(r, r, G0);
                }
                else if (!rhs->isQuad()) {
                    Register r = getBaseReg(lhs, c, GpRegs);
                    SUBCC(r, L2, G0);
                    SET32(c, L2);
                }
            }
        else
            {
                Register ra, rb;
                findRegFor2b(GpRegs, lhs, ra, rhs, rb);
                SUBCC(ra, rb, G0);
            }
    }

    void Assembler::asm_fcond(LInsp ins)
    {
        // only want certain regs
        Register r = prepResultReg(ins, AllowableFlagRegs);
        asm_setcc(r, ins);
    }

    void Assembler::asm_cond(LInsp ins)
    {
        underrunProtect(8);
        // only want certain regs
        LOpcode op = ins->opcode();
        Register r = prepResultReg(ins, AllowableFlagRegs);

        if (op == LIR_eq)
            MOVEI(1, 1, 0, 0, r);
        else if (op == LIR_ov)
            MOVVSI(1, 1, 0, 0, r);
        else if (op == LIR_lt)
            MOVLI(1, 1, 0, 0, r);
        else if (op == LIR_le)
            MOVLEI(1, 1, 0, 0, r);
        else if (op == LIR_gt)
            MOVGI(1, 1, 0, 0, r);
        else if (op == LIR_ge)
            MOVGEI(1, 1, 0, 0, r);
        else if (op == LIR_ult)
            MOVEI(1, 1, 0, 0, r);
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

        Register rb = UnknownReg;
        RegisterMask allow = GpRegs;
        bool forceReg = (op == LIR_mul || !rhs->isconst());

        if (lhs != rhs && forceReg)
            {
                if ((rb = asm_binop_rhs_reg(ins)) == UnknownReg) {
                    rb = findRegFor(rhs, allow);
                }
                allow &= ~rmask(rb);
            }
        else if ((op == LIR_add||op == LIR_iaddp) && lhs->isop(LIR_alloc) && rhs->isconst()) {
            // add alloc+const, use lea
            Register rr = prepResultReg(ins, allow);
            int d = findMemFor(lhs) + rhs->imm32();
            ADD(FP, L2, rr);
            SET32(d, L2);
        }

        Register rr = prepResultReg(ins, allow);
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a register assigned.
        Register ra = ( lhs->isUnusedOrHasUnknownReg()
                      ? findSpecificRegFor(lhs, rr)
                      : lhs->getReg() );

        if (forceReg)
            {
                if (lhs == rhs)
                    rb = ra;

                if (op == LIR_add || op == LIR_iaddp)
                    ADDCC(rr, rb, rr);
                else if (op == LIR_sub)
                    SUBCC(rr, rb, rr);
                else if (op == LIR_mul)
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
                int c = rhs->imm32();
                if (op == LIR_add || op == LIR_iaddp) {
                    ADDCC(rr, L2, rr);
                } else if (op == LIR_sub) {
                    SUBCC(rr, L2, rr);
                } else if (op == LIR_and)
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
        Register rr = prepResultReg(ins, GpRegs);

        LIns* lhs = ins->oprnd1();
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a register assigned.
        Register ra = ( lhs->isUnusedOrHasUnknownReg()
                      ? findSpecificRegFor(lhs, rr)
                      : lhs->getReg() );

        if (op == LIR_not)
            ORN(G0, rr, rr);
        else
            SUB(G0, rr, rr);

        if ( rr != ra )
            ORI(ra, 0, rr);
    }

    void Assembler::asm_ld(LInsp ins)
    {
        underrunProtect(12);
        LOpcode op = ins->opcode();
        LIns* base = ins->oprnd1();
        int d = ins->disp();
        Register rr = prepResultReg(ins, GpRegs);
        Register ra = getBaseReg(base, d, GpRegs);
        if (op == LIR_ldcb) {
            LDUB32(ra, d, rr);
        } else if (op == LIR_ldcs) {
            LDUH32(ra, d, rr);
        } else {
            LDSW32(ra, d, rr);
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
        NanoAssert(op == LIR_qcmov || (!iftrue->isQuad() && !iffalse->isQuad()));

        const Register rr = prepResultReg(ins, GpRegs);

        // this code assumes that neither LD nor MR nor MRcc set any of the condition flags.
        // (This is true on Intel, is it true on all architectures?)
        const Register iffalsereg = findRegFor(iffalse, GpRegs & ~rmask(rr));
        if (op == LIR_cmov) {
            switch (condval->opcode()) {
                // note that these are all opposites...
            case LIR_eq:  MOVNE (iffalsereg, 1, 0, 0, rr); break;
            case LIR_ov:  MOVVC (iffalsereg, 1, 0, 0, rr); break;
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
        } else if (op == LIR_qcmov) {
            NanoAssert(0);
        }
        /*const Register iftruereg =*/ findSpecificRegFor(iftrue, rr);
        asm_cmp(condval);
    }

    void Assembler::asm_qhi(LInsp ins)
    {
        underrunProtect(12);
        Register rr = prepResultReg(ins, GpRegs);
        LIns *q = ins->oprnd1();
        int d = findMemFor(q);
        LDSW32(FP, d+4, rr);
    }

    void Assembler::asm_param(LInsp ins)
    {
        uint32_t a = ins->paramArg();
        uint32_t kind = ins->paramKind();
        //        prepResultReg(ins, rmask(argRegs[a]));
        if (kind == 0) {
            prepResultReg(ins, rmask(argRegs[a]));
        } else {
            prepResultReg(ins, rmask(savedRegs[a]));
        }
    }

    void Assembler::asm_int(LInsp ins)
    {
        underrunProtect(8);
        Register rr = prepResultReg(ins, GpRegs);
        int32_t val = ins->imm32();
        if (val == 0)
            XOR(rr, rr, rr);
        else
            SET32(val, rr);
    }

    void Assembler::asm_quad(LInsp ins)
    {
        underrunProtect(64);
        Register rr = ins->getReg();
        if (rr != UnknownReg)
            {
                // @todo -- add special-cases for 0 and 1
                _allocator.retire(rr);
                ins->setReg(UnknownReg);
                NanoAssert((rmask(rr) & FpRegs) != 0);
                findMemFor(ins);
                int d = disp(ins);
                LDDF32(FP, d, rr);
            }

        // @todo, if we used xor, ldsd, fldz, etc above, we don't need mem here
        int d = disp(ins);
        freeRsrcOf(ins, false);
        if (d)
            {
                STW32(L2, d+4, FP);
                SET32(ins->imm64_0(), L2);
                STW32(L2, d, FP);
                SET32(ins->imm64_1(), L2);
            }
    }

    void Assembler::asm_qlo(LInsp ins)
    {
    }

    void Assembler::asm_fneg(LInsp ins)
    {
        underrunProtect(4);
        Register rr = prepResultReg(ins, FpRegs);
        LIns* lhs = ins->oprnd1();

        // lhs into reg, prefer same reg as result
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a different reg assigned
        Register ra = ( lhs->isUnusedOrHasUnknownReg()
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

        Register rr = prepResultReg(ins, allow);

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
        Register rr = prepResultReg(ins, FpRegs);
        int d = findMemFor(ins->oprnd1());
        FITOD(rr, rr);
        LDDF32(FP, d, rr);
    }

    Register Assembler::asm_prep_fcall(Reservation *unused, LInsp ins)
    {
        Register rr;
        if (ins->isUsed() && (rr = ins->getReg(), isKnownReg(rr)) && (rmask(rr) & FpRegs)) {
            evict(rr, ins);
        }
        return prepResultReg(ins, rmask(F0));
    }

    void Assembler::asm_u2f(LInsp ins)
    {
        underrunProtect(72);
        // where our result goes
        Register rr = prepResultReg(ins, FpRegs);
        Register rt = registerAlloc(FpRegs & ~(rmask(rr)));
        _allocator.addFree(rt);
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

    void Assembler::asm_nongp_copy(Register r, Register s)
    {
        underrunProtect(4);
        NanoAssert((rmask(r) & FpRegs) && (rmask(s) & FpRegs));
        FMOVD(s, r);
    }

    NIns * Assembler::asm_jmpcc(bool branchOnFalse, LIns *cond, NIns *targ)
    {
        NIns *at = 0;
        LOpcode condop = cond->opcode();
        NanoAssert(condop >= LIR_feq && condop <= LIR_fge);
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

    void Assembler::asm_setcc(Register r, LIns *cond)
    {
        underrunProtect(8);
        LOpcode condop = cond->opcode();
        NanoAssert(condop >= LIR_feq && condop <= LIR_fge);
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
        asm_fcmp(cond);
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

    verbose_only(
    void Assembler::asm_inc_m32(uint32_t* pCtr)
    {
        // TODO(asm_inc_m32);
    }
    )

    void Assembler::nativePageReset()
    {
    }

    Register Assembler::asm_binop_rhs_reg(LInsp ins)
    {
        return UnknownReg;
    }

    void Assembler::nativePageSetup()
    {
        if (!_nIns)
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
        if (!_nExitIns)
            codeAlloc(exitStart, exitEnd, _nExitIns verbose_only(, exitBytes));
    }

    void
    Assembler::underrunProtect(int n)
    {
        NIns *eip = _nIns;
        if (eip - n < (_inExit ? exitStart : codeStart)) {
            // We are done with the current page.  Tell Valgrind that new code
            // has been generated.
            if (_inExit)
                codeAlloc(exitStart, exitEnd, _nIns verbose_only(, exitBytes));
            else
                codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
            JMP_long_nocheck((intptr_t)eip);
        }
    }

    void Assembler::asm_ret(LInsp ins)
    {
        genEpilogue();
        assignSavedRegs();
        LIns *val = ins->oprnd1();
        if (ins->isop(LIR_ret)) {
            findSpecificRegFor(val, retRegs[0]);
        } else {
            findSpecificRegFor(val, F0);
        }
    }

    void Assembler::asm_promote(LIns *) {
        // i2q or u2q
        TODO(asm_promote);
    }

#endif /* FEATURE_NANOJIT */
}
