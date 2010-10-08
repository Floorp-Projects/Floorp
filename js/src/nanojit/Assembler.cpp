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

#ifdef FEATURE_NANOJIT

#ifdef VMCFG_VTUNE
#include "../core/CodegenLIR.h"
#endif

#ifdef _MSC_VER
    // disable some specific warnings which are normally useful, but pervasive in the code-gen macros
    #pragma warning(disable:4310) // cast truncates constant value
#endif

#ifdef VMCFG_VTUNE
namespace vtune {
    using namespace nanojit;
    void vtuneStart(void*, NIns*);
    void vtuneEnd(void*, NIns*);
    void vtuneLine(void*, int, NIns*);
    void vtuneFile(void*, void*);
}
using namespace vtune;
#endif // VMCFG_VTUNE


namespace nanojit
{
    /**
     * Need the following:
     *
     *    - merging paths ( build a graph? ), possibly use external rep to drive codegen
     */
    Assembler::Assembler(CodeAlloc& codeAlloc, Allocator& dataAlloc, Allocator& alloc, AvmCore* core, LogControl* logc, const Config& config)
        : codeList(NULL)
        , alloc(alloc)
        , _codeAlloc(codeAlloc)
        , _dataAlloc(dataAlloc)
        , _thisfrag(NULL)
        , _branchStateMap(alloc)
        , _patches(alloc)
        , _labels(alloc)
        , _noise(NULL)
    #if NJ_USES_IMMD_POOL
        , _immDPool(alloc)
    #endif
        , _epilogue(NULL)
        , _err(None)
    #if PEDANTIC
        , pedanticTop(NULL)
    #endif
    #ifdef VMCFG_VTUNE
        , vtuneHandle(NULL)
    #endif
        , _config(config)
    {
        nInit(core);
        (void)logc;
        verbose_only( _logc = logc; )
        verbose_only( _outputCache = 0; )
        verbose_only( outline[0] = '\0'; )
        verbose_only( outlineEOL[0] = '\0'; )

        reset();
    }

    // Per-opcode register hint table.  Default to no hints for all
    // instructions.  It's not marked const because individual back-ends can
    // install hint values for opcodes of interest in nInit().
    RegisterMask Assembler::nHints[LIR_sentinel+1] = {
#define OP___(op, number, repKind, retType, isCse) \
        0,
#include "LIRopcode.tbl"
#undef OP___
        0
    };

#ifdef _DEBUG

    /*static*/ LIns* const AR::BAD_ENTRY = (LIns*)0xdeadbeef;

    void AR::validateQuick()
    {
        NanoAssert(_highWaterMark < NJ_MAX_STACK_ENTRY);
        NanoAssert(_entries[0] == NULL);
        // Only check a few entries around _highWaterMark.
        uint32_t const RADIUS = 4;
        uint32_t const lo = (_highWaterMark > 1 + RADIUS ? _highWaterMark - RADIUS : 1);
        uint32_t const hi = (_highWaterMark + 1 + RADIUS < NJ_MAX_STACK_ENTRY ? _highWaterMark + 1 + RADIUS : NJ_MAX_STACK_ENTRY);
        for (uint32_t i = lo; i <= _highWaterMark; ++i)
            NanoAssert(_entries[i] != BAD_ENTRY);
        for (uint32_t i = _highWaterMark+1; i < hi; ++i)
            NanoAssert(_entries[i] == BAD_ENTRY);
    }

    void AR::validateFull()
    {
        NanoAssert(_highWaterMark < NJ_MAX_STACK_ENTRY);
        NanoAssert(_entries[0] == NULL);
        for (uint32_t i = 1; i <= _highWaterMark; ++i)
            NanoAssert(_entries[i] != BAD_ENTRY);
        for (uint32_t i = _highWaterMark+1; i < NJ_MAX_STACK_ENTRY; ++i)
            NanoAssert(_entries[i] == BAD_ENTRY);
    }

    void AR::validate()
    {
        static uint32_t validateCounter = 0;
        if (++validateCounter >= 100)
        {
            validateFull();
            validateCounter = 0;
        }
        else
        {
            validateQuick();
        }
    }

#endif

    inline void AR::clear()
    {
        _highWaterMark = 0;
        NanoAssert(_entries[0] == NULL);
    #ifdef _DEBUG
        for (uint32_t i = 1; i < NJ_MAX_STACK_ENTRY; ++i)
            _entries[i] = BAD_ENTRY;
    #endif
    }

    bool AR::Iter::next(LIns*& ins, uint32_t& nStackSlots, int32_t& arIndex)
    {
        while (_i <= _ar._highWaterMark) {
            ins = _ar._entries[_i];
            if (ins) {
                arIndex = _i;
                nStackSlots = nStackSlotsFor(ins);
                _i += nStackSlots;
                return true;
            }
            _i++;
        }
        ins = NULL;
        nStackSlots = 0;
        arIndex = 0;
        return false;
    }

    void Assembler::arReset()
    {
        _activation.clear();
        _branchStateMap.clear();
        _patches.clear();
        _labels.clear();
    #if NJ_USES_IMMD_POOL
        _immDPool.clear();
    #endif
    }

    void Assembler::registerResetAll()
    {
        nRegisterResetAll(_allocator);
        _allocator.managed = _allocator.free;

        // At start, should have some registers free and none active.
        NanoAssert(0 != _allocator.free);
        NanoAssert(0 == _allocator.activeMask());
#ifdef NANOJIT_IA32
        debug_only(_fpuStkDepth = 0; )
#endif
    }

    // Legend for register sets: A = allowed, P = preferred, F = free, S = SavedReg.
    //
    // Finds a register in 'setA___' to store the result of 'ins' (one from
    // 'set_P__' if possible), evicting one if necessary.  Doesn't consider
    // the prior state of 'ins'.
    //
    // Nb: 'setA___' comes from the instruction's use, 'set_P__' comes from its def.
    // Eg. in 'add(call(...), ...)':
    //     - the call's use means setA___==GpRegs;
    //     - the call's def means set_P__==rmask(retRegs[0]).
    //
    Register Assembler::registerAlloc(LIns* ins, RegisterMask setA___, RegisterMask set_P__)
    {
        Register r;
        RegisterMask set__F_ = _allocator.free;
        RegisterMask setA_F_ = setA___ & set__F_;

        if (setA_F_) {
            RegisterMask set___S = SavedRegs;
            RegisterMask setA_FS = setA_F_ & set___S;
            RegisterMask setAPF_ = setA_F_ & set_P__;
            RegisterMask setAPFS = setA_FS & set_P__;
            RegisterMask set;

            if      (setAPFS) set = setAPFS;
            else if (setAPF_) set = setAPF_;
            else if (setA_FS) set = setA_FS;
            else              set = setA_F_;

            r = nRegisterAllocFromSet(set);
            _allocator.addActive(r, ins);
            ins->setReg(r);
        } else {
            // Nothing free, steal one.
            // LSRA says pick the one with the furthest use.
            LIns* vic = findVictim(setA___);
            NanoAssert(vic->isInReg());
            r = vic->getReg();

            evict(vic);

            // r ends up staying active, but the LIns defining it changes.
            _allocator.removeFree(r);
            _allocator.addActive(r, ins);
            ins->setReg(r);
        }

        return r;
    }

    // Finds a register in 'allow' to store a temporary value (one not
    // associated with a particular LIns), evicting one if necessary.  The
    // returned register is marked as being free and so can only be safely
    // used for code generation purposes until the regstate is next inspected
    // or updated.
    Register Assembler::registerAllocTmp(RegisterMask allow)
    {
        LIns dummyIns;
        Register r = registerAlloc(&dummyIns, allow, /*prefer*/0);

        // Mark r as free, ready for use as a temporary value.
        _allocator.removeActive(r);
        _allocator.addFree(r);
        return r;
    }

    void Assembler::codeAlloc(NIns *&start, NIns *&end, NIns *&eip
                              verbose_only(, size_t &nBytes))
    {
        // save the block we just filled
        if (start)
            CodeAlloc::add(codeList, start, end);

        // CodeAlloc contract: allocations never fail
        _codeAlloc.alloc(start, end);
        verbose_only( nBytes += (end - start) * sizeof(NIns); )
        NanoAssert(uintptr_t(end) - uintptr_t(start) >= (size_t)LARGEST_UNDERRUN_PROT);
        eip = end;
    }

    void Assembler::reset()
    {
        _nIns = 0;
        _nExitIns = 0;
        codeStart = codeEnd = 0;
        exitStart = exitEnd = 0;
        codeList = 0;

        nativePageReset();
        registerResetAll();
        arReset();
    }

    #ifdef _DEBUG
    void Assembler::pageValidate()
    {
        if (error()) return;
        // This may be a normal code chunk or an exit code chunk.
        NanoAssertMsg(codeStart <= _nIns && _nIns <= codeEnd,
                     "Native instruction pointer overstep paging bounds; check overrideProtect for last instruction");
    }
    #endif

    #ifdef _DEBUG

    bool AR::isValidEntry(uint32_t idx, LIns* ins) const
    {
        return idx > 0 && idx <= _highWaterMark && _entries[idx] == ins;
    }

    void AR::checkForResourceConsistency(const RegAlloc& regs)
    {
        validate();
        for (uint32_t i = 1; i <= _highWaterMark; ++i)
        {
            LIns* ins = _entries[i];
            if (!ins)
                continue;
            uint32_t arIndex = ins->getArIndex();
            NanoAssert(arIndex != 0);
            if (ins->isop(LIR_allocp)) {
                int const n = i + (ins->size()>>2);
                for (int j=i+1; j < n; j++) {
                    NanoAssert(_entries[j]==ins);
                }
                NanoAssert(arIndex == (uint32_t)n-1);
                i = n-1;
            }
            else if (ins->isQorD()) {
                NanoAssert(_entries[i + 1]==ins);
                i += 1; // skip high word
            }
            else {
                NanoAssertMsg(arIndex == i, "Stack record index mismatch");
            }
            NanoAssertMsg(!ins->isInReg() || regs.isConsistent(ins->getReg(), ins),
                          "Register record mismatch");
        }
    }

