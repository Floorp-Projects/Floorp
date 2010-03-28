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

#ifdef VTUNE
#include "../core/CodegenLIR.h"
#endif

#ifdef _MSC_VER
    // disable some specific warnings which are normally useful, but pervasive in the code-gen macros
    #pragma warning(disable:4310) // cast truncates constant value
#endif

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
    #if NJ_USES_QUAD_CONSTANTS
        , _quadConstants(alloc)
    #endif
        , _epilogue(NULL)
        , _err(None)
    #if PEDANTIC
        , pedanticTop(NULL)
    #endif
    #ifdef VTUNE
        , cgen(NULL)
    #endif
        , _config(config)
    {
        VMPI_memset(&_stats, 0, sizeof(_stats));
        VMPI_memset(lookahead, 0, N_LOOKAHEAD * sizeof(LInsp));
        nInit(core);
        (void)logc;
        verbose_only( _logc = logc; )
        verbose_only( _outputCache = 0; )
        verbose_only( outline[0] = '\0'; )
        verbose_only( outlineEOL[0] = '\0'; )

        reset();
    }

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
    #if NJ_USES_QUAD_CONSTANTS
        _quadConstants.clear();
    #endif
    }

    void Assembler::registerResetAll()
    {
        nRegisterResetAll(_allocator);

        // At start, should have some registers free and none active.
        NanoAssert(0 != _allocator.free);
        NanoAssert(0 == _allocator.countActive());
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
            counter_increment(steals);

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

    /**
     * these instructions don't have to be saved & reloaded to spill,
     * they can just be recalculated w/out any inputs.
     */
    bool Assembler::canRemat(LIns *i) {
        return i->isImmAny() || i->isop(LIR_alloc);
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

        #ifdef VTUNE
        if (_nIns && _nExitIns) {
            //cgen->jitAddRecord((uintptr_t)list->code, 0, 0, true); // add placeholder record for top of page
            cgen->jitCodePosUpdate((uintptr_t)list->code);
            cgen->jitPushInfo(); // new page requires new entry
        }
        #endif
    }

    void Assembler::reset()
    {
        _nIns = 0;
        _nExitIns = 0;
        codeStart = codeEnd = 0;
        exitStart = exitEnd = 0;
        _stats.pages = 0;
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
        NanoAssertMsg(containsPtr(codeStart, codeEnd, _nIns),
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
            if (ins->isop(LIR_alloc)) {
                int const n = i + (ins->size()>>2);
                for (int j=i+1; j < n; j++) {
                    NanoAssert(_entries[j]==ins);
                }
                NanoAssert(arIndex == (uint32_t)n-1);
                i = n-1;
            }
            else if (ins->isN64()) {
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
        NanoAssert((_allocator.active[FST0] && _fpuStkDepth == -1) ||
            (!_allocator.active[FST0] && _fpuStkDepth == 0));
#endif

        _activation.checkForResourceConsistency(_allocator);

        registerConsistencyCheck();
    }

    void Assembler::registerConsistencyCheck()
    {
        RegisterMask managed = _allocator.managed;
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r)) {
            if (rmask(r) & managed) {
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
            } else {
                // A register not managed by register allocation must be
                // neither free nor active.
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
    // optimize the LIR_alloc case by indexing off FP, thus saving the use of
    // a GpReg.
    //
    Register Assembler::getBaseReg(LInsp base, int &d, RegisterMask allow)
    {
    #if !PEDANTIC
        if (base->isop(LIR_alloc)) {
            // The value of a LIR_alloc is a pointer to its stack memory,
            // which is always relative to FP.  So we can just return FP if we
            // also adjust 'd' (and can do so in a valid manner).  Or, in the
            // PEDANTIC case, we can just assign a register as normal;
            // findRegFor() will allocate the stack memory for LIR_alloc if
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
    // case where the base value is a LIR_alloc.
    void Assembler::getBaseReg2(RegisterMask allowValue, LIns* value, Register& rv,
                                RegisterMask allowBase, LIns* base, Register& rb, int &d)
    {
    #if !PEDANTIC
        if (base->isop(LIR_alloc)) {
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
        if (ins->isop(LIR_alloc)) {
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
        if (ins->isop(LIR_alloc)) {
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

#if NJ_USES_QUAD_CONSTANTS
    const uint64_t* Assembler::findQuadConstant(uint64_t q)
    {
        uint64_t* p = _quadConstants.get(q);
        if (!p)
        {
            p = new (_dataAlloc) uint64_t;
            *p = q;
            _quadConstants.put(q, p);
        }
        return p;
    }
#endif

    int Assembler::findMemFor(LIns *ins)
    {
#if NJ_USES_QUAD_CONSTANTS
        NanoAssert(!ins->isconstf());
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
        // At this point, we know the result of 'ins' result has a use later
        // in the code.  (Exception: if 'ins' is a call to an impure function
        // the return value may not be used, but 'ins' will still be present
        // because it has side-effects.)  It may have had to be evicted, in
        // which case the restore will have already been generated, so we now
        // generate the spill (unless the restore was actually a
        // rematerialize, in which case it's not necessary).
#ifdef NANOJIT_IA32
        // If 'allow' includes FST0 we have to pop if 'ins' isn't in FST0 in
        // the post-regstate.  This could be because 'ins' is unused, 'ins' is
        // in a spill slot, or 'ins' is in an XMM register.
        const bool pop = (allow & rmask(FST0)) &&
                         (!ins->isInReg() || ins->getReg() != FST0);
#else
        const bool pop = false;
#endif
        Register r = findRegFor(ins, allow);
        asm_maybe_spill(ins, pop);
#ifdef NANOJIT_IA32
        if (!ins->isInAr() && pop && r == FST0) {
            // This can only happen with a LIR_fcall to an impure function
            // whose return value was ignored (ie. if ins->isInReg() was false
            // prior to the findRegFor() call).
            FSTP(FST0);     // pop the fpu result since it isn't used
        }
#endif
        return r;
    }

    void Assembler::asm_maybe_spill(LInsp ins, bool pop)
    {
        int d = ins->isInAr() ? arDisp(ins) : 0;
        Register r = ins->getReg();
        if (ins->isInAr()) {
            verbose_only( RefBuf b;
                          if (_logc->lcbits & LC_Assembly) {
                             setOutputForEOL("  <= spill %s",
                             _thisfrag->lirbuf->printer->formatRef(&b, ins)); } )
            asm_spill(r, d, pop, ins->isN64());
        }
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
        counter_increment(steals);

        Register r = vic->getReg();

        NanoAssert(!_allocator.isFree(r));
        NanoAssert(vic == _allocator.getActive(r));

        verbose_only( RefBuf b;
                      if (_logc->lcbits & LC_Assembly) {
                        setOutputForEOL("  <= restore %s",
                        _thisfrag->lirbuf->printer->formatRef(&b, vic)); } )
        asm_restore(vic, r);

        _allocator.retire(r);
        vic->clearReg();

        // At this point 'vic' is unused (if rematerializable), or in a spill
        // slot (if not).
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

    NIns* Assembler::asm_exit(LInsp guard)
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

    NIns* Assembler::asm_leave_trace(LInsp guard)
    {
        verbose_only( int32_t nativeSave = _stats.native );
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
        verbose_only( verbose_outputf("%010lx:", (unsigned long)jmpTarget);)
        verbose_only( verbose_outputf("----------------------------------- ## BEGIN exit block (LIR_xt|LIR_xf)") );

#ifdef NANOJIT_IA32
        NanoAssertMsgf(_fpuStkDepth == _sv_fpuStkDepth, "LIR_xtf, _fpuStkDepth=%d, expect %d",_fpuStkDepth, _sv_fpuStkDepth);
        debug_only( _fpuStkDepth = _sv_fpuStkDepth; _sv_fpuStkDepth = 9999; )
#endif

        verbose_only(_stats.exitnative += (_stats.native-nativeSave));

        return jmpTarget;
    }

    void Assembler::compile(Fragment* frag, Allocator& alloc, bool optimize verbose_only(, LInsPrinter* printer))
    {
        verbose_only(
        bool anyVerb = (_logc->lcbits & 0xFFFF & ~LC_FragProfile) > 0;
        bool asmVerb = (_logc->lcbits & 0xFFFF & LC_Assembly) > 0;
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

        verbose_only(
            if (asmVerb)
                outputf("## compiling trunk %s", printer->formatAddr(&b, frag));
        )
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

        counter_reset(native);
        counter_reset(exitnative);
        counter_reset(steals);
        counter_reset(spills);
        counter_reset(remats);

        setError(None);

        // native code gen buffer setup
        nativePageSetup();

        // make sure we got memory at least one page
        if (error()) return;

#ifdef PERFM
        _stats.pages = 0;
        _stats.codeStart = _nIns-1;
        _stats.codeExitStart = _nExitIns-1;
#endif /* PERFM */

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
                    NIns** native_table = (NIns**) where;
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
        PERFM_NVPROF("code", CodeAlloc::size(codeList));

#ifdef NANOJIT_IA32
        NanoAssertMsgf(_fpuStkDepth == 0,"_fpuStkDepth %d\n",_fpuStkDepth);
#endif

        debug_only( pageValidate(); )
        NanoAssert(_branchStateMap.isEmpty());
    }

    void Assembler::releaseRegisters()
    {
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r))
        {
            LIns *ins = _allocator.getActive(r);
            if (ins) {
                // Clear reg allocation, preserve stack allocation.
                _allocator.retire(r);
                NanoAssert(r == ins->getReg());
                ins->clearReg();
            }
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

    void Assembler::gen(LirFilter* reader)
    {
        NanoAssert(_thisfrag->nStaticExits == 0);

        // The trace must end with one of these opcodes.
        NanoAssert(reader->finalIns()->isop(LIR_x)    ||
                   reader->finalIns()->isop(LIR_xtbl) ||
                   reader->finalIns()->isRet()        ||
                   reader->finalIns()->isLive());

        InsList pending_lives(alloc);

        NanoAssert(!error());

        // What's going on here: we're visiting all the LIR instructions in
        // the buffer, working strictly backwards in buffer-order, and
        // generating machine instructions for them as we go.
        //
        // For each LIns, we first determine whether it's actually necessary,
        // and if not skip it.  Otherwise we generate code for it.  There are
        // two kinds of "necessary" instructions:
        //
        // - "Statement" instructions, which have side effects.  Anything that
        //   could change control flow or the state of memory.
        //
        // - "Value" or "expression" instructions, which compute a value based
        //   only on the operands to the instruction (and, in the case of
        //   loads, the state of memory).  Because we visit instructions in
        //   reverse order, if some previously visited instruction uses the
        //   value computed by this instruction, then this instruction will
        //   already have a register assigned to hold that value.  Hence we
        //   can consult the instruction to detect whether its value is in
        //   fact used (i.e. not dead).
        //
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
        //
        // Another thing to note: we provide N_LOOKAHEAD instruction's worth
        // of lookahead because it's useful for backends.  This is nice and
        // easy because once read() gets to the LIR_start at the beginning of
        // the buffer it'll just keep regetting it.

        for (int32_t i = 0; i < N_LOOKAHEAD; i++)
            lookahead[i] = reader->read();

        while (!lookahead[0]->isop(LIR_start))
        {
            LInsp ins = lookahead[0];   // give it a shorter name for local use
            LOpcode op = ins->opcode();

            bool required = ins->isStmt() || ins->isUsed();
            if (!required)
                goto end_of_loop;

#ifdef NJ_VERBOSE
            // Output the post-regstate (registers and/or activation).
            // Because asm output comes in reverse order, doing it now means
            // it is printed after the LIR and asm, exactly when the
            // post-regstate should be shown.
            if ((_logc->lcbits & LC_Assembly) && (_logc->lcbits & LC_Activation))
                printActivationState();
            if ((_logc->lcbits & LC_Assembly) && (_logc->lcbits & LC_RegAlloc))
                printRegState();
#endif

            switch (op)
            {
                default:
                    NanoAssertMsgf(false, "unsupported LIR instruction: %d\n", op);
                    break;

                case LIR_regfence:
                    evictAllActiveRegs();
                    break;

                case LIR_live:
                case LIR_flive:
                CASE64(LIR_qlive:) {
                    countlir_live();
                    LInsp op1 = ins->oprnd1();
                    // alloca's are meant to live until the point of the LIR_live instruction, marking
                    // other expressions as live ensures that they remain so at loop bottoms.
                    // alloca areas require special treatment because they are accessed indirectly and
                    // the indirect accesses are invisible to the assembler, other than via LIR_live.
                    // other expression results are only accessed directly in ways that are visible to
                    // the assembler, so extending those expression's lifetimes past the last loop edge
                    // isn't necessary.
                    if (op1->isop(LIR_alloc)) {
                        findMemFor(op1);
                    } else {
                        pending_lives.add(ins);
                    }
                    break;
                }

                case LIR_ret:
                case LIR_fret:
                CASE64(LIR_qret:) {
                    countlir_ret();
                    asm_ret(ins);
                    break;
                }

                // Allocate some stack space.  The value of this instruction
                // is the address of the stack space.
                case LIR_alloc: {
                    countlir_alloc();
                    NanoAssert(ins->isInAr());
                    if (ins->isInReg()) {
                        Register r = ins->getReg();
                        asm_restore(ins, r);
                        _allocator.retire(r);
                        ins->clearReg();
                    }
                    freeResourcesOf(ins);
                    break;
                }
                case LIR_int:
                {
                    countlir_imm();
                    asm_immi(ins);
                    break;
                }
#ifdef NANOJIT_64BIT
                case LIR_quad:
                {
                    countlir_imm();
                    asm_immq(ins);
                    break;
                }
#endif
                case LIR_float:
                {
                    countlir_imm();
                    asm_immf(ins);
                    break;
                }
                case LIR_param:
                {
                    countlir_param();
                    asm_param(ins);
                    break;
                }
#if NJ_SOFTFLOAT_SUPPORTED
                case LIR_callh:
                {
                    // return result of quad-call in register
                    deprecated_prepResultReg(ins, rmask(retRegs[1]));
                    // if hi half was used, we must use the call to ensure it happens
                    findSpecificRegFor(ins->oprnd1(), retRegs[0]);
                    break;
                }
                case LIR_qlo:
                {
                    countlir_qlo();
                    asm_qlo(ins);
                    break;
                }
                case LIR_qhi:
                {
                    countlir_qhi();
                    asm_qhi(ins);
                    break;
                }
                case LIR_qjoin:
                {
                    countlir_qjoin();
                    asm_qjoin(ins);
                    break;
                }
#endif
                CASE64(LIR_qcmov:)
                case LIR_cmov:
                {
                    countlir_cmov();
                    asm_cmov(ins);
                    break;
                }
                case LIR_ldzb:
                case LIR_ldzs:
                case LIR_ldsb:
                case LIR_ldss:
                case LIR_ld:
                {
                    countlir_ld();
                    asm_load32(ins);
                    break;
                }

                case LIR_ld32f:
                case LIR_ldf:
                CASE64(LIR_ldq:)
                {
                    countlir_ldq();
                    asm_load64(ins);
                    break;
                }
                case LIR_neg:
                case LIR_not:
                {
                    countlir_alu();
                    asm_neg_not(ins);
                    break;
                }

#if defined NANOJIT_64BIT
                case LIR_qiadd:
                case LIR_qiand:
                case LIR_qilsh:
                case LIR_qursh:
                case LIR_qirsh:
                case LIR_qior:
                case LIR_qxor:
                {
                    asm_qbinop(ins);
                    break;
                }
#endif

                case LIR_add:
                case LIR_sub:
                case LIR_mul:
                case LIR_and:
                case LIR_or:
                case LIR_xor:
                case LIR_lsh:
                case LIR_rsh:
                case LIR_ush:
                CASE86(LIR_div:)
                CASE86(LIR_mod:)
                {
                    countlir_alu();
                    asm_arith(ins);
                    break;
                }
                case LIR_fneg:
                {
                    countlir_fpu();
                    asm_fneg(ins);
                    break;
                }
                case LIR_fadd:
                case LIR_fsub:
                case LIR_fmul:
                case LIR_fdiv:
                {
                    countlir_fpu();
                    asm_fop(ins);
                    break;
                }
                case LIR_i2f:
                {
                    countlir_fpu();
                    asm_i2f(ins);
                    break;
                }
                case LIR_u2f:
                {
                    countlir_fpu();
                    asm_u2f(ins);
                    break;
                }
                case LIR_f2i:
                {
                    countlir_fpu();
                    asm_f2i(ins);
                    break;
                }
#ifdef NANOJIT_64BIT
                case LIR_i2q:
                case LIR_u2q:
                {
                    countlir_alu();
                    asm_promote(ins);
                    break;
                }
                case LIR_q2i:
                {
                    countlir_alu();
                    asm_q2i(ins);
                    break;
                }
#endif
                case LIR_stb:
                case LIR_sts:
                case LIR_sti:
                {
                    countlir_st();
                    asm_store32(op, ins->oprnd1(), ins->disp(), ins->oprnd2());
                    break;
                }
                case LIR_st32f:
                case LIR_stfi:
                CASE64(LIR_stqi:)
                {
                    countlir_stq();
                    LIns* value = ins->oprnd1();
                    LIns* base = ins->oprnd2();
                    int dr = ins->disp();
#if NJ_SOFTFLOAT_SUPPORTED
                    if (value->isop(LIR_qjoin) && op == LIR_stfi)
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
                {
                    countlir_jmp();
                    LInsp to = ins->getTarget();
                    LabelState *label = _labels.get(to);
                    // the jump is always taken so whatever register state we
                    // have from downstream code, is irrelevant to code before
                    // this jump.  so clear it out.  we will pick up register
                    // state from the jump target, if we have seen that label.
                    releaseRegisters();
                    if (label && label->addr) {
                        // forward jump - pick up register state from target.
                        unionRegisterState(label->regs);
                        JMP(label->addr);
                    }
                    else {
                        // backwards jump
                        handleLoopCarriedExprs(pending_lives);
                        if (!label) {
                            // save empty register state at loop header
                            _labels.add(to, 0, _allocator);
                        }
                        else {
                            intersectRegisterState(label->regs);
                        }
                        JMP(0);
                        _patches.put(_nIns, to);
                    }
                    break;
                }

                case LIR_jt:
                case LIR_jf:
                {
                    countlir_jcc();
                    LInsp to = ins->getTarget();
                    LIns* cond = ins->oprnd1();
                    LabelState *label = _labels.get(to);
                    if (label && label->addr) {
                        // forward jump to known label.  need to merge with label's register state.
                        unionRegisterState(label->regs);
                        asm_branch(op == LIR_jf, cond, label->addr);
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
                        NIns *branch = asm_branch(op == LIR_jf, cond, 0);
                        _patches.put(branch,to);
                    }
                    break;
                }

                #if NJ_JTBL_SUPPORTED
                case LIR_jtbl:
                {
                    countlir_jtbl();
                    // Multiway jump can contain both forward and backward jumps.
                    // Out of range indices aren't allowed or checked.
                    // Code after this jtbl instruction is unreachable.
                    releaseRegisters();
                    NanoAssert(_allocator.countActive() == 0);

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
                    NanoAssert(_allocator.countActive() == 0);

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

                case LIR_label:
                {
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
                        if (_logc->lcbits & LC_Assembly) {
                            asm_output("[%s]", _thisfrag->lirbuf->printer->formatRef(&b, ins));
                    })
                    break;
                }
                case LIR_xbarrier: {
                    break;
                }
#ifdef NANOJIT_IA32
                case LIR_xtbl: {
                    NIns* exit = asm_exit(ins); // does intersectRegisterState()
                    asm_switch(ins, exit);
                    break;
                }
#else
                 case LIR_xtbl:
                    NanoAssertMsg(0, "Not supported for this architecture");
                    break;
#endif
                case LIR_xt:
                case LIR_xf:
                {
                    verbose_only( _thisfrag->nStaticExits++; )
                    countlir_xcc();
                    // we only support cmp with guard right now, also assume it is 'close' and only emit the branch
                    NIns* exit = asm_exit(ins); // does intersectRegisterState()
                    LIns* cond = ins->oprnd1();
                    asm_branch(op == LIR_xf, cond, exit);
                    break;
                }
                case LIR_x:
                {
                    verbose_only( _thisfrag->nStaticExits++; )
                    countlir_x();
                    // generate the side exit branch on the main trace.
                    NIns *exit = asm_exit(ins);
                    JMP( exit );
                    break;
                }
                case LIR_addxov:
                case LIR_subxov:
                case LIR_mulxov:
                {
                    verbose_only( _thisfrag->nStaticExits++; )
                    countlir_xcc();
                    countlir_alu();
                    NIns* exit = asm_exit(ins); // does intersectRegisterState()
                    asm_branch_xov(op, exit);
                    asm_arith(ins);
                    break;
                }

                case LIR_feq:
                case LIR_fle:
                case LIR_flt:
                case LIR_fgt:
                case LIR_fge:
                {
                    countlir_fpu();
                    asm_fcond(ins);
                    break;
                }
                case LIR_eq:
                case LIR_le:
                case LIR_lt:
                case LIR_gt:
                case LIR_ge:
                case LIR_ult:
                case LIR_ule:
                case LIR_ugt:
                case LIR_uge:
#ifdef NANOJIT_64BIT
                case LIR_qeq:
                case LIR_qle:
                case LIR_qlt:
                case LIR_qgt:
                case LIR_qge:
                case LIR_qult:
                case LIR_qule:
                case LIR_qugt:
                case LIR_quge:
#endif
                {
                    countlir_alu();
                    asm_cond(ins);
                    break;
                }

                case LIR_fcall:
            #ifdef NANOJIT_64BIT
                case LIR_qcall:
            #endif
                case LIR_icall:
                {
                    countlir_call();
                    asm_call(ins);
                    break;
                }

                #ifdef VTUNE
                case LIR_file:
                {
                    // we traverse backwards so we are now hitting the file
                    // that is associated with a bunch of LIR_lines we already have seen
                    uintptr_t currentFile = ins->oprnd1()->imm32();
                    cgen->jitFilenameUpdate(currentFile);
                    break;
                }
                case LIR_line:
                {
                    // add a new table entry, we don't yet knwo which file it belongs
                    // to so we need to add it to the update table too
                    // note the alloc, actual act is delayed; see above
                    uint32_t currentLine = (uint32_t) ins->oprnd1()->imm32();
                    cgen->jitLineNumUpdate(currentLine);
                    cgen->jitAddRecord((uintptr_t)_nIns, 0, currentLine, true);
                    break;
                }
                #endif // VTUNE
            }

#ifdef NJ_VERBOSE
            // We have to do final LIR printing inside this loop.  If we do it
            // before this loop, we we end up printing a lot of dead LIR
            // instructions.
            //
            // We print the LIns after generating the code.  This ensures that
            // the LIns will appear in debug output *before* the generated
            // code, because Assembler::outputf() prints everything in reverse.
            //
            // Note that some live LIR instructions won't be printed.  Eg. an
            // immediate won't be printed unless it is explicitly loaded into
            // a register (as opposed to being incorporated into an immediate
            // field in another machine instruction).
            //
            if (_logc->lcbits & LC_Assembly) {
                InsBuf b;
                LInsPrinter* printer = _thisfrag->lirbuf->printer;
                outputf("    %s", printer->formatIns(&b, ins));
                if (ins->isGuard() && ins->oprnd1() && ins->oprnd1()->isCmp()) {
                    // Special case: code is generated for guard conditions at
                    // the same time that code is generated for the guard
                    // itself.  If the condition is only used by the guard, we
                    // must print it now otherwise it won't get printed.  So
                    // we do print it now, with an explanatory comment.  If
                    // the condition *is* used again we'll end up printing it
                    // twice, but that's ok.
                    outputf("    %s       # codegen'd with the %s",
                            printer->formatIns(&b, ins->oprnd1()), lirNames[op]);

                } else if (ins->isCmov()) {
                    // Likewise for cmov conditions.
                    outputf("    %s       # codegen'd with the %s",
                            printer->formatIns(&b, ins->oprnd1()), lirNames[op]);

                }
#if defined NANOJIT_IA32 || defined NANOJIT_X64
                else if (ins->isop(LIR_mod)) {
                    // There's a similar case when a div feeds into a mod.
                    outputf("    %s       # codegen'd with the mod",
                            printer->formatIns(&b, ins->oprnd1()));
                }
#endif
            }
#endif

            if (error())
                return;

        #ifdef VTUNE
            cgen->jitCodePosUpdate((uintptr_t)_nIns);
        #endif

            // check that all is well (don't check in exit paths since its more complicated)
            debug_only( pageValidate(); )
            debug_only( resourceConsistencyCheck();  )

          end_of_loop:
            for (int32_t i = 1; i < N_LOOKAHEAD; i++)
                lookahead[i-1] = lookahead[i];
            lookahead[N_LOOKAHEAD-1] = reader->read();
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
        LInsp state = _thisfrag->lirbuf->state;
        if (state)
            findSpecificRegForUnallocated(state, argRegs[state->paramArg()]);
        LInsp param1 = _thisfrag->lirbuf->param1;
        if (param1)
            findSpecificRegForUnallocated(param1, argRegs[param1->paramArg()]);
    }

    void Assembler::handleLoopCarriedExprs(InsList& pending_lives)
    {
        // ensure that exprs spanning the loop are marked live at the end of the loop
        reserveSavedRegs();
        for (Seq<LIns*> *p = pending_lives.get(); p != NULL; p = p->tail) {
            LIns *ins = p->head;
            NanoAssert(ins->isLive());
            LIns *op1 = ins->oprnd1();
            // Must findMemFor even if we're going to findRegFor; loop-carried
            // operands may spill on another edge, and we need them to always
            // spill to the same place.
#if NJ_USES_QUAD_CONSTANTS
            // Exception: if float constants are true constants, we should
            // never call findMemFor on those ops.
            if (!op1->isconstf())
#endif
            {
                findMemFor(op1);
            }
            if (!op1->isImmAny())
                findRegFor(op1, ins->isop(LIR_flive) ? FpRegs : GpRegs);
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

        for (Register r = FirstReg; r <= LastReg; r = nextreg(r)) {
            LIns *ins = _allocator.getActive(r);
            if (ins) {
                NanoAssertMsg(!_allocator.isFree(r),
                              "Coding error; register is both free and active! " );
                RefBuf b;
                const char* n = _thisfrag->lirbuf->printer->formatRef(&b, ins);

                if (ins->isop(LIR_param) && ins->paramKind()==1 &&
                    r == Assembler::savedRegs[ins->paramArg()])
                {
                    // dont print callee-saved regs that arent used
                    continue;
                }

                VMPI_sprintf(s, " %s(%s)", gpn(r), n);
                s += VMPI_strlen(s);
            }
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
            uint32_t const spaceLeft = NJ_MAX_STACK_ENTRY - _highWaterMark - 1;
            if (spaceLeft >= 1)
            {
                 NanoAssert(_entries[_highWaterMark+1] == BAD_ENTRY);
                _entries[++_highWaterMark] = ins;
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

        Register tosave[LastReg-FirstReg+1];
        int len=0;
        RegAlloc *regs = &_allocator;
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r)) {
            if (rmask(r) & GpRegs & ~ignore) {
                LIns *ins = regs->getActive(r);
                if (ins) {
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

    void Assembler::evictAllActiveRegs()
    {
        // generate code to restore callee saved registers
        // @todo speed this up
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r)) {
            evictIfActive(r);
        }
    }

    void Assembler::evictSomeActiveRegs(RegisterMask regs)
    {
        // generate code to restore callee saved registers
        // @todo speed this up
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r)) {
            if ((rmask(r) & regs)) {
                evictIfActive(r);
            }
        }
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
        Register regsTodo[LastReg + 1];
        LIns* insTodo[LastReg + 1];
        int nTodo = 0;

        // Do evictions and pops first.
        verbose_only(bool shouldMention=false; )
        // The obvious thing to do here is to iterate from FirstReg to LastReg.
        // viz: for (Register r = FirstReg; r <= LastReg; r = nextreg(r)) ...
        // However, on ARM that causes lower-numbered integer registers
        // to be be saved at higher addresses, which inhibits the formation
        // of load/store multiple instructions.  Hence iterate the loop the
        // other way.  The "r <= LastReg" guards against wraparound in
        // the case where Register is treated as unsigned and FirstReg is zero.
        //
        // Note, the loop var is deliberately typed as int (*not* Register)
        // to outsmart compilers that will otherwise report
        // "error: comparison is always true due to limited range of data type".
        for (int ri = LastReg; ri >= FirstReg && ri <= LastReg; ri = int(prevreg(Register(ri))))
        {
            Register const r = Register(ri);
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
                if (savedins && (rmask(r) & x87Regs)) {
                    verbose_only( shouldMention=true; )
                    FSTP(r);
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
        Register regsTodo[LastReg + 1];
        LIns* insTodo[LastReg + 1];
        int nTodo = 0;

        // Do evictions and pops first.
        verbose_only(bool shouldMention=false; )
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r))
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
                if (rmask(r) & x87Regs) {
                    if (savedins) {
                        FSTP(r);
                    }
                    else if (curins) {
                        // saved state did not have fpu reg allocated,
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
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r))
        {
            if ((allow & rmask(r)) && (ins = _allocator.getActive(r)) != 0)
            {
                int pri = canRemat(ins) ? 0 : _allocator.getPriority(r);
                if (!vic || pri < allow_pri) {
                    vic = ins;
                    allow_pri = pri;
                }
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
