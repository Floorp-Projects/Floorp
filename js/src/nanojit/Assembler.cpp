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

namespace nanojit
{
#ifdef NJ_VERBOSE
    /* A listing filter for LIR, going through backwards.  It merely
       passes its input to its output, but notes it down too.  When
       destructed, prints out what went through.  Is intended to be
       used to print arbitrary intermediate transformation stages of
       LIR. */
    class ReverseLister : public LirFilter
    {
        Allocator&   _alloc;
        LirNameMap*  _names;
        const char*  _title;
        StringList   _strs;
        LogControl*  _logc;
    public:
        ReverseLister(LirFilter* in, Allocator& alloc,
                      LirNameMap* names, LogControl* logc, const char* title)
            : LirFilter(in)
            , _alloc(alloc)
            , _names(names)
            , _title(title)
            , _strs(alloc)
            , _logc(logc)
        { }

        void finish()
        {
            _logc->printf("\n");
            _logc->printf("=== BEGIN %s ===\n", _title);
            int j = 0;
            for (Seq<char*>* p = _strs.get(); p != NULL; p = p->tail)
                _logc->printf("  %02d: %s\n", j++, p->head);
            _logc->printf("=== END %s ===\n", _title);
            _logc->printf("\n");
        }

        LInsp read()
        {
            LInsp i = in->read();
            const char* str = _names->formatIns(i);
            char* cpy = new (_alloc) char[strlen(str)+1];
            VMPI_strcpy(cpy, str);
            _strs.insert(cpy);
            return i;
        }
    };
#endif

    /**
     * Need the following:
     *
     *    - merging paths ( build a graph? ), possibly use external rep to drive codegen
     */
    Assembler::Assembler(CodeAlloc& codeAlloc, Allocator& alloc, AvmCore* core, LogControl* logc)
        : codeList(NULL)
        , alloc(alloc)
        , _codeAlloc(codeAlloc)
        , _thisfrag(NULL)
        , _branchStateMap(alloc)
        , _patches(alloc)
        , _labels(alloc)
        , _epilogue(NULL)
        , _err(None)
        , config(core->config)
    {
        VMPI_memset(&_stats, 0, sizeof(_stats));
        nInit(core);
        (void)logc;
        verbose_only( _logc = logc; )
        verbose_only( _outputCache = 0; )
        verbose_only( outlineEOL[0] = '\0'; )
        verbose_only( outputAddr = false; )

        reset();
    }