    void Assembler::resourceConsistencyCheck()
    {
        NanoAssert(!error());
#ifdef NANOJIT_IA32
        // Within the expansion of a single LIR instruction, we may use the x87
        // stack for unmanaged temporaries.  Otherwise, we do not use the x87 stack
        // as such, but use the top element alone as a single allocatable FP register.
        // Compensation code must be inserted to keep the stack balanced and avoid
        // overflow, and the mechanisms for this are rather fragile and IA32-specific.
        // The predicate below should hold between any pair of instructions within
        // a basic block, at labels, and just after a conditional branch.  Currently,
        // we enforce this condition between all pairs of instructions, but this is
        // overly restrictive, and would fail if we did not generate unreachable x87
        // stack pops following unconditional branches.
        NanoAssert((_allocator.active[REGNUM(FST0)] && _fpuStkDepth == -1) ||
                   (!_allocator.active[REGNUM(FST0)] && _fpuStkDepth == 0));
#endif
        _activation.checkForResourceConsistency(_allocator);
        registerConsistencyCheck();
    }

    void Assembler::registerConsistencyCheck()
    {
        RegisterMask managed = _allocator.managed;
        for (Register r = lsReg(managed); managed; r = nextLsReg(managed, r)) {
            // A register managed by register allocation must be either
            // free or active, but not both.
            if (_allocator.isFree(r)) {
                NanoAssertMsgf(_allocator.getActive(r)==0,
                    "register %s is free but assigned to ins", gpn(r));
            } else {
                // An LIns defining a register must have that register in
                // its reservation.
                LIns* ins = _allocator.getActive(r);
                NanoAssert(ins);
                NanoAssertMsg(r == ins->getReg(), "Register record mismatch");
            }
        }

        RegisterMask not_managed = ~_allocator.managed;
        for (Register r = lsReg(not_managed); not_managed; r = nextLsReg(not_managed, r)) {
            // A register not managed by register allocation must be
            // neither free nor active.
            if (REGNUM(r) <= LastRegNum) {
                NanoAssert(!_allocator.isFree(r));
                NanoAssert(!_allocator.getActive(r));
            }
        }
    }
    #endif /* _DEBUG */

    void Assembler::findRegFor2(RegisterMask allowa, LIns* ia, Register& ra,
                                RegisterMask allowb, LIns* ib, Register& rb)
    {
        // There should be some overlap between 'allowa' and 'allowb', else
        // there's no point calling this function.
        NanoAssert(allowa & allowb);

        if (ia == ib) {
            ra = rb = findRegFor(ia, allowa & allowb);  // use intersection(allowa, allowb)

        } else if (ib->isInRegMask(allowb)) {
            // 'ib' is already in an allowable reg -- don't let it get evicted
            // when finding 'ra'.
            rb = ib->getReg();
            ra = findRegFor(ia, allowa & ~rmask(rb));

        } else {
            ra = findRegFor(ia, allowa);
            rb = findRegFor(ib, allowb & ~rmask(ra));
        }
    }

    Register Assembler::findSpecificRegFor(LIns* i, Register w)
    {
        return findRegFor(i, rmask(w));
    }

    // Like findRegFor(), but called when the LIns is used as a pointer.  It
    // doesn't have to be called, findRegFor() can still be used, but it can
    // optimize the LIR_allocp case by indexing off FP, thus saving the use of
    // a GpReg.
    //
    Register Assembler::getBaseReg(LIns* base, int &d, RegisterMask allow)
    {
    #if !PEDANTIC
        if (base->isop(LIR_allocp)) {
            // The value of a LIR_allocp is a pointer to its stack memory,
            // which is always relative to FP.  So we can just return FP if we
            // also adjust 'd' (and can do so in a valid manner).  Or, in the
            // PEDANTIC case, we can just assign a register as normal;
            // findRegFor() will allocate the stack memory for LIR_allocp if
            // necessary.
            d += findMemFor(base);
            return FP;
        }
    #else
        (void) d;
    #endif
        return findRegFor(base, allow);
    }

    // Like findRegFor2(), but used for stores where the base value has the
    // same type as the stored value, eg. in asm_store32() on 32-bit platforms
    // and asm_store64() on 64-bit platforms.  Similar to getBaseReg(),
    // findRegFor2() can be called instead, but this function can optimize the
    // case where the base value is a LIR_allocp.
    void Assembler::getBaseReg2(RegisterMask allowValue, LIns* value, Register& rv,
                                RegisterMask allowBase, LIns* base, Register& rb, int &d)
    {
    #if !PEDANTIC
        if (base->isop(LIR_allocp)) {
            rb = FP;
            d += findMemFor(base);
            rv = findRegFor(value, allowValue);
            return;
        }
    #else
        (void) d;
    #endif
        findRegFor2(allowValue, value, rv, allowBase, base, rb);
    }

    RegisterMask Assembler::hint(LIns* ins)
    {
        RegisterMask prefer = nHints[ins->opcode()];
        return (prefer == PREFER_SPECIAL) ? nHint(ins) : prefer;
    }

    // Finds a register in 'allow' to hold the result of 'ins'.  Used when we
    // encounter a use of 'ins'.  The actions depend on the prior regstate of
    // 'ins':
    // - If the result of 'ins' is not in any register, we find an allowed
    //   one, evicting one if necessary.
    // - If the result of 'ins' is already in an allowed register, we use that.
    // - If the result of 'ins' is already in a not-allowed register, we find an
    //   allowed one and move it.
    //
    Register Assembler::findRegFor(LIns* ins, RegisterMask allow)
    {
        if (ins->isop(LIR_allocp)) {
            // Never allocate a reg for this without stack space too.
            findMemFor(ins);
        }

        Register r;

        if (!ins->isInReg()) {
            // 'ins' isn't in a register (must be in a spill slot or nowhere).
            r = registerAlloc(ins, allow, hint(ins));

        } else if (rmask(r = ins->getReg()) & allow) {
            // 'ins' is in an allowed register.
            _allocator.useActive(r);

        } else {
            // 'ins' is in a register (r) that's not in 'allow'.
#ifdef NANOJIT_IA32
            if (((rmask(r)&XmmRegs) && !(allow&XmmRegs)) ||
                ((rmask(r)&x87Regs) && !(allow&x87Regs)))
            {
                // x87 <-> xmm copy required
                //_nvprof("fpu-evict",1);
                evict(ins);
                r = registerAlloc(ins, allow, hint(ins));
            } else
#elif defined(NANOJIT_PPC) || defined(NANOJIT_MIPS)
            if (((rmask(r)&GpRegs) && !(allow&GpRegs)) ||
                ((rmask(r)&FpRegs) && !(allow&FpRegs)))
            {
                evict(ins);
                r = registerAlloc(ins, allow, hint(ins));
            } else
#endif
            {
                // The post-state register holding 'ins' is 's', the pre-state
                // register holding 'ins' is 'r'.  For example, if s=eax and
                // r=ecx:
                //
                // pre-state:   ecx(ins)
                // instruction: mov eax, ecx
                // post-state:  eax(ins)
                //
                Register s = r;
                _allocator.retire(r);
                r = registerAlloc(ins, allow, hint(ins));

                // 'ins' is in 'allow', in register r (different to the old r);
                //  s is the old r.
                if ((rmask(s) & GpRegs) && (rmask(r) & GpRegs)) {
                    MR(s, r);   // move 'ins' from its pre-state reg (r) to its post-state reg (s)
                } else {
                    asm_nongp_copy(s, r);
                }
            }
        }

        return r;
    }

    // Like findSpecificRegFor(), but only for when 'r' is known to be free
    // and 'ins' is known to not already have a register allocated.  Updates
    // the regstate (maintaining the invariants) but does not generate any
    // code.  The return value is redundant, always being 'r', but it's
    // sometimes useful to have it there for assignments.
    Register Assembler::findSpecificRegForUnallocated(LIns* ins, Register r)
    {
        if (ins->isop(LIR_allocp)) {
            // never allocate a reg for this w/out stack space too
            findMemFor(ins);
        }

        NanoAssert(!ins->isInReg());
        NanoAssert(_allocator.free & rmask(r));

        ins->setReg(r);
        _allocator.removeFree(r);
        _allocator.addActive(r, ins);

        return r;
    }

#if NJ_USES_IMMD_POOL
    const uint64_t* Assembler::findImmDFromPool(uint64_t q)
    {
        uint64_t* p = _immDPool.get(q);
        if (!p)
        {
            p = new (_dataAlloc) uint64_t;
            *p = q;
            _immDPool.put(q, p);
        }
        return p;
    }
#endif

    int Assembler::findMemFor(LIns *ins)
    {
#if NJ_USES_IMMD_POOL
        NanoAssert(!ins->isImmD());
#endif
        if (!ins->isInAr()) {
            uint32_t const arIndex = arReserve(ins);
            ins->setArIndex(arIndex);
            NanoAssert(_activation.isValidEntry(ins->getArIndex(), ins) == (arIndex != 0));
        }
        return arDisp(ins);
    }

    // XXX: this function is dangerous and should be phased out;
    // See bug 513615.  Calls to it should replaced it with a
    // prepareResultReg() / generate code / freeResourcesOf() sequence.
    Register Assembler::deprecated_prepResultReg(LIns *ins, RegisterMask allow)
    {
#ifdef NANOJIT_IA32
        // We used to have to worry about possibly popping the x87 stack here.
        // But this function is no longer used on i386, and this assertion
        // ensures that.
        NanoAssert(0);
#endif
        Register r = findRegFor(ins, allow);
        deprecated_freeRsrcOf(ins);
        return r;
    }

