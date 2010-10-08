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


#ifndef __nanojit_Assembler__
#define __nanojit_Assembler__


namespace nanojit
{
    /**
     * Some notes on this Assembler (Emitter).
     *
     *   The class RegAlloc is essentially the register allocator from MIR
     *
     *   The Assembler class parses the LIR instructions starting at any point and converts
     *   them to machine code.  It does the translation using expression trees which are simply
     *   LIR instructions in the stream that have side-effects.  Any other instruction in the
     *   stream is simply ignored.
     *   This approach is interesting in that dead code elimination occurs for 'free', strength
     *   reduction occurs fairly naturally, along with some other optimizations.
     *
     *   A negative is that we require state as we 'push' and 'pop' nodes along the tree.
     *   Also, this is most easily performed using recursion which may not be desirable in
     *   the mobile environment.
     *
     */

    #define STACK_GRANULARITY        sizeof(void *)

    // Basics:
    // - 'entry' records the state of the native machine stack at particular
    //   points during assembly.  Each entry represents four bytes.
    //
    // - Parts of the stack can be allocated by LIR_allocp, in which case each
    //   slot covered by the allocation contains a pointer to the LIR_allocp
    //   LIns.
    //
    // - The stack also holds spilled values, in which case each slot holding
    //   a spilled value (one slot for 32-bit values, two slots for 64-bit
    //   values) contains a pointer to the instruction defining the spilled
    //   value.
    //
    // - Each LIns has a "reservation" which includes a stack index,
    //   'arIndex'.  Combined with AR, it provides a two-way mapping between
    //   stack slots and LIR instructions.
    //
    // - Invariant: the two-way mapping between active stack slots and their
    //   defining/allocating instructions must hold in both directions and be
    //   unambiguous.  More specifically:
    //
    //   * An LIns can appear in at most one contiguous sequence of slots in
    //     AR, and the length of that sequence depends on the opcode (1 slot
    //     for instructions producing 32-bit values, 2 slots for instructions
    //     producing 64-bit values, N slots for LIR_allocp).
    //
    //   * An LIns named by 'entry[i]' must have an in-use reservation with
    //     arIndex==i (or an 'i' indexing the start of the same contiguous
    //     sequence that 'entry[i]' belongs to).
    //
    //   * And vice versa:  an LIns with an in-use reservation with arIndex==i
    //     must be named by 'entry[i]'.
    //
    //   * If an LIns's reservation names has arIndex==0 then LIns should not
    //     be in 'entry[]'.
    //
    class AR
    {
    private:
        uint32_t        _highWaterMark;                 /* index of highest entry used since last clear() */
        LIns*           _entries[ NJ_MAX_STACK_ENTRY ]; /* maps to 4B contiguous locations relative to the frame pointer.
                                                            NB: _entries[0] is always unused */

        #ifdef _DEBUG
        static LIns* const BAD_ENTRY;
        #endif

        bool isEmptyRange(uint32_t start, uint32_t nStackSlots) const;
        static uint32_t nStackSlotsFor(LIns* ins);

    public:
        AR();

        uint32_t stackSlotsNeeded() const;

        void clear();
        void freeEntryAt(uint32_t i);
        uint32_t reserveEntry(LIns* ins); /* return 0 if unable to reserve the entry */

        #ifdef _DEBUG
        void validateQuick();
        void validateFull();
        void validate();
        bool isValidEntry(uint32_t idx, LIns* ins) const; /* return true iff idx and ins are matched */
        void checkForResourceConsistency(const RegAlloc& regs);
        void checkForResourceLeaks() const;
        #endif

        class Iter
        {
        private:
            const AR& _ar;
            // '_i' points to the start of the entries for an LIns, or to the first NULL entry.
            uint32_t _i;
        public:
            inline Iter(const AR& ar) : _ar(ar), _i(1) { }
            bool next(LIns*& ins, uint32_t& nStackSlots, int32_t& offset);             // get the next one (moves iterator forward)
        };
    };