    void Assembler::arReset()
    {
        _activation.lowwatermark = 0;
        _activation.tos = 0;

        for(uint32_t i=0; i<NJ_MAX_STACK_ENTRY; i++)
            _activation.entry[i] = 0;

        _branchStateMap.clear();
        _patches.clear();
        _labels.clear();
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

    Register Assembler::registerAlloc(RegisterMask allow)
    {
        RegAlloc &regs = _allocator;
        RegisterMask allowedAndFree = allow & regs.free;

        if (allowedAndFree)
        {
            // At least one usable register is free -- no need to steal.  
            // Pick a preferred one if possible.
            RegisterMask preferredAndFree = allowedAndFree & SavedRegs;
            RegisterMask set = ( preferredAndFree ? preferredAndFree : allowedAndFree );
            Register r = nRegisterAllocFromSet(set);
            return r;
        }
        counter_increment(steals);

        // nothing free, steal one
        // LSRA says pick the one with the furthest use
        LIns* vic = findVictim(allow);
        NanoAssert(vic);

        // restore vic
        Register r = vic->getReg();
        regs.removeActive(r);
        vic->setReg(UnknownReg);

        asm_restore(vic, vic->resv(), r);
        return r;
    }

    /**
     * these instructions don't have to be saved & reloaded to spill,
     * they can just be recalculated w/out any inputs.
     */
    bool Assembler::canRemat(LIns *i) {
        return i->isconst() || i->isconstq() || i->isop(LIR_alloc);
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
        // _nIns needs to be at least on one of these pages
        NanoAssertMsg(_inExit ? containsPtr(exitStart, exitEnd, _nIns) : containsPtr(codeStart, codeEnd, _nIns),
                     "Native instruction pointer overstep paging bounds; check overrideProtect for last instruction");
    }
    #endif

    #ifdef _DEBUG

    void Assembler::resourceConsistencyCheck()
    {
        NanoAssert(!error());

#ifdef NANOJIT_IA32
        NanoAssert((_allocator.active[FST0] && _fpuStkDepth == -1) ||
            (!_allocator.active[FST0] && _fpuStkDepth == 0));
#endif

        AR &ar = _activation;
        // check AR entries
        NanoAssert(ar.tos < NJ_MAX_STACK_ENTRY);
        LIns* ins = 0;
        RegAlloc* regs = &_allocator;
        for(uint32_t i = ar.lowwatermark; i < ar.tos; i++)
        {
            ins = ar.entry[i];
            if ( !ins )
                continue;
            Register r = ins->getReg();
            uint32_t arIndex = ins->getArIndex();
            if (arIndex != 0) {
                if (ins->isop(LIR_alloc)) {
                    int j=i+1;
                    for (int n = i + (ins->size()>>2); j < n; j++) {
                        NanoAssert(ar.entry[j]==ins);
                    }
                    NanoAssert(arIndex == (uint32_t)j-1);
                    i = j-1;
                }
                else if (ins->isQuad()) {
                    NanoAssert(ar.entry[i - stack_direction(1)]==ins);
                    i += 1; // skip high word
                }
                else {
                    NanoAssertMsg(arIndex == i, "Stack record index mismatch");
                }
            }
            NanoAssertMsg( !isKnownReg(r) || regs->isConsistent(r,ins), "Register record mismatch");
        }

        registerConsistencyCheck();
    }

    void Assembler::registerConsistencyCheck()
    {
        // check registers
        RegAlloc *regs = &_allocator;
        RegisterMask managed = regs->managed;
        Register r = FirstReg;
        while(managed)
        {
            if (managed&1)
            {
                if (regs->isFree(r))
                {
                    NanoAssertMsgf(regs->getActive(r)==0, "register %s is free but assigned to ins", gpn(r));
                }
                else
                {
                    LIns* ins = regs->getActive(r);
                    // @todo we should be able to check across RegAlloc's somehow (to include savedGP...)
                    Register r2 = ins->getReg();
                    NanoAssertMsg(regs->getActive(r2)==ins, "Register record mismatch");
                }
            }

            // next register in bitfield
            r = nextreg(r);
            managed >>= 1;
        }
    }
    #endif /* _DEBUG */

    void Assembler::findRegFor2(RegisterMask allow, LIns* ia, Reservation* &resva, LIns* ib, Reservation* &resvb)
    {
        if (ia == ib)
        {
            findRegFor(ia, allow);
            resva = resvb = ia->resvUsed();
        }
        else
        {
            resvb = ib->resv();
            bool rbDone = (resvb->used && isKnownReg(resvb->reg) && (allow & rmask(resvb->reg)));
            if (rbDone) {
                // ib already assigned to an allowable reg, keep that one
                allow &= ~rmask(resvb->reg);
            }
            Register ra = findRegFor(ia, allow);
            resva = ia->resv();
            NanoAssert(error() || (resva->used && isKnownReg(ra)));
            if (!rbDone) {
                allow &= ~rmask(ra);
                findRegFor(ib, allow);
                resvb = ib->resvUsed();
            }
        }
    }

    // XXX: this will replace findRegFor2() once Reservations are fully removed
    void Assembler::findRegFor2b(RegisterMask allow, LIns* ia, Register &ra, LIns* ib, Register &rb)
    {
        Reservation *resva, *resvb;
        findRegFor2(allow, ia, resva, ib, resvb);
        ra = resva->reg;
        rb = resvb->reg;
    }

    Register Assembler::findSpecificRegFor(LIns* i, Register w)
    {
        return findRegFor(i, rmask(w));
    }

    Register Assembler::getBaseReg(LIns *i, int &d, RegisterMask allow)
    {
        if (i->isop(LIR_alloc)) {
            d += findMemFor(i);
            return FP;
        }
        return findRegFor(i, allow);
    }

    // Finds a register in 'allow' to hold the result of 'ins'.  Used when we
    // encounter a use of 'ins'.  The actions depend on the prior state of
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
            // never allocate a reg for this w/out stack space too
            findMemFor(ins);
        }

        Register r;