    // Finds a register in 'allow' to hold the result of 'ins'.  Also
    // generates code to spill the result if necessary.  Called just prior to
    // generating the code for 'ins' (because we generate code backwards).
    //
    // An example where no spill is necessary.  Lines marked '*' are those
    // done by this function.
    //
    //   regstate:  R
    //   asm:       define res into r
    // * regstate:  R + r(res)
    //              ...
    //   asm:       use res in r
    //
    // An example where a spill is necessary.
    //
    //   regstate:  R
    //   asm:       define res into r
    // * regstate:  R + r(res)
    // * asm:       spill res from r
    //   regstate:  R
    //              ...
    //   asm:       restore res into r2
    //   regstate:  R + r2(res) + other changes from "..."
    //   asm:       use res in r2
    //
    Register Assembler::prepareResultReg(LIns *ins, RegisterMask allow)
    {
        // At this point, we know the result of 'ins' is used later in the
        // code, unless it is a call to an impure function that must be
        // included for effect even though its result is ignored.  It may have
        // had to be evicted, in which case the restore will have already been
        // generated, so we now generate the spill.  QUERY: Is there any attempt
        // to elide the spill if we know that all restores can be rematerialized?
#ifdef NANOJIT_IA32
        const bool notInFST0 = (!ins->isInReg() || ins->getReg() != FST0);
        Register r = findRegFor(ins, allow);
        // If the result register is FST0, but FST0 is not in the post-regstate,
        // then we must pop the x87 stack.  This may occur because the result is
        // unused, or because it has been stored to a spill slot or an XMM register.
        const bool needPop = notInFST0 && (r == FST0);
        const bool didSpill = asm_maybe_spill(ins, needPop);
        if (!didSpill && needPop) {
            // If the instruction is spilled, then the pop will have already
            // been performed by the store to the stack slot.  Otherwise, we
            // must pop now.  This may occur when the result of a LIR_calld
            // to an impure (side-effecting) function is not used.
            FSTP(FST0);
        }
#else
        Register r = findRegFor(ins, allow);
        asm_maybe_spill(ins, false);
#endif
        return r;
    }

    bool Assembler::asm_maybe_spill(LIns* ins, bool pop)
    {
        if (ins->isInAr()) {
            int d = arDisp(ins);
            Register r = ins->getReg();
            verbose_only( RefBuf b;
                          if (_logc->lcbits & LC_Native) {
                             setOutputForEOL("  <= spill %s",
                             _thisfrag->lirbuf->printer->formatRef(&b, ins)); } )
#ifdef NANOJIT_IA32
            asm_spill(r, d, pop);
#else
            (void)pop;
            asm_spill(r, d, ins->isQorD());
#endif
            return true;
        }
        return false;
    }

    // XXX: This function is error-prone and should be phased out; see bug 513615.
    void Assembler::deprecated_freeRsrcOf(LIns *ins)
    {
        if (ins->isInReg()) {
            asm_maybe_spill(ins, /*pop*/false);
            _allocator.retire(ins->getReg());   // free any register associated with entry
            ins->clearReg();
        }
        if (ins->isInAr()) {
            arFree(ins);                        // free any AR space associated with entry
            ins->clearArIndex();
        }
    }

    // Frees all record of registers and spill slots used by 'ins'.
    void Assembler::freeResourcesOf(LIns *ins)
    {
        if (ins->isInReg()) {
            _allocator.retire(ins->getReg());   // free any register associated with entry
            ins->clearReg();
        }
        if (ins->isInAr()) {
            arFree(ins);                        // free any AR space associated with entry
            ins->clearArIndex();
        }
    }

    // Frees 'r' in the RegAlloc regstate, if it's not already free.
    void Assembler::evictIfActive(Register r)
    {
        if (LIns* vic = _allocator.getActive(r)) {
            NanoAssert(vic->getReg() == r);
            evict(vic);
        }
    }

    // Frees 'r' (which currently holds the result of 'vic') in the regstate.
    // An example:
    //
    //   pre-regstate:  eax(ld1)
    //   instruction:   mov ebx,-4(ebp) <= restore add1   # %ebx is dest
    //   post-regstate: eax(ld1) ebx(add1)
    //
    // At run-time we are *restoring* 'add1' into %ebx, hence the call to
    // asm_restore().  But at regalloc-time we are moving backwards through
    // the code, so in that sense we are *evicting* 'add1' from %ebx.
    //
    void Assembler::evict(LIns* vic)
    {
        // Not free, need to steal.
        Register r = vic->getReg();

        NanoAssert(!_allocator.isFree(r));
        NanoAssert(vic == _allocator.getActive(r));

        verbose_only( RefBuf b;
                      if (_logc->lcbits & LC_Native) {
                        setOutputForEOL("  <= restore %s",
                        _thisfrag->lirbuf->printer->formatRef(&b, vic)); } )
        asm_restore(vic, r);

        _allocator.retire(r);
        vic->clearReg();

        // At this point 'vic' is unused (if rematerializable), or in a spill
        // slot (if not).
    }

    // If we have this:
    //
    //   W = ld(addp(B, lshp(I, k)))[d] , where int(1) <= k <= int(3)
    //
    // then we set base=B, index=I, scale=k.
    //
    // Otherwise, we must have this:
    //
    //   W = ld(addp(B, I))[d]
    //
    // and we set base=B, index=I, scale=0.
    //
    void Assembler::getBaseIndexScale(LIns* addp, LIns** base, LIns** index, int* scale)
    {
        NanoAssert(addp->isop(LIR_addp));

        *base = addp->oprnd1();
        LIns* rhs = addp->oprnd2();
        int k;

        if (rhs->opcode() == LIR_lshp && rhs->oprnd2()->isImmI() &&
            (k = rhs->oprnd2()->immI(), (1 <= k && k <= 3)))
        {
            *index = rhs->oprnd1();
            *scale = k;
        } else {
            *index = rhs;
            *scale = 0;
        }
    }
    void Assembler::patch(GuardRecord *lr)
    {
        if (!lr->jmp) // the guard might have been eliminated as redundant
            return;
        Fragment *frag = lr->exit->target;
        NanoAssert(frag->fragEntry != 0);
        nPatchBranch((NIns*)lr->jmp, frag->fragEntry);
        CodeAlloc::flushICache(lr->jmp, LARGEST_BRANCH_PATCH);
        verbose_only(verbose_outputf("patching jump at %p to target %p\n",
            lr->jmp, frag->fragEntry);)
    }

    void Assembler::patch(SideExit *exit)
    {
        GuardRecord *rec = exit->guards;
        NanoAssert(rec);
        while (rec) {
            patch(rec);
            rec = rec->next;
        }
    }

#ifdef NANOJIT_IA32
    void Assembler::patch(SideExit* exit, SwitchInfo* si)
    {
        for (GuardRecord* lr = exit->guards; lr; lr = lr->next) {
            Fragment *frag = lr->exit->target;
            NanoAssert(frag->fragEntry != 0);
            si->table[si->index] = frag->fragEntry;
        }
    }
#endif

    NIns* Assembler::asm_exit(LIns* guard)
    {
        SideExit *exit = guard->record()->exit;
        NIns* at = 0;
        if (!_branchStateMap.get(exit))
        {
            at = asm_leave_trace(guard);
        }
        else
        {
            RegAlloc* captured = _branchStateMap.get(exit);
            intersectRegisterState(*captured);
            at = exit->target->fragEntry;
            NanoAssert(at != 0);
            _branchStateMap.remove(exit);
        }
        return at;
    }

    NIns* Assembler::asm_leave_trace(LIns* guard)
    {
        verbose_only( verbose_outputf("----------------------------------- ## END exit block %p", guard);)

        // This point is unreachable.  So free all the registers.  If an
        // instruction has a stack entry we will leave it alone, otherwise we
        // free it entirely.  intersectRegisterState() will restore.
        RegAlloc capture = _allocator;
        releaseRegisters();

        swapCodeChunks();
        _inExit = true;

#ifdef NANOJIT_IA32
        debug_only( _sv_fpuStkDepth = _fpuStkDepth; _fpuStkDepth = 0; )
#endif

        nFragExit(guard);

        // Restore the callee-saved register and parameters.
        assignSavedRegs();
        assignParamRegs();

        intersectRegisterState(capture);

        // this can be useful for breaking whenever an exit is taken
        //INT3();
        //NOP();

        // we are done producing the exit logic for the guard so demark where our exit block code begins
        NIns* jmpTarget = _nIns;     // target in exit path for our mainline conditional jump

        // swap back pointers, effectively storing the last location used in the exit path
        swapCodeChunks();
        _inExit = false;

        //verbose_only( verbose_outputf("         LIR_xt/xf swapCodeChunks, _nIns is now %08X(%08X), _nExitIns is now %08X(%08X)",_nIns, *_nIns,_nExitIns,*_nExitIns) );
        verbose_only( verbose_outputf("%p:", jmpTarget);)
        verbose_only( verbose_outputf("----------------------------------- ## BEGIN exit block (LIR_xt|LIR_xf)") );

#ifdef NANOJIT_IA32
        NanoAssertMsgf(_fpuStkDepth == _sv_fpuStkDepth, "LIR_xtf, _fpuStkDepth=%d, expect %d",_fpuStkDepth, _sv_fpuStkDepth);
        debug_only( _fpuStkDepth = _sv_fpuStkDepth; _sv_fpuStkDepth = 9999; )
#endif

        return jmpTarget;
    }