    inline AR::AR()
    {
         _entries[0] = NULL;
         clear();
    }

    inline /*static*/ uint32_t AR::nStackSlotsFor(LIns* ins)
    {
        uint32_t n = 0;
        if (ins->isop(LIR_allocp)) {
            n = ins->size() >> 2;
        } else {
            switch (ins->retType()) {
            case LTy_I:   n = 1;          break;
            CASE64(LTy_Q:)
            case LTy_D:   n = 2;          break;
            case LTy_V:  NanoAssert(0);  break;
            default:        NanoAssert(0);  break;
            }
        }
        return n;
    }

    inline uint32_t AR::stackSlotsNeeded() const
    {
        // NB: _highWaterMark is an index, not a count
        return _highWaterMark+1;
    }

    #ifndef AVMPLUS_ALIGN16
        #ifdef _MSC_VER
            #define AVMPLUS_ALIGN16(type) __declspec(align(16)) type
        #else
            #define AVMPLUS_ALIGN16(type) type __attribute__ ((aligned (16)))
        #endif
    #endif

    class Noise
    {
        public:
            // produce a random number from 0-maxValue for the JIT to use in attack mitigation
            virtual uint32_t getValue(uint32_t maxValue) = 0;
    };

    // error codes
    enum AssmError
    {
         None = 0
        ,StackFull
        ,UnknownBranch
        ,ConditionalBranchTooFar
    };

    typedef SeqBuilder<NIns*> NInsList;
    typedef HashMap<NIns*, LIns*> NInsMap;
#if NJ_USES_IMMD_POOL
    typedef HashMap<uint64_t, uint64_t*> ImmDPoolMap;
#endif

#ifdef VMCFG_VTUNE
    class avmplus::CodegenLIR;
#endif

    class LabelState
    {
    public:
        RegAlloc regs;
        NIns *addr;
        LabelState(NIns *a, RegAlloc &r) : regs(r), addr(a)
        {}
    };

    class LabelStateMap
    {
        Allocator& alloc;
        HashMap<LIns*, LabelState*> labels;
    public:
        LabelStateMap(Allocator& alloc) : alloc(alloc), labels(alloc)
        {}

        void clear() { labels.clear(); }
        void add(LIns *label, NIns *addr, RegAlloc &regs);
        LabelState *get(LIns *);
    };

    /** map tracking the register allocation state at each bailout point
     *  (represented by SideExit*) in a trace fragment. */
    typedef HashMap<SideExit*, RegAlloc*> RegAllocMap;

    /**
     * Information about the activation record for the method is built up
     * as we generate machine code.  As part of the prologue, we issue
     * a stack adjustment instruction and then later patch the adjustment
     * value.  Temporary values can be placed into the AR as method calls
     * are issued.   Also LIR_allocp instructions will consume space.
     */
    class Assembler
    {
        friend class VerboseBlockReader;
            #ifdef NJ_VERBOSE
        public:
            // Buffer for holding text as we generate it in reverse order.
            StringList* _outputCache;

            // Outputs the format string and 'outlineEOL', and resets
            // 'outline' and 'outlineEOL'.
            void outputf(const char* format, ...);

        private:
            // Log controller object.  Contains what-stuff-should-we-print
            // bits, and a sink function for debug printing.
            LogControl* _logc;

            // Buffer used in most of the output function.  It must big enough
            // to hold both the output line and the 'outlineEOL' buffer, which
            // is concatenated onto 'outline' just before it is printed.
            static char  outline[8192];
            // Buffer used to hold extra text to be printed at the end of some
            // lines.
            static char  outlineEOL[512];

            // Outputs 'outline' and 'outlineEOL', and resets them both.
            // Output goes to '_outputCache' if it's non-NULL, or is printed
            // directly via '_logc'.
            void output();

            // Sets 'outlineEOL'.
            void setOutputForEOL(const char* format, ...);