        if (!ins->isUsed()) {
            // No reservation.  Create one, and do a fresh allocation.
            ins->markAsUsed();
            RegisterMask prefer = hint(ins, allow);
            r = registerAlloc(prefer);
            ins->setReg(r);
            _allocator.addActive(r, ins);

        } else if (!ins->hasKnownReg()) {
            // Existing reservation with an unknown register.  Do a fresh
            // allocation.
            RegisterMask prefer = hint(ins, allow);
            r = registerAlloc(prefer);
            ins->setReg(r);
            _allocator.addActive(r, ins);

        } else if (rmask(r = ins->getReg()) & allow) {
            // Existing reservation with a known register allocated, and
            // that register is allowed.  Use it.
            _allocator.useActive(r);

        } else {
            // Existing reservation with a known register allocated, but
            // the register is not allowed.
            RegisterMask prefer = hint(ins, allow);
#ifdef NANOJIT_IA32
            if (((rmask(r)&XmmRegs) && !(allow&XmmRegs)) ||
                ((rmask(r)&x87Regs) && !(allow&x87Regs)))
            {
                // x87 <-> xmm copy required
                //_nvprof("fpu-evict",1);
                evict(r, ins);
                r = registerAlloc(prefer);
                ins->setReg(r);
                _allocator.addActive(r, ins);
            } else
#elif defined(NANOJIT_PPC)
            if (((rmask(r)&GpRegs) && !(allow&GpRegs)) ||
                ((rmask(r)&FpRegs) && !(allow&FpRegs)))
            {
                evict(r, ins);
                r = registerAlloc(prefer);
                ins->setReg(r);
                _allocator.addActive(r, ins);
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
                _allocator.retire(r);
                Register s = r;
                r = registerAlloc(prefer);
                ins->setReg(r);
                _allocator.addActive(r, ins);
                if ((rmask(s) & GpRegs) && (rmask(r) & GpRegs)) {
#ifdef NANOJIT_ARM
                    MOV(s, r);  // ie. move 'ins' from its pre-state reg to its post-state reg
#else
                    MR(s, r);
#endif
                }
                else {
                    asm_nongp_copy(s, r);
                }
            }
        }
        return r;
    }

    int Assembler::findMemFor(LIns *ins)
    {
        if (!ins->isUsed())
            ins->markAsUsed();
        if (!ins->getArIndex()) {
            ins->setArIndex(arReserve(ins));
            NanoAssert(ins->getArIndex() <= _activation.tos);
        }
        return disp(ins);
    }

    Register Assembler::prepResultReg(LIns *ins, RegisterMask allow)
    {
        const bool pop = ins->isUnusedOrHasUnknownReg();
        Register r = findRegFor(ins, allow);
        freeRsrcOf(ins, pop);
        return r;
    }

    void Assembler::asm_spilli(LInsp ins, bool pop)
    {
        int d = disp(ins);
        Register r = ins->getReg();
        bool quad = ins->opcode() == LIR_iparam || ins->isQuad();
        verbose_only( if (d && (_logc->lcbits & LC_RegAlloc)) {
                         outputForEOL("  <= spill %s",
                                      _thisfrag->lirbuf->names->formatRef(ins)); } )
        asm_spill(r, d, pop, quad);
    }

    // NOTE: Because this function frees slots on the stack, it is not safe to
    // follow a call to this with a call to anything which might spill a
    // register, as the stack can be corrupted. Refer to bug 495239 for a more
    // detailed description.
    void Assembler::freeRsrcOf(LIns *ins, bool pop)
    {
        Register r = ins->getReg();
        if (isKnownReg(r)) {
            asm_spilli(ins, pop);
            _allocator.retire(r);   // free any register associated with entry
        }
        int arIndex = ins->getArIndex();
        if (arIndex) {
            NanoAssert(_activation.entry[arIndex] == ins);
            arFree(arIndex);        // free any stack stack space associated with entry
        }
        ins->markAsClear();
    }

    // Frees 'r' in the RegAlloc state, if it's not already free.
    void Assembler::evictIfActive(Register r)
    {
        if (LIns* vic = _allocator.getActive(r)) {
            evict(r, vic);
        }
    }

    // Frees 'r' (which currently holds the result of 'vic') in the RegAlloc
    // state.  An example:
    //
    //   pre-state:     eax(ld1)
    //   instruction:   mov ebx,-4(ebp) <= restore add1   # %ebx is dest
    //   post-state:    eax(ld1) ebx(add1)
    //
    // At run-time we are *restoring* 'add1' into %ebx, hence the call to
    // asm_restore().  But at regalloc-time we are moving backwards through
    // the code, so in that sense we are *evicting* 'add1' from %ebx.
    //
    void Assembler::evict(Register r, LIns* vic)
    {
        // Not free, need to steal.
        counter_increment(steals);

        // Get vic's resv, check r matches.
        NanoAssert(!_allocator.isFree(r));
        NanoAssert(vic == _allocator.getActive(r));
        NanoAssert(r == vic->getReg());

        // Free r.
        _allocator.retire(r);
        vic->setReg(UnknownReg);

        // Restore vic.
        asm_restore(vic, vic->resv(), r);
    }

    void Assembler::patch(GuardRecord *lr)
    {
        if (!lr->jmp) // the guard might have been eliminated as redundant
            return;
        Fragment *frag = lr->exit->target;
        NanoAssert(frag->fragEntry != 0);
        nPatchBranch((NIns*)lr->jmp, frag->fragEntry);
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

        RegAlloc capture = _allocator;

        // this point is unreachable.  so free all the registers.
        // if an instruction has a stack entry we will leave it alone,
        // otherwise we free it entirely.  intersectRegisterState will restore.
        releaseRegisters();

        swapptrs();
        _inExit = true;

#ifdef NANOJIT_IA32
        debug_only( _sv_fpuStkDepth = _fpuStkDepth; _fpuStkDepth = 0; )
#endif

        nFragExit(guard);

        // restore the callee-saved register and parameters
        assignSavedRegs();
        assignParamRegs();

        intersectRegisterState(capture);

        // this can be useful for breaking whenever an exit is taken
        //INT3();
        //NOP();

        // we are done producing the exit logic for the guard so demark where our exit block code begins
        NIns* jmpTarget = _nIns;     // target in exit path for our mainline conditional jump

        // swap back pointers, effectively storing the last location used in the exit path
        swapptrs();
        _inExit = false;

        //verbose_only( verbose_outputf("         LIR_xt/xf swapptrs, _nIns is now %08X(%08X), _nExitIns is now %08X(%08X)",_nIns, *_nIns,_nExitIns,*_nExitIns) );
        verbose_only( verbose_outputf("%010lx:", (unsigned long)jmpTarget);)
        verbose_only( verbose_outputf("----------------------------------- ## BEGIN exit block (LIR_xt|LIR_xf)") );

#ifdef NANOJIT_IA32
        NanoAssertMsgf(_fpuStkDepth == _sv_fpuStkDepth, "LIR_xtf, _fpuStkDepth=%d, expect %d",_fpuStkDepth, _sv_fpuStkDepth);
        debug_only( _fpuStkDepth = _sv_fpuStkDepth; _sv_fpuStkDepth = 9999; )
#endif

        verbose_only(_stats.exitnative += (_stats.native-nativeSave));

        return jmpTarget;
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
        _activation.lowwatermark = 1;
        _activation.tos = _activation.lowwatermark;
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

    void Assembler::assemble(Fragment* frag)
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

        // Used for debug printing, if needed
        verbose_only(
        ReverseLister *pp_init = NULL;
        ReverseLister *pp_after_sf = NULL;
        )

        // set up backwards pipeline: assembler -> StackFilter -> LirReader
        LirReader bufreader(frag->lastIns);

        // Used to construct the pipeline
        LirFilter* prev = &bufreader;

        // The LIR passes through these filters as listed in this
        // function, viz, top to bottom.

        // INITIAL PRINTING
        verbose_only( if (_logc->lcbits & LC_ReadLIR) {
        pp_init = new (alloc) ReverseLister(prev, alloc, frag->lirbuf->names, _logc,
                                    "Initial LIR");
        prev = pp_init;
        })

        // STACKFILTER
        StackFilter stackfilter(prev, alloc, frag->lirbuf, frag->lirbuf->sp, frag->lirbuf->rp);
        prev = &stackfilter;

        verbose_only( if (_logc->lcbits & LC_AfterSF) {
        pp_after_sf = new (alloc) ReverseLister(prev, alloc, frag->lirbuf->names, _logc,
                                                "After StackFilter");
        prev = pp_after_sf;
        })

        _inExit = false;

        gen(prev);

        if (!error()) {
            // patch all branches
            NInsMap::Iter iter(_patches);
            while (iter.next()) {
                NIns* where = iter.key();
                LIns* targ = iter.value();
                LabelState *label = _labels.get(targ);
                NIns* ntarg = label->addr;
                if (ntarg) {
                    nPatchBranch(where,ntarg);
                }
                else {
                    setError(UnknownBranch);
                    break;
                }
            }
        }

        // If we were accumulating debug info in the various ReverseListers,
        // call finish() to emit whatever contents they have accumulated.
        verbose_only(
        if (pp_init)        pp_init->finish();
        if (pp_after_sf)    pp_after_sf->finish();
        )
    }

    void Assembler::endAssembly(Fragment* frag)
    {
        // don't try to patch code if we are in an error state since we might have partially
        // overwritten the code cache already
        if (error()) {
            // something went wrong, release all allocated code memory
            _codeAlloc.freeAll(codeList);
            _codeAlloc.free(exitStart, exitEnd);
            _codeAlloc.free(codeStart, codeEnd);
            return;
        }

        NIns* fragEntry = genPrologue();
        verbose_only( outputAddr=true; )
        verbose_only( asm_output("[prologue]"); )

        // check for resource leaks
        debug_only(
            for (uint32_t i = _activation.lowwatermark; i < _activation.tos; i++) {
                NanoAssertMsgf(_activation.entry[i] == 0, "frame entry %d wasn't freed\n",-4*i);
            }
        )

        // save used parts of current block on fragment's code list, free the rest
#ifdef NANOJIT_ARM
        // [codeStart, _nSlot) ... gap ... [_nIns, codeEnd)
        _codeAlloc.addRemainder(codeList, exitStart, exitEnd, _nExitSlot, _nExitIns);
        _codeAlloc.addRemainder(codeList, codeStart, codeEnd, _nSlot, _nIns);
        verbose_only( exitBytes -= (_nExitIns - _nExitSlot) * sizeof(NIns); )
        verbose_only( codeBytes -= (_nIns - _nSlot) * sizeof(NIns); )
#else
        // [codeStart ... gap ... [_nIns, codeEnd))
        _codeAlloc.addRemainder(codeList, exitStart, exitEnd, exitStart, _nExitIns);
        _codeAlloc.addRemainder(codeList, codeStart, codeEnd, codeStart, _nIns);
        verbose_only( exitBytes -= (_nExitIns - exitStart) * sizeof(NIns); )
        verbose_only( codeBytes -= (_nIns - codeStart) * sizeof(NIns); )
#endif

        // at this point all our new code is in the d-cache and not the i-cache,
        // so flush the i-cache on cpu's that need it.
        _codeAlloc.flushICache(codeList);

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
            if (ins)
            {
                // clear reg allocation, preserve stack allocation.
                _allocator.retire(r);
                NanoAssert(r == ins->getReg());
                ins->setReg(UnknownReg);

                if (!ins->getArIndex())
                {
                    ins->markAsClear();
                }
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
#endif

    void Assembler::gen(LirFilter* reader)
    {
        NanoAssert(_thisfrag->nStaticExits == 0);

        // trace must end with LIR_x, LIR_[f]ret, LIR_xtbl, or LIR_[f]live
        NanoAssert(reader->pos()->isop(LIR_x) ||
                   reader->pos()->isop(LIR_ret) ||
                   reader->pos()->isop(LIR_fret) ||
                   reader->pos()->isop(LIR_xtbl) ||
                   reader->pos()->isop(LIR_flive) ||
                   reader->pos()->isop(LIR_live));

        InsList pending_lives(alloc);

        for (LInsp ins = reader->read(); !ins->isop(LIR_start) && !error();
                                         ins = reader->read())
        {
            /* What's going on here: we're visiting all the LIR instructions
               in the buffer, working strictly backwards in buffer-order, and
               generating machine instructions for them as we go.

               For each LIns, we first determine whether it's actually
               necessary, and if not skip it.  Otherwise we generate code for
               it.  There are two kinds of "necessary" instructions:

               - "Statement" instructions, which have side effects.  Anything
                 that could change control flow or the state of memory.

               - "Value" or "expression" instructions, which compute a value
                 based only on the operands to the instruction (and, in the
                 case of loads, the state of memory).  Because we visit
                 instructions in reverse order, if some previously visited
                 instruction uses the value computed by this instruction, then
                 this instruction will already have a register assigned to
                 hold that value.  Hence we can consult the Reservation to
                 detect whether the value is in fact used (i.e. not dead).

              Note that the backwards code traversal can make register
              allocation confusing.  (For example, we restore a value before
              we spill it!)  In particular, words like "before" and "after"
              must be used very carefully -- their meaning at regalloc-time is
              opposite to their meaning at run-time.  We use the term
              "pre-state" to refer to the register allocation state that
              occurs prior to an instruction's execution, and "post-state" to
              refer to the state that occurs after an instruction's execution,
              e.g.:

                pre-state:     ebx(ins)
                instruction:   mov eax, ebx     // mov dst, src
                post-state:    eax(ins)

              At run-time, the instruction updates the pre-state into the
              post-state (and these states are the real machine's states).
              But when allocating registers, because we go backwards, the
              pre-state is constructed from the post-state (and these states
              are those stored in RegAlloc).
            */
            bool required = ins->isStmt() || ins->isUsed();
            if (!required)
                continue;

            LOpcode op = ins->opcode();
            switch(op)
            {
                default:
                    NanoAssertMsgf(false, "unsupported LIR instruction: %d (~0x40: %d)\n", op, op&~LIR64);
                    break;

                case LIR_regfence:
                    evictAllActiveRegs();
                    break;

                case LIR_flive:
                case LIR_live: {
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

                case LIR_fret:
                case LIR_ret:  {
                    countlir_ret();
                    asm_ret(ins);
                    break;
                }

                // allocate some stack space.  the value of this instruction
                // is the address of the stack space.
                case LIR_alloc: {
                    countlir_alloc();
                    NanoAssert(ins->getArIndex() != 0);
                    Register r = ins->getReg();
                    if (isKnownReg(r)) {
                        _allocator.retire(r);
                        ins->setReg(UnknownReg);
                        asm_restore(ins, ins->resv(), r);
                    }
                    freeRsrcOf(ins, 0);
                    break;
                }
                case LIR_int:
                {
                    countlir_imm();
                    asm_int(ins);
                    break;
                }
                case LIR_float:
                case LIR_quad:
                {
                    countlir_imm();
                    asm_quad(ins);
                    break;
                }
#if !defined NANOJIT_64BIT
                case LIR_callh:
                {
                    // return result of quad-call in register
                    prepResultReg(ins, rmask(retRegs[1]));
                    // if hi half was used, we must use the call to ensure it happens
                    findSpecificRegFor(ins->oprnd1(), retRegs[0]);
                    break;
                }
#endif
                case LIR_param:
                {
                    countlir_param();
                    asm_param(ins);
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
                case LIR_qcmov:
                case LIR_cmov:
                {
                    countlir_cmov();
                    asm_cmov(ins);
                    break;
                }
                case LIR_ld:
                case LIR_ldc:
                case LIR_ldcb:
                case LIR_ldcs:
                {
                    countlir_ld();
                    asm_ld(ins);
                    break;
                }
                case LIR_ldq:
                case LIR_ldqc:
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
                case LIR_qjoin:
                {
                    countlir_qjoin();
                    asm_qjoin(ins);
                    break;
                }

#if defined NANOJIT_64BIT
                case LIR_qiadd:
                case LIR_qiand:
                case LIR_qilsh:
                case LIR_qursh:
                case LIR_qirsh:
                case LIR_qior:
                case LIR_qaddp:
                case LIR_qxor:
                {
                    asm_qbinop(ins);
                    break;
                }
#endif

                case LIR_add:
                case LIR_iaddp:
                case LIR_sub:
                case LIR_mul:
                case LIR_and:
                case LIR_or:
                case LIR_xor:
                case LIR_lsh:
                case LIR_rsh:
                case LIR_ush:
                case LIR_div:
                case LIR_mod:
                {
                    countlir_alu();
                    asm_arith(ins);
                    break;
                }
#ifndef NJ_SOFTFLOAT
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
                case LIR_i2q:
                case LIR_u2q:
                {
                    countlir_alu();
                    asm_promote(ins);
                    break;
                }
#endif // NJ_SOFTFLOAT
                case LIR_sti:
                {
                    countlir_st();
                    asm_store32(ins->oprnd1(), ins->disp(), ins->oprnd2());
                    break;
                }
                case LIR_stqi:
                {
                    countlir_stq();
                    LIns* value = ins->oprnd1();
                    LIns* base = ins->oprnd2();
                    int dr = ins->disp();
                    if (value->isop(LIR_qjoin))
                    {
                        // this is correct for little-endian only
                        asm_store32(value->oprnd1(), dr, base);
                        asm_store32(value->oprnd2(), dr+4, base);
                    }
                    else
                    {
                        asm_store64(value, dr, base);
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
                    verbose_only( if (_logc->lcbits & LC_Assembly) { 
                        outputAddr=true; asm_output("[%s]", 
                        _thisfrag->lirbuf->names->formatRef(ins)); 
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

#ifndef NJ_SOFTFLOAT
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
#endif
                case LIR_eq:
                case LIR_ov:
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

            #ifndef NJ_SOFTFLOAT
                case LIR_fcall:
            #endif
            #ifdef NANOJIT_64BIT
                case LIR_qcall:
            #endif
                case LIR_icall:
                {
                    countlir_call();
                    Register rr = UnknownReg;
#ifndef NJ_SOFTFLOAT
                    if (op == LIR_fcall)
                    {
                        // fcall
                        rr = asm_prep_fcall(getresv(ins), ins);
                    }
                    else
#endif
                    {
                        rr = retRegs[0];
                        prepResultReg(ins, rmask(rr));
                    }

                    // do this after we've handled the call result, so we dont
                    // force the call result to be spilled unnecessarily.

                    evictScratchRegs();

                    asm_call(ins);
                }
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
                LirNameMap* names = _thisfrag->lirbuf->names;
                outputf("    %s", names->formatIns(ins));
                if (ins->isGuard() && ins->oprnd1()) {
                    // Special case: code is generated for guard conditions at
                    // the same time that code is generated for the guard
                    // itself.  If the condition is only used by the guard, we
                    // must print it now otherwise it won't get printed.  So
                    // we do print it now, with an explanatory comment.  If
                    // the condition *is* used again we'll end up printing it
                    // twice, but that's ok.
                    outputf("    %s       # codegen'd with the %s",
                            names->formatIns(ins->oprnd1()), lirNames[op]);

                } else if (ins->isop(LIR_cmov) || ins->isop(LIR_qcmov)) {
                    // Likewise for cmov conditions.
                    outputf("    %s       # codegen'd with the %s",
                            names->formatIns(ins->oprnd1()), lirNames[op]);

                } else if (ins->isop(LIR_mod)) {
                    // There's a similar case when a div feeds into a mod.
                    outputf("    %s       # codegen'd with the mod",
                            names->formatIns(ins->oprnd1()));
                }
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
        underrunProtect(si->count * sizeof(NIns*) + 20);
        _nIns = reinterpret_cast<NIns*>(uintptr_t(_nIns) & ~(sizeof(NIns*) - 1));
        for (uint32_t i = 0; i < si->count; ++i) {
            _nIns = (NIns*) (((intptr_t) _nIns) - sizeof(NIns*));
            *(NIns**) _nIns = target;
        }
        si->table = (NIns**) _nIns;
    }

    void Assembler::assignSavedRegs()
    {
        // restore saved regs
        releaseRegisters();
        LirBuffer *b = _thisfrag->lirbuf;
        for (int i=0, n = NumSavedRegs; i < n; i++) {
            LIns *p = b->savedRegs[i];
            if (p)
                findSpecificRegFor(p, savedRegs[p->paramArg()]);
        }
    }

    void Assembler::reserveSavedRegs()
    {
        LirBuffer *b = _thisfrag->lirbuf;
        for (int i=0, n = NumSavedRegs; i < n; i++) {
            LIns *p = b->savedRegs[i];
            if (p)
                findMemFor(p);
        }
    }

    // restore parameter registers
    void Assembler::assignParamRegs()
    {
        LInsp state = _thisfrag->lirbuf->state;
        if (state)
            findSpecificRegFor(state, argRegs[state->paramArg()]);
        LInsp param1 = _thisfrag->lirbuf->param1;
        if (param1)
            findSpecificRegFor(param1, argRegs[param1->paramArg()]);
    }

    void Assembler::handleLoopCarriedExprs(InsList& pending_lives)
    {
        // ensure that exprs spanning the loop are marked live at the end of the loop
        reserveSavedRegs();
        for (Seq<LIns*> *p = pending_lives.get(); p != NULL; p = p->tail) {
            LIns *i = p->head;
            NanoAssert(i->isop(LIR_live) || i->isop(LIR_flive));
            LIns *op1 = i->oprnd1();
            if (op1->isconst() || op1->isconstf() || op1->isconstq())
                findMemFor(op1);
            else
                findRegFor(op1, i->isop(LIR_flive) ? FpRegs : GpRegs);
        }

        // clear this list since we have now dealt with those lifetimes.  extending
        // their lifetimes again later (earlier in the code) serves no purpose.
        pending_lives.clear();
    }

    void Assembler::arFree(uint32_t idx)
    {
        verbose_only( printActivationState(" >FP"); )

        AR &ar = _activation;
        LIns *i = ar.entry[idx];
        NanoAssert(i != 0);
        do {
            ar.entry[idx] = 0;
            idx--;
        } while (ar.entry[idx] == i);
    }

#ifdef NJ_VERBOSE
    void Assembler::printActivationState(const char* what)
    {
        if (!(_logc->lcbits & LC_Activation))
            return;

        char* s = &outline[0];
        VMPI_memset(s, ' ', 45);  s[45] = '\0';
        s += VMPI_strlen(s);
        VMPI_sprintf(s, "%s", what);
        s += VMPI_strlen(s);

        int32_t max = _activation.tos < NJ_MAX_STACK_ENTRY ? _activation.tos : NJ_MAX_STACK_ENTRY;
        for(int32_t i = _activation.lowwatermark; i < max; i++) {
            LIns *ins = _activation.entry[i];
            if (ins) {
                const char* n = _thisfrag->lirbuf->names->formatRef(ins);
                if (ins->isop(LIR_alloc)) {
                    int32_t count = ins->size()>>2;
                    VMPI_sprintf(s," %d-%d(%s)", 4*i, 4*(i+count-1), n);
                    count += i-1;
                    while (i < count) {
                        NanoAssert(_activation.entry[i] == ins);
                        i++;
                    }
                }
                else if (ins->isQuad()) {
                    VMPI_sprintf(s," %d+(%s)", 4*i, n);
                    NanoAssert(_activation.entry[i+1] == ins);
                    i++;
                }
                else {
                    VMPI_sprintf(s," %d(%s)", 4*i, n);
                }
            }
            s += VMPI_strlen(s);
        }
        output(&outline[0]);
    }
#endif

    bool canfit(int32_t size, int32_t loc, AR &ar) {
        for (int i=0; i < size; i++) {
            if (ar.entry[loc+stack_direction(i)])
                return false;
        }
        return true;
    }

    uint32_t Assembler::arReserve(LIns* l)
    {
        int32_t size = l->isop(LIR_alloc) ? (l->size()>>2) : l->isQuad() ? 2 : 1;
        AR &ar = _activation;
        const int32_t tos = ar.tos;
        int32_t start = ar.lowwatermark;
        int32_t i = 0;
        NanoAssert(start>0);
        verbose_only( printActivationState(" <FP"); )

        if (size == 1) {
            // easy most common case -- find a hole, or make the frame bigger
            for (i=start; i < NJ_MAX_STACK_ENTRY; i++) {
                if (ar.entry[i] == 0) {
                    // found a hole
                    ar.entry[i] = l;
                    break;
                }
            }
        }
        else if (size == 2) {
            if ( (start&1)==1 ) start++;  // even 8 boundary
            for (i=start; i < NJ_MAX_STACK_ENTRY; i+=2) {
                if ( (ar.entry[i+stack_direction(1)] == 0) && (i==tos || (ar.entry[i] == 0)) ) {
                    // found 2 adjacent aligned slots
                    NanoAssert(ar.entry[i] == 0);
                    NanoAssert(ar.entry[i+stack_direction(1)] == 0);
                    ar.entry[i] = l;
                    ar.entry[i+stack_direction(1)] = l;
                    break;
                }
            }
        }
        else {
            // alloc larger block on 8byte boundary.
            if (start < size) start = size;
            if ((start&1)==1) start++;
            for (i=start; i < NJ_MAX_STACK_ENTRY; i+=2) {
                if (canfit(size, i, ar)) {
                    // place the entry in the table and mark the instruction with it
                    for (int32_t j=0; j < size; j++) {
                        NanoAssert(ar.entry[i+stack_direction(j)] == 0);
                        ar.entry[i+stack_direction(j)] = l;
                    }
                    break;
                }
            }
        }
        if (i >= (int32_t)ar.tos) {
            ar.tos = i+1;
        }
        if (tos+size >= NJ_MAX_STACK_ENTRY) {
            setError(StackFull);
        }
        return i;
    }

    /**
     * move regs around so the SavedRegs contains the highest priority regs.
     */
    void Assembler::evictScratchRegs()
    {
        // find the top GpRegs that are candidates to put in SavedRegs

        // tosave is a binary heap stored in an array.  the root is tosave[0],
        // left child is at i+1, right child is at i+2.

        Register tosave[LastReg-FirstReg+1];
        int len=0;
        RegAlloc *regs = &_allocator;
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r)) {
            if (rmask(r) & GpRegs) {
                LIns *i = regs->getActive(r);
                if (i) {
                    if (canRemat(i)) {
                        evict(r, i);
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

        // now primap has the live exprs in priority order.
        // allocate each of the top priority exprs to a SavedReg

        RegisterMask allow = SavedRegs;
        while (allow && len > 0) {
            // get the highest priority var
            Register hi = tosave[0];
            if (!(rmask(hi) & SavedRegs)) {
                LIns *i = regs->getActive(hi);
                Register r = findRegFor(i, allow);
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
        evictSomeActiveRegs(~SavedRegs);
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
     * Merge the current state of the registers with a previously stored version
     * current == saved    skip
     * current & saved     evict current, keep saved
     * current & !saved    evict current  (unionRegisterState would keep)
     * !current & saved    keep saved
     */
    void Assembler::intersectRegisterState(RegAlloc& saved)
    {
        // evictions and pops first
        RegisterMask skip = 0;
        verbose_only(bool shouldMention=false; )
        for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
        {
            LIns * curins = _allocator.getActive(r);
            LIns * savedins = saved.getActive(r);
            if (curins == savedins)
            {
                //verbose_only( if (curins) verbose_outputf("                                              skip %s", regNames[r]); )
                skip |= rmask(r);
            }
            else
            {
                if (curins) {
                    //_nvprof("intersect-evict",1);
                    verbose_only( shouldMention=true; )
                    evict(r, curins);
                }

                #ifdef NANOJIT_IA32
                if (savedins && (rmask(r) & x87Regs)) {
                    verbose_only( shouldMention=true; )
                    FSTP(r);
                }
                #endif
            }
        }
        assignSaved(saved, skip);
        verbose_only(
            if (shouldMention)
                verbose_outputf("## merging registers (intersect) "
                                "with existing edge");
        )
    }

    /**
     * Merge the current state of the registers with a previously stored version.
     *
     * current == saved    skip
     * current & saved     evict current, keep saved
     * current & !saved    keep current (intersectRegisterState would evict)
     * !current & saved    keep saved
     */
    void Assembler::unionRegisterState(RegAlloc& saved)
    {
        // evictions and pops first
        verbose_only(bool shouldMention=false; )
        RegisterMask skip = 0;
        for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
        {
            LIns * curins = _allocator.getActive(r);
            LIns * savedins = saved.getActive(r);
            if (curins == savedins)
            {
                //verbose_only( if (curins) verbose_outputf("                                              skip %s", regNames[r]); )
                skip |= rmask(r);
            }
            else
            {
                if (curins && savedins) {
                    //_nvprof("union-evict",1);
                    verbose_only( shouldMention=true; )
                    evict(r, curins);
                }

                #ifdef NANOJIT_IA32
                if (rmask(r) & x87Regs) {
                    if (savedins) {
                        FSTP(r);
                    }
                    else {
                        // saved state did not have fpu reg allocated,
                        // so we must evict here to keep x87 stack balanced.
                        evictIfActive(r);
                    }
                    verbose_only( shouldMention=true; )
                }
                #endif
            }
        }
        assignSaved(saved, skip);
        verbose_only( if (shouldMention) verbose_outputf("                                              merging registers (union) with existing edge");  )
    }

    void Assembler::assignSaved(RegAlloc &saved, RegisterMask skip)
    {
        // now reassign mainline registers
        for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
        {
            LIns *i = saved.getActive(r);
            if (i && !(skip&rmask(r)))
                findSpecificRegFor(i, r);
        }
    }

    // Scan table for instruction with the lowest priority, meaning it is used
    // furthest in the future.
    LIns* Assembler::findVictim(RegisterMask allow)
    {
        NanoAssert(allow != 0);
        LIns *i, *a=0;
        int allow_pri = 0x7fffffff;
        for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
        {
            if ((allow & rmask(r)) && (i = _allocator.getActive(r)) != 0)
            {
                int pri = canRemat(i) ? 0 : _allocator.getPriority(r);
                if (!a || pri < allow_pri) {
                    a = i;
                    allow_pri = pri;
                }
            }
        }
        NanoAssert(a != 0);
        return a;
    }

#ifdef NJ_VERBOSE
    // "outline" must be able to hold the output line in addition to the
    // outlineEOL buffer, which is concatenated onto outline just before it
    // is printed.
    char Assembler::outline[8192];
    char Assembler::outlineEOL[512];

    void Assembler::outputForEOL(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        outlineEOL[0] = '\0';
        vsprintf(outlineEOL, format, args);
    }

    void Assembler::outputf(const char* format, ...)
    {
        va_list     args;
        va_start(args, format);
        outline[0] = '\0';

        // Format the output string and remember the number of characters
        // that were written.
        uint32_t outline_len = vsprintf(outline, format, args);

        // Add the EOL string to the output, ensuring that we leave enough
        // space for the terminating NULL character, then reset it so it
        // doesn't repeat on the next outputf.
        VMPI_strncat(outline, outlineEOL, sizeof(outline)-(outline_len+1));
        outlineEOL[0] = '\0';

        output(outline);
    }

    void Assembler::output(const char* s)
    {
        if (_outputCache)
        {
            char* str = new (alloc) char[VMPI_strlen(s)+1];
            VMPI_strcpy(str, s);
            _outputCache->insert(str);
        }
        else
        {
            _logc->printf("%s\n", s);
        }
    }

    void Assembler::output_asm(const char* s)
    {
        if (!(_logc->lcbits & LC_Assembly))
            return;

        // Add the EOL string to the output, ensuring that we leave enough
        // space for the terminating NULL character, then reset it so it
        // doesn't repeat on the next outputf.
        VMPI_strncat(outline, outlineEOL, sizeof(outline)-(strlen(outline)+1));
        outlineEOL[0] = '\0';

        output(s);
    }

    char* Assembler::outputAlign(char *s, int col)
    {
        int len = (int)VMPI_strlen(s);
        int add = ((col-len)>0) ? col-len : 1;
        VMPI_memset(&s[len], ' ', add);
        s[col] = '\0';
        return &s[col];
    }
#endif // NJ_VERBOSE

    uint32_t CallInfo::_count_args(uint32_t mask) const
    {
        uint32_t argc = 0;
        uint32_t argt = _argtypes;
        for (uint32_t i = 0; i < MAXARGS; ++i) {
            argt >>= ARGSIZE_SHIFT;
            if (!argt)
                break;
            argc += (argt & mask) != 0;
        }
        return argc;
    }

    uint32_t CallInfo::get_sizes(ArgSize* sizes) const
    {
        uint32_t argt = _argtypes;
        uint32_t argc = 0;
        for (uint32_t i = 0; i < MAXARGS; i++) {
            argt >>= ARGSIZE_SHIFT;
            ArgSize a = ArgSize(argt & ARGSIZE_MASK_ANY);
            if (a != ARGSIZE_NONE) {
                sizes[argc++] = a;
            } else {
                break;
            }
        }
        return argc;
    }

    void LabelStateMap::add(LIns *label, NIns *addr, RegAlloc &regs) {
        LabelState *st = new (alloc) LabelState(addr, regs);
        labels.put(label, st);
    }

    LabelState* LabelStateMap::get(LIns *label) {
        return labels.get(label);
    }
}
#endif /* FEATURE_NANOJIT */