    void Assembler::compile(Fragment* frag, Allocator& alloc, bool optimize verbose_only(, LInsPrinter* printer))
    {
        verbose_only(
        bool anyVerb = (_logc->lcbits & 0xFFFF & ~LC_FragProfile) > 0;
        bool liveVerb = (_logc->lcbits & 0xFFFF & LC_Liveness) > 0;
        )

        /* BEGIN decorative preamble */
        verbose_only(
        if (anyVerb) {
            _logc->printf("========================================"
                          "========================================\n");
            _logc->printf("=== BEGIN LIR::compile(%p, %p)\n",
                          (void*)this, (void*)frag);
            _logc->printf("===\n");
        })
        /* END decorative preamble */

        verbose_only( if (liveVerb) {
            _logc->printf("\n");
            _logc->printf("=== Results of liveness analysis:\n");
            _logc->printf("===\n");
            LirReader br(frag->lastIns);
            LirFilter* lir = &br;
            if (optimize) {
                StackFilter* sf = new (alloc) StackFilter(lir, alloc, frag->lirbuf->sp);
                lir = sf;
            }
            live(lir, alloc, frag, _logc);
        })

        /* Set up the generic text output cache for the assembler */
        verbose_only( StringList asmOutput(alloc); )
        verbose_only( _outputCache = &asmOutput; )

        beginAssembly(frag);
        if (error())
            return;

        //_logc->printf("recompile trigger %X kind %d\n", (int)frag, frag->kind);

        verbose_only( if (anyVerb) {
            _logc->printf("=== Translating LIR fragments into assembly:\n");
        })

        // now the the main trunk
        verbose_only( RefBuf b; )
        verbose_only( if (anyVerb) {
            _logc->printf("=== -- Compile trunk %s: begin\n", printer->formatAddr(&b, frag));
        })

        // Used for debug printing, if needed
        debug_only(ValidateReader *validate = NULL;)
        verbose_only(
        ReverseLister *pp_init = NULL;
        ReverseLister *pp_after_sf = NULL;
        )

        // The LIR passes through these filters as listed in this
        // function, viz, top to bottom.

        // set up backwards pipeline: assembler <- StackFilter <- LirReader
        LirFilter* lir = new (alloc) LirReader(frag->lastIns);

#ifdef DEBUG
        // VALIDATION
        validate = new (alloc) ValidateReader(lir);
        lir = validate;
#endif

        // INITIAL PRINTING
        verbose_only( if (_logc->lcbits & LC_ReadLIR) {
        pp_init = new (alloc) ReverseLister(lir, alloc, frag->lirbuf->printer, _logc,
                                    "Initial LIR");
        lir = pp_init;
        })

        // STACKFILTER
        if (optimize) {
            StackFilter* stackfilter = new (alloc) StackFilter(lir, alloc, frag->lirbuf->sp);
            lir = stackfilter;
        }

        verbose_only( if (_logc->lcbits & LC_AfterSF) {
        pp_after_sf = new (alloc) ReverseLister(lir, alloc, frag->lirbuf->printer, _logc,
                                                "After StackFilter");
        lir = pp_after_sf;
        })

        assemble(frag, lir);

        // If we were accumulating debug info in the various ReverseListers,
        // call finish() to emit whatever contents they have accumulated.
        verbose_only(
        if (pp_init)        pp_init->finish();
        if (pp_after_sf)    pp_after_sf->finish();
        )

        verbose_only( if (anyVerb) {
            _logc->printf("=== -- Compile trunk %s: end\n", printer->formatAddr(&b, frag));
        })

        endAssembly(frag);

        // Reverse output so that assembly is displayed low-to-high.
        // Up to this point, _outputCache has been non-NULL, and so has been
        // accumulating output.  Now we set it to NULL, traverse the entire
        // list of stored strings, and hand them a second time to output.
        // Since _outputCache is now NULL, outputf just hands these strings
        // directly onwards to _logc->printf.
        verbose_only( if (anyVerb) {
            _logc->printf("\n");
            _logc->printf("=== Aggregated assembly output: BEGIN\n");
            _logc->printf("===\n");
            _outputCache = 0;
            for (Seq<char*>* p = asmOutput.get(); p != NULL; p = p->tail) {
                char *str = p->head;
                outputf("  %s", str);
            }
            _logc->printf("===\n");
            _logc->printf("=== Aggregated assembly output: END\n");
        });

        if (error())
            frag->fragEntry = 0;

        verbose_only( frag->nCodeBytes += codeBytes; )
        verbose_only( frag->nExitBytes += exitBytes; )

        /* BEGIN decorative postamble */
        verbose_only( if (anyVerb) {
            _logc->printf("\n");
            _logc->printf("===\n");
            _logc->printf("=== END LIR::compile(%p, %p)\n",
                          (void*)this, (void*)frag);
            _logc->printf("========================================"
                          "========================================\n");
            _logc->printf("\n");
        });
        /* END decorative postamble */
    }

    void Assembler::beginAssembly(Fragment *frag)
    {
        verbose_only( codeBytes = 0; )
        verbose_only( exitBytes = 0; )

        reset();

        NanoAssert(codeList == 0);
        NanoAssert(codeStart == 0);
        NanoAssert(codeEnd == 0);
        NanoAssert(exitStart == 0);
        NanoAssert(exitEnd == 0);
        NanoAssert(_nIns == 0);
        NanoAssert(_nExitIns == 0);

        _thisfrag = frag;
        _inExit = false;

        setError(None);

        // native code gen buffer setup
        nativePageSetup();

        // make sure we got memory at least one page
        if (error()) return;

        _epilogue = NULL;

        nBeginAssembly();
    }

    void Assembler::assemble(Fragment* frag, LirFilter* reader)
    {
        if (error()) return;
        _thisfrag = frag;

        // check the fragment is starting out with a sane profiling state
        verbose_only( NanoAssert(frag->nStaticExits == 0); )
        verbose_only( NanoAssert(frag->nCodeBytes == 0); )
        verbose_only( NanoAssert(frag->nExitBytes == 0); )
        verbose_only( NanoAssert(frag->profCount == 0); )
        verbose_only( if (_logc->lcbits & LC_FragProfile)
                          NanoAssert(frag->profFragID > 0);
                      else
                          NanoAssert(frag->profFragID == 0); )

        _inExit = false;

        gen(reader);

        if (!error()) {
            // patch all branches
            NInsMap::Iter iter(_patches);
            while (iter.next()) {
                NIns* where = iter.key();
                LIns* target = iter.value();
                if (target->isop(LIR_jtbl)) {
                    // Need to patch up a whole jump table, 'where' is the table.
                    LIns *jtbl = target;
                    NIns** native_table = (NIns**) (void *) where;
                    for (uint32_t i = 0, n = jtbl->getTableSize(); i < n; i++) {
                        LabelState* lstate = _labels.get(jtbl->getTarget(i));
                        NIns* ntarget = lstate->addr;
                        if (ntarget) {
                            native_table[i] = ntarget;
                        } else {
                            setError(UnknownBranch);
                            break;
                        }
                    }
                } else {
                    // target is a label for a single-target branch
                    LabelState *lstate = _labels.get(target);
                    NIns* ntarget = lstate->addr;
                    if (ntarget) {
                        nPatchBranch(where, ntarget);
                    } else {
                        setError(UnknownBranch);
                        break;
                    }
                }
            }
        }
    }

    void Assembler::endAssembly(Fragment* frag)
    {
        // don't try to patch code if we are in an error state since we might have partially
        // overwritten the code cache already
        if (error()) {
            // something went wrong, release all allocated code memory
            _codeAlloc.freeAll(codeList);
            if (_nExitIns)
                _codeAlloc.free(exitStart, exitEnd);
            _codeAlloc.free(codeStart, codeEnd);
            codeList = NULL;
            return;
        }

        NIns* fragEntry = genPrologue();
        verbose_only( asm_output("[prologue]"); )

        debug_only(_activation.checkForResourceLeaks());

        NanoAssert(!_inExit);
        // save used parts of current block on fragment's code list, free the rest
#if defined(NANOJIT_ARM) || defined(NANOJIT_MIPS)
        // [codeStart, _nSlot) ... gap ... [_nIns, codeEnd)
        if (_nExitIns) {
            _codeAlloc.addRemainder(codeList, exitStart, exitEnd, _nExitSlot, _nExitIns);
            verbose_only( exitBytes -= (_nExitIns - _nExitSlot) * sizeof(NIns); )
        }
        _codeAlloc.addRemainder(codeList, codeStart, codeEnd, _nSlot, _nIns);
        verbose_only( codeBytes -= (_nIns - _nSlot) * sizeof(NIns); )
#else
        // [codeStart ... gap ... [_nIns, codeEnd))
        if (_nExitIns) {
            _codeAlloc.addRemainder(codeList, exitStart, exitEnd, exitStart, _nExitIns);
            verbose_only( exitBytes -= (_nExitIns - exitStart) * sizeof(NIns); )
        }
        _codeAlloc.addRemainder(codeList, codeStart, codeEnd, codeStart, _nIns);
        verbose_only( codeBytes -= (_nIns - codeStart) * sizeof(NIns); )
#endif

        // at this point all our new code is in the d-cache and not the i-cache,
        // so flush the i-cache on cpu's that need it.
        CodeAlloc::flushICache(codeList);

        // save entry point pointers
        frag->fragEntry = fragEntry;
        frag->setCode(_nIns);

#ifdef VMCFG_VTUNE
        if (vtuneHandle)
        {
            vtuneEnd(vtuneHandle, codeEnd);
            vtuneStart(vtuneHandle, _nIns);
        }
#endif

        PERFM_NVPROF("code", CodeAlloc::size(codeList));

#ifdef NANOJIT_IA32
        NanoAssertMsgf(_fpuStkDepth == 0,"_fpuStkDepth %d\n",_fpuStkDepth);
#endif

        debug_only( pageValidate(); )
        NanoAssert(_branchStateMap.isEmpty());
    }

    void Assembler::releaseRegisters()
    {
        RegisterMask active = _allocator.activeMask();
        for (Register r = lsReg(active); active; r = nextLsReg(active, r))
        {
            LIns *ins = _allocator.getActive(r);
            // Clear reg allocation, preserve stack allocation.
            _allocator.retire(r);
            NanoAssert(r == ins->getReg());
            ins->clearReg();
        }
    }

#ifdef PERFM
#define countlir_live() _nvprof("lir-live",1)
#define countlir_ret() _nvprof("lir-ret",1)
#define countlir_alloc() _nvprof("lir-alloc",1)
#define countlir_var() _nvprof("lir-var",1)
#define countlir_use() _nvprof("lir-use",1)
#define countlir_def() _nvprof("lir-def",1)
#define countlir_imm() _nvprof("lir-imm",1)
#define countlir_param() _nvprof("lir-param",1)
#define countlir_cmov() _nvprof("lir-cmov",1)
#define countlir_ld() _nvprof("lir-ld",1)
#define countlir_ldq() _nvprof("lir-ldq",1)
#define countlir_alu() _nvprof("lir-alu",1)
#define countlir_qjoin() _nvprof("lir-qjoin",1)
#define countlir_qlo() _nvprof("lir-qlo",1)
#define countlir_qhi() _nvprof("lir-qhi",1)
#define countlir_fpu() _nvprof("lir-fpu",1)
#define countlir_st() _nvprof("lir-st",1)
#define countlir_stq() _nvprof("lir-stq",1)
#define countlir_jmp() _nvprof("lir-jmp",1)
#define countlir_jcc() _nvprof("lir-jcc",1)
#define countlir_label() _nvprof("lir-label",1)
#define countlir_xcc() _nvprof("lir-xcc",1)
#define countlir_x() _nvprof("lir-x",1)
#define countlir_call() _nvprof("lir-call",1)
#define countlir_jtbl() _nvprof("lir-jtbl",1)
#else
#define countlir_live()
#define countlir_ret()
#define countlir_alloc()
#define countlir_var()
#define countlir_use()
#define countlir_def()
#define countlir_imm()
#define countlir_param()
#define countlir_cmov()
#define countlir_ld()
#define countlir_ldq()
#define countlir_alu()
#define countlir_qjoin()
#define countlir_qlo()
#define countlir_qhi()
#define countlir_fpu()
#define countlir_st()
#define countlir_stq()
#define countlir_jmp()
#define countlir_jcc()
#define countlir_label()
#define countlir_xcc()
#define countlir_x()
#define countlir_call()
#define countlir_jtbl()
#endif