            void printRegState();
            void printActivationState();
            #endif // NJ_VERBOSE

        public:
            #ifdef VMCFG_VTUNE
            void* vtuneHandle;
            #endif

            Assembler(CodeAlloc& codeAlloc, Allocator& dataAlloc, Allocator& alloc, AvmCore* core, LogControl* logc, const Config& config);

            void        compile(Fragment *frag, Allocator& alloc, bool optimize
                                verbose_only(, LInsPrinter*));

            void        endAssembly(Fragment* frag);
            void        assemble(Fragment* frag, LirFilter* reader);
            void        beginAssembly(Fragment *frag);

            void        setNoiseGenerator(Noise* noise)  { _noise = noise; } // used for attack mitigation; setting to 0 disables all mitigations

            void        releaseRegisters();
            void        patch(GuardRecord *lr);
            void        patch(SideExit *exit);
#ifdef NANOJIT_IA32
            void        patch(SideExit *exit, SwitchInfo* si);
#endif
            AssmError   error()    { return _err; }
            void        setError(AssmError e) { _err = e; }

            void        reset();

            debug_only ( void       pageValidate(); )

            // support calling out from a fragment ; used to debug the jit
            debug_only( void        resourceConsistencyCheck(); )
            debug_only( void        registerConsistencyCheck(); )

            CodeList*   codeList;                   // finished blocks of code.

        private:
            void        gen(LirFilter* toCompile);
            NIns*       genPrologue();
            NIns*       genEpilogue();

            uint32_t    arReserve(LIns* ins);
            void        arFree(LIns* ins);
            void        arReset();

            Register    registerAlloc(LIns* ins, RegisterMask allow, RegisterMask prefer);
            Register    registerAllocTmp(RegisterMask allow);
            void        registerResetAll();
            void        evictAllActiveRegs() {
                // The evicted set will be be intersected with activeSet(),
                // so use an all-1s mask to avoid an extra load or call.
                evictSomeActiveRegs(~RegisterMask(0));
            }
            void        evictSomeActiveRegs(RegisterMask regs);
            void        evictScratchRegsExcept(RegisterMask ignore);
            void        intersectRegisterState(RegAlloc& saved);
            void        unionRegisterState(RegAlloc& saved);
            void        assignSaved(RegAlloc &saved, RegisterMask skip);
            LIns*       findVictim(RegisterMask allow);

            Register    getBaseReg(LIns *ins, int &d, RegisterMask allow);
            void        getBaseReg2(RegisterMask allowValue, LIns* value, Register& rv,
                                    RegisterMask allowBase, LIns* base, Register& rb, int &d);
#if NJ_USES_IMMD_POOL
            const uint64_t*
                        findImmDFromPool(uint64_t q);
#endif
            int         findMemFor(LIns* ins);
            Register    findRegFor(LIns* ins, RegisterMask allow);
            void        findRegFor2(RegisterMask allowa, LIns* ia, Register &ra,
                                    RegisterMask allowb, LIns *ib, Register &rb);
            Register    findSpecificRegFor(LIns* ins, Register r);
            Register    findSpecificRegForUnallocated(LIns* ins, Register r);
            Register    deprecated_prepResultReg(LIns *ins, RegisterMask allow);
            Register    prepareResultReg(LIns *ins, RegisterMask allow);
            void        deprecated_freeRsrcOf(LIns *ins);
            void        freeResourcesOf(LIns *ins);
            void        evictIfActive(Register r);
            void        evict(LIns* vic);
            RegisterMask hint(LIns* ins);

            void        codeAlloc(NIns *&start, NIns *&end, NIns *&eip
                                  verbose_only(, size_t &nBytes));

            // These instructions don't have to be saved & reloaded to spill,
            // they can just be recalculated cheaply.
            //
            // WARNING: this function must match asm_restore() -- it should return
            // true for the instructions that are handled explicitly without a spill
            // in asm_restore(), and false otherwise.
            //
            // If it doesn't match asm_restore(), the register allocator's decisions
            // about which values to evict will be suboptimal.
            static bool canRemat(LIns*);