    void Assembler::asm_jmp(LIns* ins, InsList& pending_lives)
    {
        NanoAssert((ins->isop(LIR_j) && !ins->oprnd1()) ||
                   (ins->isop(LIR_jf) && ins->oprnd1()->isImmI(0)) ||
                   (ins->isop(LIR_jt) && ins->oprnd1()->isImmI(1)));

        countlir_jmp();
        LIns* to = ins->getTarget();
        LabelState *label = _labels.get(to);
        // The jump is always taken so whatever register state we
        // have from downstream code, is irrelevant to code before
        // this jump.  So clear it out.  We will pick up register
        // state from the jump target, if we have seen that label.
        releaseRegisters();
#ifdef NANOJIT_IA32
        // Unreachable, so assume correct stack depth.
        debug_only( _fpuStkDepth = 0; )
#endif
        if (label && label->addr) {
            // Forward jump - pick up register state from target.
            unionRegisterState(label->regs);
#ifdef NANOJIT_IA32
            // Set stack depth according to the register state we just loaded,
            // negating the effect of any unreachable x87 stack pop that might
            // have been emitted by unionRegisterState().
            debug_only( _fpuStkDepth = (_allocator.getActive(FST0) ? -1 : 0); )
#endif
            JMP(label->addr);
        }
        else {
            // Backwards jump.
            handleLoopCarriedExprs(pending_lives);
            if (!label) {
                // save empty register state at loop header
                _labels.add(to, 0, _allocator);
            }
            else {
                intersectRegisterState(label->regs);
#ifdef NANOJIT_IA32
                debug_only( _fpuStkDepth = (_allocator.getActive(FST0) ? -1 : 0); )
#endif
            }
            JMP(0);
            _patches.put(_nIns, to);
        }
    }

    void Assembler::asm_jcc(LIns* ins, InsList& pending_lives)
    {
        bool branchOnFalse = (ins->opcode() == LIR_jf);
        LIns* cond = ins->oprnd1();
        if (cond->isImmI()) {
            if ((!branchOnFalse && !cond->immI()) || (branchOnFalse && cond->immI())) {
                // jmp never taken, not needed
            } else {
                asm_jmp(ins, pending_lives);    // jmp always taken
            }
            return;
        }

        // Changes to the logic below will likely need to be propagated to Assembler::asm_jov().

        countlir_jcc();
        LIns* to = ins->getTarget();
        LabelState *label = _labels.get(to);
        if (label && label->addr) {
            // Forward jump to known label.  Need to merge with label's register state.
            unionRegisterState(label->regs);
            asm_branch(branchOnFalse, cond, label->addr);
        }
        else {
            // Back edge.
            handleLoopCarriedExprs(pending_lives);
            if (!label) {
                // Evict all registers, most conservative approach.
                evictAllActiveRegs();
                _labels.add(to, 0, _allocator);
            }
            else {
                // Evict all registers, most conservative approach.
                intersectRegisterState(label->regs);
            }
            NIns *branch = asm_branch(branchOnFalse, cond, 0);
            _patches.put(branch,to);
        }
    }

    void Assembler::asm_jov(LIns* ins, InsList& pending_lives)
    {
        // The caller is responsible for countlir_* profiling, unlike
        // asm_jcc above.  The reason for this is that asm_jov may not be
        // be called if the instruction is dead, and it is our convention
        // to count such instructions anyway.
        LOpcode op = ins->opcode();
        LIns* to = ins->getTarget();
        LabelState *label = _labels.get(to);
        if (label && label->addr) {
            // forward jump to known label.  need to merge with label's register state.
            unionRegisterState(label->regs);
            asm_branch_ov(op, label->addr);
        }
        else {
            // back edge.
            handleLoopCarriedExprs(pending_lives);
            if (!label) {
                // evict all registers, most conservative approach.
                evictAllActiveRegs();
                _labels.add(to, 0, _allocator);
            }
            else {
                // evict all registers, most conservative approach.
                intersectRegisterState(label->regs);
            }
            NIns *branch = asm_branch_ov(op, 0);
            _patches.put(branch,to);
        }
    }

    void Assembler::asm_x(LIns* ins)
    {
        verbose_only( _thisfrag->nStaticExits++; )
        countlir_x();
        // Generate the side exit branch on the main trace.
        NIns *exit = asm_exit(ins);
        JMP(exit);
    }

    void Assembler::asm_xcc(LIns* ins)
    {
        LIns* cond = ins->oprnd1();
        if (cond->isImmI()) {
            if ((ins->isop(LIR_xt) && !cond->immI()) || (ins->isop(LIR_xf) && cond->immI())) {
                // guard never taken, not needed
            } else {
                asm_x(ins);     // guard always taken
            }
            return;
        }

        verbose_only( _thisfrag->nStaticExits++; )
        countlir_xcc();
        // We only support cmp with guard right now, also assume it is 'close'
        // and only emit the branch.
        NIns* exit = asm_exit(ins); // does intersectRegisterState()
        asm_branch(ins->opcode() == LIR_xf, cond, exit);
    }

    void Assembler::gen(LirFilter* reader)
    {
        NanoAssert(_thisfrag->nStaticExits == 0);

        InsList pending_lives(alloc);

        NanoAssert(!error());

        // What's going on here: we're visiting all the LIR instructions in
        // the buffer, working strictly backwards in buffer-order, and
        // generating machine instructions for them as we go.
        //
        // For each LIns, we first check if it's live.  If so we mark its
        // operands as also live, and then generate code for it *if
        // necessary*.  It may not be necessary if the instruction is an
        // expression and code has already been generated for all its uses in
        // combination with previously handled instructions (ins->isExtant()
        // will return false if this is so).

        // Note that the backwards code traversal can make register allocation
        // confusing.  (For example, we restore a value before we spill it!)
        // In particular, words like "before" and "after" must be used very
        // carefully -- their meaning at regalloc-time is opposite to their
        // meaning at run-time.  We use the term "pre-regstate" to refer to
        // the register allocation state that occurs prior to an instruction's
        // execution, and "post-regstate" to refer to the state that occurs
        // after an instruction's execution, e.g.:
        //
        //   pre-regstate:  ebx(ins)
        //   instruction:   mov eax, ebx     // mov dst, src
        //   post-regstate: eax(ins)
        //
        // At run-time, the instruction updates the pre-regstate into the
        // post-regstate (and these states are the real machine's regstates).
        // But when allocating registers, because we go backwards, the
        // pre-regstate is constructed from the post-regstate (and these
        // regstates are those stored in RegAlloc).
        //
        // One consequence of generating code backwards is that we tend to
        // both spill and restore registers as early (at run-time) as
        // possible;  this is good for tolerating memory latency.  If we
        // generated code forwards, we would expect to both spill and restore
        // registers as late (at run-time) as possible;  this might be better
        // for reducing register pressure.

        // The trace must end with one of these opcodes.  Mark it as live.
        NanoAssert(reader->finalIns()->isop(LIR_x)    ||
                   reader->finalIns()->isop(LIR_xtbl) ||
                   reader->finalIns()->isRet()        ||
                   isLiveOpcode(reader->finalIns()->opcode()));

        for (currIns = reader->read(); !currIns->isop(LIR_start); currIns = reader->read())
        {
            LIns* ins = currIns;        // give it a shorter name for local use

            if (!ins->isLive()) {
                NanoAssert(!ins->isExtant());
                continue;
            }

#ifdef NJ_VERBOSE
            // Output the post-regstate (registers and/or activation).
            // Because asm output comes in reverse order, doing it now means
            // it is printed after the LIR and native code, exactly when the
            // post-regstate should be shown.
            if ((_logc->lcbits & LC_Native) && (_logc->lcbits & LC_Activation))
                printActivationState();
            if ((_logc->lcbits & LC_Native) && (_logc->lcbits & LC_RegAlloc))
                printRegState();
#endif

            LOpcode op = ins->opcode();
            switch (op)
            {
                default:
                    NanoAssertMsgf(false, "unsupported LIR instruction: %d\n", op);
                    break;

                case LIR_regfence:
                    evictAllActiveRegs();
                    break;

                case LIR_livei:
                CASE64(LIR_liveq:)
                case LIR_lived: {
                    countlir_live();
                    LIns* op1 = ins->oprnd1();
                    op1->setResultLive();
                    // LIR_allocp's are meant to live until the point of the
                    // LIR_livep instruction, marking other expressions as
                    // live ensures that they remain so at loop bottoms.
                    // LIR_allocp areas require special treatment because they
                    // are accessed indirectly and the indirect accesses are
                    // invisible to the assembler, other than via LIR_livep.
                    // Other expression results are only accessed directly in
                    // ways that are visible to the assembler, so extending
                    // those expression's lifetimes past the last loop edge
                    // isn't necessary.
                    if (op1->isop(LIR_allocp)) {
                        findMemFor(op1);
                    } else {
                        pending_lives.add(ins);
                    }
                    break;
                }

                case LIR_reti:
                CASE64(LIR_retq:)
                case LIR_retd:
                    countlir_ret();
                    ins->oprnd1()->setResultLive();
                    asm_ret(ins);
                    break;

                // Allocate some stack space.  The value of this instruction
                // is the address of the stack space.
                case LIR_allocp:
                    countlir_alloc();
                    if (ins->isExtant()) {
                        NanoAssert(ins->isInAr());
                        if (ins->isInReg())
                            evict(ins);
                        freeResourcesOf(ins);
                    }
                    break;

                case LIR_immi:
                    countlir_imm();
                    if (ins->isExtant()) {
                        asm_immi(ins);
                    }
                    break;

#ifdef NANOJIT_64BIT
                case LIR_immq:
                    countlir_imm();
                    if (ins->isExtant()) {
                        asm_immq(ins);
                    }
                    break;
#endif
                case LIR_immd:
                    countlir_imm();
                    if (ins->isExtant()) {
                        asm_immd(ins);
                    }
                    break;

                case LIR_paramp:
                    countlir_param();
                    if (ins->isExtant()) {
                        asm_param(ins);
                    }
                    break;

#if NJ_SOFTFLOAT_SUPPORTED
                case LIR_hcalli: {
                    LIns* op1 = ins->oprnd1();
                    op1->setResultLive();
                    if (ins->isExtant()) {
                        // Return result of quad-call in register.
                        deprecated_prepResultReg(ins, rmask(retRegs[1]));
                        // If hi half was used, we must use the call to ensure it happens.
                        findSpecificRegFor(op1, retRegs[0]);
                    }
                    break;
                }

                case LIR_dlo2i:
                    countlir_qlo();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_qlo(ins);
                    }
                    break;

                case LIR_dhi2i:
                    countlir_qhi();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_qhi(ins);
                    }
                    break;

                case LIR_ii2d:
                    countlir_qjoin();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    if (ins->isExtant()) {
                        asm_qjoin(ins);
                    }
                    break;
#endif
                case LIR_cmovi:
                CASE64(LIR_cmovq:)
                case LIR_cmovd:
                    countlir_cmov();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    ins->oprnd3()->setResultLive();
                    if (ins->isExtant()) {
                        asm_cmov(ins);
                    }
                    break;

                case LIR_lduc2ui:
                case LIR_ldus2ui:
                case LIR_ldc2i:
                case LIR_lds2i:
                case LIR_ldi:
                    countlir_ld();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_load32(ins);
                    }
                    break;

                CASE64(LIR_ldq:)
                case LIR_ldd:
                case LIR_ldf2d:
                    countlir_ldq();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_load64(ins);
                    }
                    break;

                case LIR_negi:
                case LIR_noti:
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_neg_not(ins);
                    }
                    break;

#if defined NANOJIT_64BIT
                case LIR_addq:
                case LIR_subq:
                case LIR_andq:
                case LIR_lshq:
                case LIR_rshuq:
                case LIR_rshq:
                case LIR_orq:
                case LIR_xorq:
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    if (ins->isExtant()) {
                        asm_qbinop(ins);
                    }
                    break;
#endif

                case LIR_addi:
                case LIR_subi:
                case LIR_muli:
                case LIR_andi:
                case LIR_ori:
                case LIR_xori:
                case LIR_lshi:
                case LIR_rshi:
                case LIR_rshui:
                CASE86(LIR_divi:)
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    if (ins->isExtant()) {
                        asm_arith(ins);
                    }
                    break;

#if defined NANOJIT_IA32 || defined NANOJIT_X64
                CASE86(LIR_modi:)
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_arith(ins);
                    }
                    break;
#endif

                case LIR_negd:
                    countlir_fpu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_fneg(ins);
                    }
                    break;

                case LIR_addd:
                case LIR_subd:
                case LIR_muld:
                case LIR_divd:
                    countlir_fpu();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    if (ins->isExtant()) {
                        asm_fop(ins);
                    }
                    break;

                case LIR_i2d:
                    countlir_fpu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_i2d(ins);
                    }
                    break;

                case LIR_ui2d:
                    countlir_fpu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_ui2d(ins);
                    }
                    break;

                case LIR_d2i:
                    countlir_fpu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_d2i(ins);
                    }
                    break;

#ifdef NANOJIT_64BIT
                case LIR_i2q:
                case LIR_ui2uq:
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_ui2uq(ins);
                    }
                    break;

                case LIR_q2i:
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_q2i(ins);
                    }
                    break;

                case LIR_dasq:
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_dasq(ins);
                    }
                    break;

                case LIR_qasd:
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    if (ins->isExtant()) {
                        asm_qasd(ins);
                    }
                    break;
#endif
                case LIR_sti2c:
                case LIR_sti2s:
                case LIR_sti:
                    countlir_st();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    asm_store32(op, ins->oprnd1(), ins->disp(), ins->oprnd2());
                    break;

                CASE64(LIR_stq:)
                case LIR_std:
                case LIR_std2f: {
                    countlir_stq();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    LIns* value = ins->oprnd1();
                    LIns* base = ins->oprnd2();
                    int dr = ins->disp();
#if NJ_SOFTFLOAT_SUPPORTED
                    if (value->isop(LIR_ii2d) && op == LIR_std)
                    {
                        // This is correct for little-endian only.
                        asm_store32(LIR_sti, value->oprnd1(), dr, base);
                        asm_store32(LIR_sti, value->oprnd2(), dr+4, base);
                    }
                    else
#endif
                    {
                        asm_store64(op, value, dr, base);
                    }
                    break;
                }

                case LIR_j:
                    asm_jmp(ins, pending_lives);
                    break;

                case LIR_jt:
                case LIR_jf:
                    ins->oprnd1()->setResultLive();
                    asm_jcc(ins, pending_lives);
                    break;

                #if NJ_JTBL_SUPPORTED
                case LIR_jtbl: {
                    countlir_jtbl();
                    ins->oprnd1()->setResultLive();
                    // Multiway jump can contain both forward and backward jumps.
                    // Out of range indices aren't allowed or checked.
                    // Code after this jtbl instruction is unreachable.
                    releaseRegisters();
                    NanoAssert(_allocator.activeMask() == 0);

                    uint32_t count = ins->getTableSize();
                    bool has_back_edges = false;

                    // Merge the regstates of labels we have already seen.
                    for (uint32_t i = count; i-- > 0;) {
                        LIns* to = ins->getTarget(i);
                        LabelState *lstate = _labels.get(to);
                        if (lstate) {
                            unionRegisterState(lstate->regs);
                            verbose_only( RefBuf b; )
                            asm_output("   %u: [&%s]", i, _thisfrag->lirbuf->printer->formatRef(&b, to));
                        } else {
                            has_back_edges = true;
                        }
                    }
                    asm_output("forward edges");

                    // In a multi-way jump, the register allocator has no ability to deal
                    // with two existing edges that have conflicting register assignments, unlike
                    // a conditional branch where code can be inserted on the fall-through path
                    // to reconcile registers.  So, frontends *must* insert LIR_regfence at labels of
                    // forward jtbl jumps.  Check here to make sure no registers were picked up from
                    // any forward edges.
                    NanoAssert(_allocator.activeMask() == 0);

                    if (has_back_edges) {
                        handleLoopCarriedExprs(pending_lives);
                        // save merged (empty) register state at target labels we haven't seen yet
                        for (uint32_t i = count; i-- > 0;) {
                            LIns* to = ins->getTarget(i);
                            LabelState *lstate = _labels.get(to);
                            if (!lstate) {
                                _labels.add(to, 0, _allocator);
                                verbose_only( RefBuf b; )
                                asm_output("   %u: [&%s]", i, _thisfrag->lirbuf->printer->formatRef(&b, to));
                            }
                        }
                        asm_output("backward edges");
                    }

                    // Emit the jump instruction, which allocates 1 register for the jump index.
                    NIns** native_table = new (_dataAlloc) NIns*[count];
                    asm_output("[%p]:", (void*)native_table);
                    _patches.put((NIns*)native_table, ins);
                    asm_jtbl(ins, native_table);
                    break;
                }
                #endif

                case LIR_label: {
                    countlir_label();
                    LabelState *label = _labels.get(ins);
                    // add profiling inc, if necessary.
                    verbose_only( if (_logc->lcbits & LC_FragProfile) {
                        if (ins == _thisfrag->loopLabel)
                            asm_inc_m32(& _thisfrag->profCount);
                    })
                    if (!label) {
                        // label seen first, normal target of forward jump, save addr & allocator
                        _labels.add(ins, _nIns, _allocator);
                    }
                    else {
                        // we're at the top of a loop
                        NanoAssert(label->addr == 0);
                        //evictAllActiveRegs();
                        intersectRegisterState(label->regs);
                        label->addr = _nIns;
                    }
                    verbose_only(
                        RefBuf b;
                        if (_logc->lcbits & LC_Native) {
                            asm_output("[%s]", _thisfrag->lirbuf->printer->formatRef(&b, ins));
                    })
                    break;
                }

                case LIR_xbarrier:
                    break;

                case LIR_xtbl: {
                    ins->oprnd1()->setResultLive();
#ifdef NANOJIT_IA32
                    NIns* exit = asm_exit(ins); // does intersectRegisterState()
                    asm_switch(ins, exit);
#else
                    NanoAssertMsg(0, "Not supported for this architecture");
#endif
                    break;
                }

                case LIR_xt:
                case LIR_xf:
                    ins->oprnd1()->setResultLive();
                    asm_xcc(ins);
                    break;

                case LIR_x:
                    asm_x(ins);
                    break;

                case LIR_addxovi:
                case LIR_subxovi:
                case LIR_mulxovi:
                    verbose_only( _thisfrag->nStaticExits++; )
                    countlir_xcc();
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    if (ins->isExtant()) {
                        NIns* exit = asm_exit(ins); // does intersectRegisterState()
                        asm_branch_ov(op, exit);
                        asm_arith(ins);
                    }
                    break;

                case LIR_addjovi:
                case LIR_subjovi:
                case LIR_muljovi:
                    countlir_jcc();
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    if (ins->isExtant()) {
                        asm_jov(ins, pending_lives);
                        asm_arith(ins);
                    }
                    break;