            bool deprecated_isKnownReg(Register r) {
                return r != deprecated_UnknownReg;
            }

            Allocator&          alloc;              // for items with same lifetime as this Assembler
            CodeAlloc&          _codeAlloc;         // for code we generate
            Allocator&          _dataAlloc;         // for data used by generated code
            Fragment*           _thisfrag;
            RegAllocMap         _branchStateMap;
            NInsMap             _patches;
            LabelStateMap       _labels;
            Noise*              _noise;             // object to generate random noise used when hardening enabled.
        #if NJ_USES_IMMD_POOL
            ImmDPoolMap     _immDPool;
        #endif

            // We generate code into two places:  normal code chunks, and exit
            // code chunks (for exit stubs).  We use a hack to avoid having to
            // parameterise the code that does the generating -- we let that
            // code assume that it's always generating into a normal code
            // chunk (most of the time it is), and when we instead need to
            // generate into an exit code chunk, we set _inExit to true and
            // temporarily swap all the code/exit variables below (using
            // swapCodeChunks()).  Afterwards we swap them all back and set
            // _inExit to false again.
            bool        _inExit, vpad2[3];
            NIns        *codeStart, *codeEnd;   // current normal code chunk
            NIns        *exitStart, *exitEnd;   // current exit code chunk
            NIns*       _nIns;                  // current instruction in current normal code chunk
            NIns*       _nExitIns;              // current instruction in current exit code chunk
                                                // note: _nExitIns == NULL until the first side exit is seen.
        #ifdef NJ_VERBOSE
            size_t      codeBytes;              // bytes allocated in normal code chunks
            size_t      exitBytes;              // bytes allocated in exit code chunks
        #endif

            #define     SWAP(t, a, b)   do { t tmp = a; a = b; b = tmp; } while (0)
            void        swapCodeChunks();

            NIns*       _epilogue;
            AssmError   _err;           // 0 = means assemble() appears ok, otherwise it failed
        #if PEDANTIC
            NIns*       pedanticTop;
        #endif

            // Holds the current instruction during gen().
            LIns*       currIns;

            AR          _activation;
            RegAlloc    _allocator;

            verbose_only( void asm_inc_m32(uint32_t*); )
            void        asm_mmq(Register rd, int dd, Register rs, int ds);
            void        asm_jmp(LIns* ins, InsList& pending_lives);
            void        asm_jcc(LIns* ins, InsList& pending_lives);
            void        asm_jov(LIns* ins, InsList& pending_lives);
            void        asm_x(LIns* ins);
            void        asm_xcc(LIns* ins);
            NIns*       asm_exit(LIns* guard);
            NIns*       asm_leave_trace(LIns* guard);
            void        asm_store32(LOpcode op, LIns *val, int d, LIns *base);
            void        asm_store64(LOpcode op, LIns *val, int d, LIns *base);

            // WARNING: the implementation of asm_restore() should emit fast code
            // to rematerialize instructions where canRemat() returns true.
            // Otherwise, register allocation decisions will be suboptimal.
            void        asm_restore(LIns*, Register);