#ifdef NANOJIT_64BIT
                case LIR_addjovq:
                case LIR_subjovq:
                    countlir_jcc();
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    if (ins->isExtant()) {
                        asm_jov(ins, pending_lives);
                        asm_qbinop(ins);
                    }
                    break;
#endif

                case LIR_eqd:
                case LIR_led:
                case LIR_ltd:
                case LIR_gtd:
                case LIR_ged:
                    countlir_fpu();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    if (ins->isExtant()) {
                        asm_condd(ins);
                    }
                    break;

                case LIR_eqi:
                case LIR_lei:
                case LIR_lti:
                case LIR_gti:
                case LIR_gei:
                case LIR_ltui:
                case LIR_leui:
                case LIR_gtui:
                case LIR_geui:
                CASE64(LIR_eqq:)
                CASE64(LIR_leq:)
                CASE64(LIR_ltq:)
                CASE64(LIR_gtq:)
                CASE64(LIR_geq:)
                CASE64(LIR_ltuq:)
                CASE64(LIR_leuq:)
                CASE64(LIR_gtuq:)
                CASE64(LIR_geuq:)
                    countlir_alu();
                    ins->oprnd1()->setResultLive();
                    ins->oprnd2()->setResultLive();
                    if (ins->isExtant()) {
                        asm_cond(ins);
                    }
                    break;

                case LIR_callv:
                case LIR_calli:
                CASE64(LIR_callq:)
                case LIR_calld:
                    countlir_call();
                    for (int i = 0, argc = ins->argc(); i < argc; i++)
                        ins->arg(i)->setResultLive();
                    // It must be impure or pure-and-extant -- it couldn't be
                    // pure-and-not-extant, because there's no way the codegen
                    // for a call can be folded into the codegen of another
                    // LIR instruction.
                    NanoAssert(!ins->callInfo()->_isPure || ins->isExtant());
                    asm_call(ins);
                    break;

                #ifdef VMCFG_VTUNE
                case LIR_file: {
                     // we traverse backwards so we are now hitting the file
                     // that is associated with a bunch of LIR_lines we already have seen
                    if (vtuneHandle) {
                        void * currentFile = (void *) ins->oprnd1()->immI();
                        vtuneFile(vtuneHandle, currentFile);
                    }
                    break;
                }
                case LIR_line: {
                     // add a new table entry, we don't yet knwo which file it belongs
                     // to so we need to add it to the update table too
                     // note the alloc, actual act is delayed; see above
                    if (vtuneHandle) {
                        uint32_t currentLine = (uint32_t) ins->oprnd1()->immI();
                        vtuneLine(vtuneHandle, currentLine, _nIns);
                    }
                    break;
                }
               #endif // VMCFG_VTUNE

            }

#ifdef NJ_VERBOSE
            // We do final LIR printing inside this loop to avoid printing
            // dead LIR instructions.  We print the LIns after generating the
            // code.  This ensures that the LIns will appear in debug output
            // *before* the native code, because Assembler::outputf()
            // prints everything in reverse.
            //
            if (_logc->lcbits & LC_AfterDCE) {
                InsBuf b;
                LInsPrinter* printer = _thisfrag->lirbuf->printer;
                outputf("    %s", printer->formatIns(&b, ins));
            }
#endif

            if (error())
                return;

            // check that all is well (don't check in exit paths since its more complicated)
            debug_only( pageValidate(); )
            debug_only( resourceConsistencyCheck();  )
        }
    }

    /*
     * Write a jump table for the given SwitchInfo and store the table
     * address in the SwitchInfo. Every entry will initially point to
     * target.
     */
    void Assembler::emitJumpTable(SwitchInfo* si, NIns* target)
    {
        si->table = (NIns **) alloc.alloc(si->count * sizeof(NIns*));
        for (uint32_t i = 0; i < si->count; ++i)
            si->table[i] = target;
    }

    void Assembler::assignSavedRegs()
    {
        // Restore saved regsters.
        LirBuffer *b = _thisfrag->lirbuf;
        for (int i=0, n = NumSavedRegs; i < n; i++) {
            LIns *p = b->savedRegs[i];
            if (p)
                findSpecificRegForUnallocated(p, savedRegs[p->paramArg()]);
        }
    }

    void Assembler::reserveSavedRegs()
    {
        LirBuffer *b = _thisfrag->lirbuf;
        for (int i = 0, n = NumSavedRegs; i < n; i++) {
            LIns *ins = b->savedRegs[i];
            if (ins)
                findMemFor(ins);
        }
    }

    void Assembler::assignParamRegs()
    {
        LIns* state = _thisfrag->lirbuf->state;
        if (state)
            findSpecificRegForUnallocated(state, argRegs[state->paramArg()]);
        LIns* param1 = _thisfrag->lirbuf->param1;
        if (param1)
            findSpecificRegForUnallocated(param1, argRegs[param1->paramArg()]);
    }

    void Assembler::handleLoopCarriedExprs(InsList& pending_lives)
    {
        // ensure that exprs spanning the loop are marked live at the end of the loop
        reserveSavedRegs();
        for (Seq<LIns*> *p = pending_lives.get(); p != NULL; p = p->tail) {
            LIns *ins = p->head;
            NanoAssert(isLiveOpcode(ins->opcode()));
            LIns *op1 = ins->oprnd1();
            // Must findMemFor even if we're going to findRegFor; loop-carried
            // operands may spill on another edge, and we need them to always
            // spill to the same place.
#if NJ_USES_IMMD_POOL
            // Exception: if float constants are true constants, we should
            // never call findMemFor on those ops.
            if (!op1->isImmD())
#endif
            {
                findMemFor(op1);
            }
            if (!op1->isImmAny())
                findRegFor(op1, ins->isop(LIR_lived) ? FpRegs : GpRegs);
        }

        // clear this list since we have now dealt with those lifetimes.  extending
        // their lifetimes again later (earlier in the code) serves no purpose.
        pending_lives.clear();
    }

    void AR::freeEntryAt(uint32_t idx)
    {
        NanoAssert(idx > 0 && idx <= _highWaterMark);

        // NB: this loop relies on using entry[0] being NULL,
        // so that we are guaranteed to terminate
        // without access negative entries.
        LIns* i = _entries[idx];
        NanoAssert(i != NULL);
        do {
            _entries[idx] = NULL;
            idx--;
        } while (_entries[idx] == i);
    }

#ifdef NJ_VERBOSE
    void Assembler::printRegState()
    {
        char* s = &outline[0];
        VMPI_memset(s, ' ', 26);  s[26] = '\0';
        s += VMPI_strlen(s);
        VMPI_sprintf(s, "RR");
        s += VMPI_strlen(s);

        RegisterMask active = _allocator.activeMask();
        for (Register r = lsReg(active); active != 0; r = nextLsReg(active, r)) {
            LIns *ins = _allocator.getActive(r);
            NanoAssertMsg(!_allocator.isFree(r),
                          "Coding error; register is both free and active! " );
            RefBuf b;
            const char* n = _thisfrag->lirbuf->printer->formatRef(&b, ins);

            if (ins->isop(LIR_paramp) && ins->paramKind()==1 &&
                r == Assembler::savedRegs[ins->paramArg()])
            {
                // dont print callee-saved regs that arent used
                continue;
            }

            VMPI_sprintf(s, " %s(%s)", gpn(r), n);
            s += VMPI_strlen(s);
        }
        output();
    }

    void Assembler::printActivationState()
    {
        char* s = &outline[0];
        VMPI_memset(s, ' ', 26);  s[26] = '\0';
        s += VMPI_strlen(s);
        VMPI_sprintf(s, "AR");
        s += VMPI_strlen(s);

        LIns* ins = 0;
        uint32_t nStackSlots = 0;
        int32_t arIndex = 0;
        for (AR::Iter iter(_activation); iter.next(ins, nStackSlots, arIndex); )
        {
            RefBuf b;
            const char* n = _thisfrag->lirbuf->printer->formatRef(&b, ins);
            if (nStackSlots > 1) {
                VMPI_sprintf(s," %d-%d(%s)", 4*arIndex, 4*(arIndex+nStackSlots-1), n);
            }
            else {
                VMPI_sprintf(s," %d(%s)", 4*arIndex, n);
            }
            s += VMPI_strlen(s);
        }
        output();
    }