            bool        asm_maybe_spill(LIns* ins, bool pop);
#ifdef NANOJIT_IA32
            void        asm_spill(Register rr, int d, bool pop);
#else
            void        asm_spill(Register rr, int d, bool quad);
#endif
            void        asm_load64(LIns* ins);
            void        asm_ret(LIns* ins);
#ifdef NANOJIT_64BIT
            void        asm_immq(LIns* ins);
#endif
            void        asm_immd(LIns* ins);
            void        asm_condd(LIns* ins);
            void        asm_cond(LIns* ins);
            void        asm_arith(LIns* ins);
            void        asm_neg_not(LIns* ins);
            void        asm_load32(LIns* ins);
            void        asm_cmov(LIns* ins);
            void        asm_param(LIns* ins);
            void        asm_immi(LIns* ins);
#if NJ_SOFTFLOAT_SUPPORTED
            void        asm_qlo(LIns* ins);
            void        asm_qhi(LIns* ins);
            void        asm_qjoin(LIns *ins);
#endif
            void        asm_fneg(LIns* ins);
            void        asm_fop(LIns* ins);
            void        asm_i2d(LIns* ins);
            void        asm_ui2d(LIns* ins);
            void        asm_d2i(LIns* ins);
#ifdef NANOJIT_64BIT
            void        asm_q2i(LIns* ins);
            void        asm_ui2uq(LIns *ins);
            void        asm_dasq(LIns *ins);
            void        asm_qasd(LIns *ins);
#endif
            void        asm_nongp_copy(Register r, Register s);
            void        asm_call(LIns*);
            Register    asm_binop_rhs_reg(LIns* ins);
            NIns*       asm_branch(bool branchOnFalse, LIns* cond, NIns* targ);
            NIns*       asm_branch_ov(LOpcode op, NIns* targ);
            void        asm_switch(LIns* ins, NIns* target);
            void        asm_jtbl(LIns* ins, NIns** table);
            void        emitJumpTable(SwitchInfo* si, NIns* target);
            void        assignSavedRegs();
            void        reserveSavedRegs();
            void        assignParamRegs();
            void        handleLoopCarriedExprs(InsList& pending_lives);

            // platform specific implementation (see NativeXXX.cpp file)
            void        nInit(AvmCore *);
            void        nBeginAssembly();
            Register    nRegisterAllocFromSet(RegisterMask set);
            void        nRegisterResetAll(RegAlloc& a);
            static void nPatchBranch(NIns* branch, NIns* location);
            void        nFragExit(LIns* guard);

            static RegisterMask nHints[LIR_sentinel+1];
            RegisterMask nHint(LIns* ins);

            // A special entry for hints[];  if an opcode has this value, we call
            // nHint() in the back-end.  Used for cases where you need to look at more
            // than just the opcode to decide.
            static const RegisterMask PREFER_SPECIAL = 0xffffffff;

            // platform specific methods
        public:
            const static Register savedRegs[NumSavedRegs+1]; // Allocate an extra element in case NumSavedRegs == 0
            DECLARE_PLATFORM_ASSEMBLER()

        private:
#ifdef NANOJIT_IA32
            debug_only( int32_t _fpuStkDepth; )
            debug_only( int32_t _sv_fpuStkDepth; )

            // The FPU stack depth is the number of pushes in excess of the number of pops.
            // Since we generate backwards, we track the FPU stack depth as a negative number.
            // We use the top of the x87 stack as the single allocatable FP register, FST0.
            // Thus, between LIR instructions, the depth of the FPU stack must be either 0 or -1,
            // depending on whether FST0 is in use.  Within the expansion of a single LIR
            // instruction, however, deeper levels of the stack may be used as unmanaged
            // temporaries.  Hence, we allow for all eight levels in the assertions below.
            inline void fpu_push() {
                debug_only( ++_fpuStkDepth; NanoAssert(_fpuStkDepth <= 0); )
            }
            inline void fpu_pop() {
                debug_only( --_fpuStkDepth; NanoAssert(_fpuStkDepth >= -7); )
            }
#endif
            const Config& _config;
    };

    inline int32_t arDisp(LIns* ins)
    {
        // even on 64bit cpu's, we allocate stack area in 4byte chunks
        return -4 * int32_t(ins->getArIndex());
    }
    // XXX: deprecated, use arDisp() instead.  See bug 538924.
    inline int32_t deprecated_disp(LIns* ins)
    {
        // even on 64bit cpu's, we allocate stack area in 4byte chunks
        return -4 * int32_t(ins->deprecated_getArIndex());
    }
}
#endif // __nanojit_Assembler__