#endif

    inline bool AR::isEmptyRange(uint32_t start, uint32_t nStackSlots) const
    {
        for (uint32_t i=0; i < nStackSlots; i++)
        {
            if (_entries[start-i] != NULL)
                return false;
        }
        return true;
    }

    uint32_t AR::reserveEntry(LIns* ins)
    {
        uint32_t const nStackSlots = nStackSlotsFor(ins);

        if (nStackSlots == 1)
        {
            for (uint32_t i = 1; i <= _highWaterMark; i++)
            {
                if (_entries[i] == NULL)
                {
                    _entries[i] = ins;
                    return i;
                }
            }
            if (_highWaterMark < NJ_MAX_STACK_ENTRY - 1)
            {
                NanoAssert(_entries[_highWaterMark+1] == BAD_ENTRY);
                _highWaterMark++;
                _entries[_highWaterMark] = ins;
                return _highWaterMark;
             }
        }
        else
        {
            // alloc larger block on 8byte boundary.
            uint32_t const start = nStackSlots + (nStackSlots & 1);
            for (uint32_t i = start; i <= _highWaterMark; i += 2)
            {
                if (isEmptyRange(i, nStackSlots))
                {
                    // place the entry in the table and mark the instruction with it
                    for (uint32_t j=0; j < nStackSlots; j++)
                    {
                        NanoAssert(i-j <= _highWaterMark);
                        NanoAssert(_entries[i-j] == NULL);
                        _entries[i-j] = ins;
                    }
                    return i;
                }
            }

            // Be sure to account for any 8-byte-round-up when calculating spaceNeeded.
            uint32_t const spaceLeft = NJ_MAX_STACK_ENTRY - _highWaterMark - 1;
            uint32_t const spaceNeeded = nStackSlots + (_highWaterMark & 1);
            if (spaceLeft >= spaceNeeded)
            {
                if (_highWaterMark & 1)
                {
                    NanoAssert(_entries[_highWaterMark+1] == BAD_ENTRY);
                    _entries[_highWaterMark+1] = NULL;
                }
                _highWaterMark += spaceNeeded;
                for (uint32_t j = 0; j < nStackSlots; j++)
                {
                    NanoAssert(_highWaterMark-j < NJ_MAX_STACK_ENTRY);
                    NanoAssert(_entries[_highWaterMark-j] == BAD_ENTRY);
                    _entries[_highWaterMark-j] = ins;
                }
                return _highWaterMark;
            }
        }
        // no space. oh well.
        return 0;
    }

    #ifdef _DEBUG
    void AR::checkForResourceLeaks() const
    {
        for (uint32_t i = 1; i <= _highWaterMark; i++) {
            NanoAssertMsgf(_entries[i] == NULL, "frame entry %d wasn't freed\n",4*i);
        }
    }
    #endif

    uint32_t Assembler::arReserve(LIns* ins)
    {
        uint32_t i = _activation.reserveEntry(ins);
        if (!i)
            setError(StackFull);
        return i;
    }

    void Assembler::arFree(LIns* ins)
    {
        NanoAssert(ins->isInAr());
        uint32_t arIndex = ins->getArIndex();
        NanoAssert(arIndex);
        NanoAssert(_activation.isValidEntry(arIndex, ins));
        _activation.freeEntryAt(arIndex);        // free any stack stack space associated with entry
    }

    /**
     * Move regs around so the SavedRegs contains the highest priority regs.
     */
    void Assembler::evictScratchRegsExcept(RegisterMask ignore)
    {
        // Find the top GpRegs that are candidates to put in SavedRegs.

        // 'tosave' is a binary heap stored in an array.  The root is tosave[0],
        // left child is at i+1, right child is at i+2.

        Register tosave[LastRegNum - FirstRegNum + 1];
        int len=0;
        RegAlloc *regs = &_allocator;
        RegisterMask evict_set = regs->activeMask() & GpRegs & ~ignore;
        for (Register r = lsReg(evict_set); evict_set; r = nextLsReg(evict_set, r)) {
            LIns *ins = regs->getActive(r);
            if (canRemat(ins)) {
                NanoAssert(ins->getReg() == r);
                evict(ins);
            }
            else {
                int32_t pri = regs->getPriority(r);
                // add to heap by adding to end and bubbling up
                int j = len++;
                while (j > 0 && pri > regs->getPriority(tosave[j/2])) {
                    tosave[j] = tosave[j/2];
                    j /= 2;
                }
                NanoAssert(size_t(j) < sizeof(tosave)/sizeof(tosave[0]));
                tosave[j] = r;
            }
        }

        // Now primap has the live exprs in priority order.
        // Allocate each of the top priority exprs to a SavedReg.

        RegisterMask allow = SavedRegs;
        while (allow && len > 0) {
            // get the highest priority var
            Register hi = tosave[0];
            if (!(rmask(hi) & SavedRegs)) {
                LIns *ins = regs->getActive(hi);
                Register r = findRegFor(ins, allow);
                allow &= ~rmask(r);
            }
            else {
                // hi is already in a saved reg, leave it alone.
                allow &= ~rmask(hi);
            }

            // remove from heap by replacing root with end element and bubbling down.
            if (allow && --len > 0) {
                Register last = tosave[len];
                int j = 0;
                while (j+1 < len) {
                    int child = j+1;
                    if (j+2 < len && regs->getPriority(tosave[j+2]) > regs->getPriority(tosave[j+1]))
                        child++;
                    if (regs->getPriority(last) > regs->getPriority(tosave[child]))
                        break;
                    tosave[j] = tosave[child];
                    j = child;
                }
                tosave[j] = last;
            }
        }

        // now evict everything else.
        evictSomeActiveRegs(~(SavedRegs | ignore));
    }

    // Generate code to restore any registers in 'regs' that are currently active,
    void Assembler::evictSomeActiveRegs(RegisterMask regs)
    {
        RegisterMask evict_set = regs & _allocator.activeMask();
        for (Register r = lsReg(evict_set); evict_set; r = nextLsReg(evict_set, r))
            evict(_allocator.getActive(r));
    }

    /**
     * Merge the current regstate with a previously stored version.
     *
     * Situation                            Change to _allocator
     * ---------                            --------------------
     * !current & !saved
     * !current &  saved                    add saved
     *  current & !saved                    evict current (unionRegisterState does nothing)
     *  current &  saved & current==saved
     *  current &  saved & current!=saved   evict current, add saved
     */
    void Assembler::intersectRegisterState(RegAlloc& saved)
    {
        Register regsTodo[LastRegNum + 1];
        LIns* insTodo[LastRegNum + 1];
        int nTodo = 0;

        // Do evictions and pops first.
        verbose_only(bool shouldMention=false; )
        // The obvious thing to do here is to iterate from FirstRegNum to
        // LastRegNum.  However, on ARM that causes lower-numbered integer
        // registers to be be saved at higher addresses, which inhibits the
        // formation of load/store multiple instructions.  Hence iterate the
        // loop the other way.
        RegisterMask reg_set = _allocator.activeMask() | saved.activeMask();
        for (Register r = msReg(reg_set); reg_set; r = nextMsReg(reg_set, r))
        {
            LIns* curins = _allocator.getActive(r);
            LIns* savedins = saved.getActive(r);
            if (curins != savedins)
            {
                if (savedins) {
                    regsTodo[nTodo] = r;
                    insTodo[nTodo] = savedins;
                    nTodo++;
                }
                if (curins) {
                    //_nvprof("intersect-evict",1);
                    verbose_only( shouldMention=true; )
                    NanoAssert(curins->getReg() == r);
                    evict(curins);
                }

                #ifdef NANOJIT_IA32
                if (savedins && r == FST0) {
                    verbose_only( shouldMention=true; )
                    FSTP(FST0);
                }
                #endif
            }
        }
        // Now reassign mainline registers.
        for (int i = 0; i < nTodo; i++) {
            findSpecificRegFor(insTodo[i], regsTodo[i]);
        }
        verbose_only(
            if (shouldMention)
                verbose_outputf("## merging registers (intersect) with existing edge");
        )
    }

    /**
     * Merge the current state of the registers with a previously stored version.
     *
     * Situation                            Change to _allocator
     * ---------                            --------------------
     * !current & !saved                    none
     * !current &  saved                    add saved
     *  current & !saved                    none (intersectRegisterState evicts current)
     *  current &  saved & current==saved   none
     *  current &  saved & current!=saved   evict current, add saved
     */
    void Assembler::unionRegisterState(RegAlloc& saved)
    {
        Register regsTodo[LastRegNum + 1];
        LIns* insTodo[LastRegNum + 1];
        int nTodo = 0;

        // Do evictions and pops first.
        verbose_only(bool shouldMention=false; )
        RegisterMask reg_set = _allocator.activeMask() | saved.activeMask();
        for (Register r = lsReg(reg_set); reg_set; r = nextLsReg(reg_set, r))
        {
            LIns* curins = _allocator.getActive(r);
            LIns* savedins = saved.getActive(r);
            if (curins != savedins)
            {
                if (savedins) {
                    regsTodo[nTodo] = r;
                    insTodo[nTodo] = savedins;
                    nTodo++;
                }
                if (curins && savedins) {
                    //_nvprof("union-evict",1);
                    verbose_only( shouldMention=true; )
                    NanoAssert(curins->getReg() == r);
                    evict(curins);
                }

                #ifdef NANOJIT_IA32
                if (r == FST0) {
                    if (savedins) {
                        // Discard top of x87 stack.
                        FSTP(FST0);
                    }
                    else if (curins) {
                        // Saved state did not have fpu reg allocated,
                        // so we must evict here to keep x87 stack balanced.
                        evict(curins);
                    }
                    verbose_only( shouldMention=true; )
                }
                #endif
            }
        }
        // Now reassign mainline registers.
        for (int i = 0; i < nTodo; i++) {
            findSpecificRegFor(insTodo[i], regsTodo[i]);
        }
        verbose_only(
            if (shouldMention)
                verbose_outputf("## merging registers (union) with existing edge");
        )
    }

    // Scan table for instruction with the lowest priority, meaning it is used
    // furthest in the future.
    LIns* Assembler::findVictim(RegisterMask allow)
    {
        NanoAssert(allow);
        LIns *ins, *vic = 0;
        int allow_pri = 0x7fffffff;
        RegisterMask vic_set = allow & _allocator.activeMask();
        for (Register r = lsReg(vic_set); vic_set; r = nextLsReg(vic_set, r))
        {
            ins = _allocator.getActive(r);
            int pri = canRemat(ins) ? 0 : _allocator.getPriority(r);
            if (!vic || pri < allow_pri) {
                vic = ins;
                allow_pri = pri;
            }
        }
        NanoAssert(vic != 0);
        return vic;
    }

#ifdef NJ_VERBOSE
    char Assembler::outline[8192];
    char Assembler::outlineEOL[512];

    void Assembler::output()
    {
        // The +1 is for the terminating NUL char.
        VMPI_strncat(outline, outlineEOL, sizeof(outline)-(strlen(outline)+1));

        if (_outputCache) {
            char* str = new (alloc) char[VMPI_strlen(outline)+1];
            VMPI_strcpy(str, outline);
            _outputCache->insert(str);
        } else {
            _logc->printf("%s\n", outline);
        }

        outline[0] = '\0';
        outlineEOL[0] = '\0';
    }

    void Assembler::outputf(const char* format, ...)
    {
        va_list args;
        va_start(args, format);

        outline[0] = '\0';
        vsprintf(outline, format, args);
        output();
    }

    void Assembler::setOutputForEOL(const char* format, ...)
    {
        va_list args;
        va_start(args, format);

        outlineEOL[0] = '\0';
        vsprintf(outlineEOL, format, args);
    }
#endif // NJ_VERBOSE

    void LabelStateMap::add(LIns *label, NIns *addr, RegAlloc &regs) {
        LabelState *st = new (alloc) LabelState(addr, regs);
        labels.put(label, st);
    }

    LabelState* LabelStateMap::get(LIns *label) {
        return labels.get(label);
    }
}
#endif /* FEATURE_NANOJIT */
