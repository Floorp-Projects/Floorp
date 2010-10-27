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

#ifndef __nanojit_LIR__
#define __nanojit_LIR__

namespace nanojit
{
    enum LOpcode
#if defined(_MSC_VER) && _MSC_VER >= 1400
#pragma warning(disable:4480) // nonstandard extension used: specifying underlying type for enum
          : unsigned
#endif
    {
#define OP___(op, number, repKind, retType, isCse) \
        LIR_##op = (number),
#include "LIRopcode.tbl"
        LIR_sentinel,
#undef OP___

#ifdef NANOJIT_64BIT
#  define PTR_SIZE(a,b)  b
#else
#  define PTR_SIZE(a,b)  a
#endif

        // Pointer-sized synonyms.

        LIR_paramp  = PTR_SIZE(LIR_parami,  LIR_paramq),

        LIR_retp    = PTR_SIZE(LIR_reti,    LIR_retq),

        LIR_livep   = PTR_SIZE(LIR_livei,   LIR_liveq),

        LIR_ldp     = PTR_SIZE(LIR_ldi,     LIR_ldq),

        LIR_stp     = PTR_SIZE(LIR_sti,     LIR_stq),

        LIR_callp   = PTR_SIZE(LIR_calli,   LIR_callq),

        LIR_eqp     = PTR_SIZE(LIR_eqi,     LIR_eqq),
        LIR_ltp     = PTR_SIZE(LIR_lti,     LIR_ltq),
        LIR_gtp     = PTR_SIZE(LIR_gti,     LIR_gtq),
        LIR_lep     = PTR_SIZE(LIR_lei,     LIR_leq),
        LIR_gep     = PTR_SIZE(LIR_gei,     LIR_geq),
        LIR_ltup    = PTR_SIZE(LIR_ltui,    LIR_ltuq),
        LIR_gtup    = PTR_SIZE(LIR_gtui,    LIR_gtuq),
        LIR_leup    = PTR_SIZE(LIR_leui,    LIR_leuq),
        LIR_geup    = PTR_SIZE(LIR_geui,    LIR_geuq),

        LIR_addp    = PTR_SIZE(LIR_addi,    LIR_addq),
        LIR_subp    = PTR_SIZE(LIR_subi,    LIR_subq),
        LIR_addjovp = PTR_SIZE(LIR_addjovi, LIR_addjovq),

        LIR_andp    = PTR_SIZE(LIR_andi,    LIR_andq),
        LIR_orp     = PTR_SIZE(LIR_ori,     LIR_orq),
        LIR_xorp    = PTR_SIZE(LIR_xori,    LIR_xorq),

        LIR_lshp    = PTR_SIZE(LIR_lshi,    LIR_lshq),
        LIR_rshp    = PTR_SIZE(LIR_rshi,    LIR_rshq),
        LIR_rshup   = PTR_SIZE(LIR_rshui,   LIR_rshuq),

        LIR_cmovp   = PTR_SIZE(LIR_cmovi,   LIR_cmovq)
    };

    // 32-bit integer comparisons must be contiguous, as must 64-bit integer
    // comparisons and 64-bit float comparisons.
    NanoStaticAssert(LIR_eqi + 1 == LIR_lti  &&
                     LIR_eqi + 2 == LIR_gti  &&
                     LIR_eqi + 3 == LIR_lei  &&
                     LIR_eqi + 4 == LIR_gei  &&
                     LIR_eqi + 5 == LIR_ltui &&
                     LIR_eqi + 6 == LIR_gtui &&
                     LIR_eqi + 7 == LIR_leui &&
                     LIR_eqi + 8 == LIR_geui);
#ifdef NANOJIT_64BIT
    NanoStaticAssert(LIR_eqq + 1 == LIR_ltq  &&
                     LIR_eqq + 2 == LIR_gtq  &&
                     LIR_eqq + 3 == LIR_leq  &&
                     LIR_eqq + 4 == LIR_geq  &&
                     LIR_eqq + 5 == LIR_ltuq &&
                     LIR_eqq + 6 == LIR_gtuq &&
                     LIR_eqq + 7 == LIR_leuq &&
                     LIR_eqq + 8 == LIR_geuq);
#endif
    NanoStaticAssert(LIR_eqd + 1 == LIR_ltd &&
                     LIR_eqd + 2 == LIR_gtd &&
                     LIR_eqd + 3 == LIR_led &&
                     LIR_eqd + 4 == LIR_ged);

    // Various opcodes must be changeable to their opposite with op^1
    // (although we use invertXyz() when possible, ie. outside static
    // assertions).
    NanoStaticAssert((LIR_jt^1) == LIR_jf && (LIR_jf^1) == LIR_jt);

    NanoStaticAssert((LIR_xt^1) == LIR_xf && (LIR_xf^1) == LIR_xt);

    NanoStaticAssert((LIR_lti^1)  == LIR_gti  && (LIR_gti^1)  == LIR_lti);
    NanoStaticAssert((LIR_lei^1)  == LIR_gei  && (LIR_gei^1)  == LIR_lei);
    NanoStaticAssert((LIR_ltui^1) == LIR_gtui && (LIR_gtui^1) == LIR_ltui);
    NanoStaticAssert((LIR_leui^1) == LIR_geui && (LIR_geui^1) == LIR_leui);

#ifdef NANOJIT_64BIT
    NanoStaticAssert((LIR_ltq^1)  == LIR_gtq  && (LIR_gtq^1)  == LIR_ltq);
    NanoStaticAssert((LIR_leq^1)  == LIR_geq  && (LIR_geq^1)  == LIR_leq);
    NanoStaticAssert((LIR_ltuq^1) == LIR_gtuq && (LIR_gtuq^1) == LIR_ltuq);
    NanoStaticAssert((LIR_leuq^1) == LIR_geuq && (LIR_geuq^1) == LIR_leuq);
#endif

    NanoStaticAssert((LIR_ltd^1) == LIR_gtd && (LIR_gtd^1) == LIR_ltd);
    NanoStaticAssert((LIR_led^1) == LIR_ged && (LIR_ged^1) == LIR_led);


    struct GuardRecord;
    struct SideExit;

    enum AbiKind {
        ABI_FASTCALL,
        ABI_THISCALL,
        ABI_STDCALL,
        ABI_CDECL
    };

    // This is much the same as LTy, but we need to distinguish signed and
    // unsigned 32-bit ints so that they will be extended to 64-bits correctly
    // on 64-bit platforms.
    //
    // All values must fit into three bits.  See CallInfo for details.
    enum ArgType {
        ARGTYPE_V  = 0,     // void
        ARGTYPE_I  = 1,     // int32_t
        ARGTYPE_UI = 2,     // uint32_t
#ifdef NANOJIT_64BIT
        ARGTYPE_Q  = 3,     // uint64_t
#endif
        ARGTYPE_D  = 4,     // double

        // aliases
        ARGTYPE_P = PTR_SIZE(ARGTYPE_I, ARGTYPE_Q), // pointer
        ARGTYPE_B = ARGTYPE_I                       // bool
    };

    enum IndirectCall {
        CALL_INDIRECT = 0
    };

    //-----------------------------------------------------------------------
    // Aliasing
    // --------
    // *Aliasing* occurs when a single memory location can be accessed through
    // multiple names.  For example, consider this code:
    //
    //   ld a[0]
    //   sti b[0]
    //   ld a[0]
    //
    // In general, it's possible that a[0] and b[0] may refer to the same
    // memory location.  This means, for example, that you cannot safely
    // perform CSE on the two loads.  However, if you know that 'a' cannot be
    // an alias of 'b' (ie. the two loads do not alias with the store) then
    // you can safely perform CSE.
    //
    // Access regions
    // --------------
    // Doing alias analysis precisely is difficult.  But it turns out that
    // keeping track of aliasing at a coarse level is enough to help with many
    // optimisations.  So we conceptually divide the memory that is accessible
    // from LIR into a small number of "access regions" (aka. "Acc").  An
    // access region may be non-contiguous.  No two access regions can
    // overlap.  The union of all access regions covers all memory accessible
    // from LIR.
    //
    // In general a (static) load or store may be executed more than once, and
    // thus may access multiple regions;  however, in practice almost all
    // loads and stores will obviously access only a single region.  A
    // function called from LIR may load and/or store multiple access regions
    // (even if executed only once).
    //
    // If two loads/stores/calls are known to not access the same region(s),
    // then they do not alias.
    //
    // All regions are defined by the embedding.  It makes sense to add new
    // embedding-specific access regions when doing so will help with one or
    // more optimisations.
    //
    // Access region sets and instruction markings
    // -------------------------------------------
    // Each load/store is marked with an "access region set" (aka. "AccSet"),
    // which is a set of one or more access regions.  This indicates which
    // parts of LIR-accessible memory the load/store may touch.
    //
    // Each function called from LIR is also marked with an access region set
    // for memory stored to by the function.  (We could also have a marking
    // for memory loads done by the function, but there's no need at the
    // moment.)  These markings apply to the function itself, not the call
    // site, ie. they're not context-sensitive.
    //
    // These load/store/call markings MUST BE ACCURATE -- if not then invalid
    // optimisations might occur that change the meaning of the code.
    // However, they can safely be imprecise (ie. conservative), ie. a
    // load/store/call can be marked with an access region set that is a
    // superset of the actual access region set.  Such imprecision is safe but
    // may reduce optimisation opportunities.
    //
    // Optimisations that use access region info
    // -----------------------------------------
    // Currently only CseFilter uses this, and only for determining whether
    // loads can be CSE'd.  Note that CseFilter treats loads that are marked
    // with a single access region precisely, but all loads marked with
    // multiple access regions get lumped together.  So if you can't mark a
    // load with a single access region, you might as well use ACC_LOAD_ANY.
    //-----------------------------------------------------------------------

    // An access region set is represented as a bitset.  Using a uint32_t
    // restricts us to at most 32 alias regions for the moment.  This could be
    // expanded to a uint64_t easily if needed.
    typedef uint32_t AccSet;
    static const int NUM_ACCS = sizeof(AccSet) * 8;

    // Some common (non-singleton) access region sets.  ACCSET_NONE does not make
    // sense for loads or stores (which must access at least one region), it
    // only makes sense for calls.
    //
    static const AccSet ACCSET_NONE      = 0x0;
    static const AccSet ACCSET_ALL       = 0xffffffff;
    static const AccSet ACCSET_LOAD_ANY  = ACCSET_ALL;      // synonym
    static const AccSet ACCSET_STORE_ANY = ACCSET_ALL;      // synonym

    inline bool isSingletonAccSet(AccSet accSet) {
        // This is a neat way of testing if a value has only one bit set.
        return (accSet & (accSet - 1)) == 0;
    }

    // Full AccSets don't fit into load and store instructions.  But
    // load/store AccSets almost always contain a single access region.  We
    // take advantage of this to create a compressed AccSet, MiniAccSet, that
    // does fit.
    //
    // The 32 single-region AccSets get compressed into a number in the range
    // 0..31 (according to the position of the set bit), and all other
    // (multi-region) AccSets get converted into MINI_ACCSET_MULTIPLE.  So the
    // representation is lossy in the latter case, but that case is rare for
    // loads/stores.  We use a full AccSet for the storeAccSets of calls, for
    // which multi-region AccSets are common.
    //
    // We wrap the uint8_t inside a struct to avoid the possiblity of subtle
    // bugs caused by mixing up AccSet and MiniAccSet, which is easy to do.
    // However, the struct gets padded inside LInsLd in an inconsistent way on
    // Windows, so we actually store a MiniAccSetVal inside LInsLd.  Sigh.
    // But we use MiniAccSet everywhere else.
    //
    typedef uint8_t MiniAccSetVal;
    struct MiniAccSet { MiniAccSetVal val; };
    static const MiniAccSet MINI_ACCSET_MULTIPLE = { 99 };

    static MiniAccSet compressAccSet(AccSet accSet) {
        if (isSingletonAccSet(accSet)) {
            MiniAccSet ret = { uint8_t(msbSet32(accSet)) };
            return ret;
        }

        // If we got here, it must be a multi-region AccSet.
        return MINI_ACCSET_MULTIPLE;
    }

    static AccSet decompressMiniAccSet(MiniAccSet miniAccSet) {
        return (miniAccSet.val == MINI_ACCSET_MULTIPLE.val) ? ACCSET_ALL : (1 << miniAccSet.val);
    }

    // The LoadQual affects how a load can be optimised:
    //
    // - CONST: These loads are guaranteed to always return the same value
    //   during a single execution of a fragment (but the value is allowed to
    //   change between executions of the fragment).  This means that the
    //   location is never stored to by the LIR, and is never modified by an
    //   external entity while the fragment is running.
    //
    // - NORMAL: These loads may be stored to by the LIR, but are never
    //   modified by an external entity while the fragment is running.
    //
    // - VOLATILE: These loads may be stored to by the LIR, and may be
    //   modified by an external entity while the fragment is running.
    //
    // This gives a lattice with the ordering:  CONST < NORMAL < VOLATILE.
    // As usual, it's safe to mark a load with a value higher (less precise)
    // that actual, but it may result in fewer optimisations occurring.
    //
    // Generally CONST loads are highly amenable to optimisation (eg. CSE),
    // VOLATILE loads are entirely unoptimisable, and NORMAL loads are in
    // between and require some alias analysis to optimise.
    //
    // Note that CONST has a stronger meaning to "const" in C and C++;  in C
    // and C++ a "const" variable may be modified by an external entity, such
    // as hardware.  Hence "const volatile" makes sense in C and C++, but
    // CONST+VOLATILE doesn't make sense in LIR.
    //
    // Note also that a 2-bit bitfield in LInsLd is used to hold LoadQual
    // values, so you can one add one more value without expanding it.
    //
    enum LoadQual {
        LOAD_CONST    = 0,
        LOAD_NORMAL   = 1,
        LOAD_VOLATILE = 2
    };

    struct CallInfo
    {
    private:
        // In CallInfo::_typesig, each entry is three bits.
        static const int TYPESIG_FIELDSZB = 3;
        static const int TYPESIG_FIELDMASK = 7;

    public:
        uintptr_t   _address;
        uint32_t    _typesig:27;     // 9 3-bit fields indicating arg type, by ARGTYPE above (including ret type): a1 a2 a3 a4 a5 ret
        AbiKind     _abi:3;
        uint32_t    _isPure:1;      // _isPure=1 means no side-effects, result only depends on args
        AccSet      _storeAccSet;   // access regions stored by the function
        verbose_only ( const char* _name; )

        // The following encode 'r func()' through to 'r func(a1, a2, a3, a4, a5, a6, a7, a8)'.
        static inline uint32_t typeSig0(ArgType r) {
            return r;
        }
        static inline uint32_t typeSig1(ArgType r, ArgType a1) {
            return a1 << TYPESIG_FIELDSZB*1 | typeSig0(r);
        }
        static inline uint32_t typeSig2(ArgType r, ArgType a1, ArgType a2) {
            return a1 << TYPESIG_FIELDSZB*2 | typeSig1(r, a2);
        }
        static inline uint32_t typeSig3(ArgType r, ArgType a1, ArgType a2, ArgType a3) {
            return a1 << TYPESIG_FIELDSZB*3 | typeSig2(r, a2, a3);
        }
        static inline uint32_t typeSig4(ArgType r, ArgType a1, ArgType a2, ArgType a3, ArgType a4) {
            return a1 << TYPESIG_FIELDSZB*4 | typeSig3(r, a2, a3, a4);
        }
        static inline uint32_t typeSig5(ArgType r,  ArgType a1, ArgType a2, ArgType a3,
                                 ArgType a4, ArgType a5) {
            return a1 << TYPESIG_FIELDSZB*5 | typeSig4(r, a2, a3, a4, a5);
        }
        static inline uint32_t typeSig6(ArgType r, ArgType a1, ArgType a2, ArgType a3,
                                 ArgType a4, ArgType a5, ArgType a6) {
            return a1 << TYPESIG_FIELDSZB*6 | typeSig5(r, a2, a3, a4, a5, a6);
        }
        static inline uint32_t typeSig7(ArgType r,  ArgType a1, ArgType a2, ArgType a3,
                                 ArgType a4, ArgType a5, ArgType a6, ArgType a7) {
            return a1 << TYPESIG_FIELDSZB*7 | typeSig6(r, a2, a3, a4, a5, a6, a7);
        }
        static inline uint32_t typeSig8(ArgType r,  ArgType a1, ArgType a2, ArgType a3, ArgType a4,
                                 ArgType a5, ArgType a6, ArgType a7, ArgType a8) {
            return a1 << TYPESIG_FIELDSZB*8 | typeSig7(r, a2, a3, a4, a5, a6, a7, a8);
        }
        // Encode 'r func(a1, ..., aN))'
        static inline uint32_t typeSigN(ArgType r, int N, ArgType a[]) {
            uint32_t typesig = r;
            for (int i = 0; i < N; i++) {
                typesig |= a[i] << TYPESIG_FIELDSZB*(N-i);
            }
            return typesig;
        }

        uint32_t count_args() const;
        uint32_t count_int32_args() const;
        // Nb: uses right-to-left order, eg. sizes[0] is the size of the right-most arg.
        // XXX: See bug 525815 for fixing this.
        uint32_t getArgTypes(ArgType* types) const;

        inline ArgType returnType() const {
            return ArgType(_typesig & TYPESIG_FIELDMASK);
        }

        inline bool isIndirect() const {
            return _address < 256;
        }
    };

    /*
     * Record for extra data used to compile switches as jump tables.
     */
    struct SwitchInfo
    {
        NIns**      table;       // Jump table; a jump address is NIns*
        uint32_t    count;       // Number of table entries
        // Index value at last execution of the switch. The index value
        // is the offset into the jump table. Thus it is computed as
        // (switch expression) - (lowest case value).
        uint32_t    index;
    };

    // Array holding the 'isCse' field from LIRopcode.tbl.
    extern const int8_t isCses[];       // cannot be uint8_t, some values are negative

    inline bool isCseOpcode(LOpcode op) {
        NanoAssert(isCses[op] != -1);   // see LIRopcode.tbl to understand this
        return isCses[op] == 1;
    }
    inline bool isLiveOpcode(LOpcode op) {
        return
#if defined NANOJIT_64BIT
               op == LIR_liveq ||
#endif
               op == LIR_livei || op == LIR_lived;
    }
    inline bool isRetOpcode(LOpcode op) {
        return
#if defined NANOJIT_64BIT
            op == LIR_retq ||
#endif
            op == LIR_reti || op == LIR_retd;
    }
    inline bool isCmovOpcode(LOpcode op) {
        return
#if defined NANOJIT_64BIT
            op == LIR_cmovq ||
#endif
            op == LIR_cmovi ||
            op == LIR_cmovd;
    }
    inline bool isCmpIOpcode(LOpcode op) {
        return LIR_eqi <= op && op <= LIR_geui;
    }
    inline bool isCmpSIOpcode(LOpcode op) {
        return LIR_eqi <= op && op <= LIR_gei;
    }
    inline bool isCmpUIOpcode(LOpcode op) {
        return LIR_eqi == op || (LIR_ltui <= op && op <= LIR_geui);
    }
#ifdef NANOJIT_64BIT
    inline bool isCmpQOpcode(LOpcode op) {
        return LIR_eqq <= op && op <= LIR_geuq;
    }
    inline bool isCmpSQOpcode(LOpcode op) {
        return LIR_eqq <= op && op <= LIR_geq;
    }
    inline bool isCmpUQOpcode(LOpcode op) {
        return LIR_eqq == op || (LIR_ltuq <= op && op <= LIR_geuq);
    }
#endif
    inline bool isCmpDOpcode(LOpcode op) {
        return LIR_eqd <= op && op <= LIR_ged;
    }
    inline bool isCmpOpcode(LOpcode op) {
        return isCmpIOpcode(op) ||
#if defined NANOJIT_64BIT
               isCmpQOpcode(op) ||
#endif
               isCmpDOpcode(op);
    }

    inline LOpcode invertCondJmpOpcode(LOpcode op) {
        NanoAssert(op == LIR_jt || op == LIR_jf);
        return LOpcode(op ^ 1);
    }
    inline LOpcode invertCondGuardOpcode(LOpcode op) {
        NanoAssert(op == LIR_xt || op == LIR_xf);
        return LOpcode(op ^ 1);
    }
    inline LOpcode invertCmpOpcode(LOpcode op) {
        NanoAssert(isCmpOpcode(op));
        return LOpcode(op ^ 1);
    }

    inline LOpcode getCallOpcode(const CallInfo* ci) {
        LOpcode op = LIR_callp;
        switch (ci->returnType()) {
        case ARGTYPE_V: op = LIR_callv; break;
        case ARGTYPE_I:
        case ARGTYPE_UI: op = LIR_calli; break;
#ifdef NANOJIT_64BIT
        case ARGTYPE_Q: op = LIR_callq; break;
#endif
        case ARGTYPE_D: op = LIR_calld; break;
        default:        NanoAssert(0);  break;
        }
        return op;
    }

    LOpcode arithOpcodeD2I(LOpcode op);
#ifdef NANOJIT_64BIT
    LOpcode cmpOpcodeI2Q(LOpcode op);
#endif
    LOpcode cmpOpcodeD2I(LOpcode op);
    LOpcode cmpOpcodeD2UI(LOpcode op);

    // Array holding the 'repKind' field from LIRopcode.tbl.
    extern const uint8_t repKinds[];

    enum LTy {
        LTy_V,  // void: no value/no type
        LTy_I,  // int:  32-bit integer
#ifdef NANOJIT_64BIT
        LTy_Q,  // quad: 64-bit integer
#endif
        LTy_D,  // double: 64-bit float

        LTy_P  = PTR_SIZE(LTy_I, LTy_Q)   // word-sized integer
    };

    // Array holding the 'retType' field from LIRopcode.tbl.
    extern const LTy retTypes[];

    inline RegisterMask rmask(Register r)
    {
        return RegisterMask(1) << REGNUM(r);
    }

    //-----------------------------------------------------------------------
    // Low-level instructions.  This is a bit complicated, because we have a
    // variable-width representation to minimise space usage.
    //
    // - Instruction size is always an integral multiple of word size.
    //
    // - Every instruction has at least one word, holding the opcode and the
    //   reservation info ("SharedFields").  That word is in class LIns.
    //
    // - Beyond that, most instructions have 1, 2 or 3 extra words.  These
    //   extra words are in classes LInsOp1, LInsOp2, etc (collectively called
    //   "LInsXYZ" in what follows).  Each LInsXYZ class also contains an LIns,
    //   accessible by the 'ins' member, which holds the LIns data.
    //
    // - LIR is written forward, but read backwards.  When reading backwards,
    //   in order to find the opcode, it must be in a predictable place in the
    //   LInsXYZ isn't affected by instruction width.  Therefore, the LIns
    //   word (which contains the opcode) is always the *last* word in an
    //   instruction.
    //
    // - Each instruction is created by casting pre-allocated bytes from a
    //   LirBuffer to the LInsXYZ type.  Therefore there are no constructors
    //   for LIns or LInsXYZ.
    //
    // - The standard handle for an instruction is a LIns*.  This actually
    //   points to the LIns word, ie. to the final word in the instruction.
    //   This is a bit odd, but it allows the instruction's opcode to be
    //   easily accessed.  Once you've looked at the opcode and know what kind
    //   of instruction it is, if you want to access any of the other words,
    //   you need to use toLInsXYZ(), which takes the LIns* and gives you an
    //   LInsXYZ*, ie. the pointer to the actual start of the instruction's
    //   bytes.  From there you can access the instruction-specific extra
    //   words.
    //
    // - However, from outside class LIns, LInsXYZ isn't visible, nor is
    //   toLInsXYZ() -- from outside LIns, all LIR instructions are handled
    //   via LIns pointers and get/set methods are used for all LIns/LInsXYZ
    //   accesses.  In fact, all data members in LInsXYZ are private and can
    //   only be accessed by LIns, which is a friend class.  The only thing
    //   anyone outside LIns can do with a LInsXYZ is call getLIns().
    //
    // - An example Op2 instruction and the likely pointers to it (each line
    //   represents a word, and pointers to a line point to the start of the
    //   word on that line):
    //
    //      [ oprnd_2         <-- LInsOp2* insOp2 == toLInsOp2(ins)
    //        oprnd_1
    //        opcode + resv ] <-- LIns* ins
    //
    // - LIR_skip instructions are used to link code chunks.  If the first
    //   instruction on a chunk isn't a LIR_start, it will be a skip, and the
    //   skip's operand will point to the last LIns on the preceding chunk.
    //   LInsSk has the same layout as LInsOp1, but we represent it as a
    //   different class because there are some places where we treat
    //   skips specially and so having it separate seems like a good idea.
    //
    // - Various things about the size and layout of LIns and LInsXYZ are
    //   statically checked in staticSanityCheck().  In particular, this is
    //   worthwhile because there's nothing that guarantees that all the
    //   LInsXYZ classes have a size that is a multiple of word size (but in
    //   practice all sane compilers use a layout that results in this).  We
    //   also check that every LInsXYZ is word-aligned in
    //   LirBuffer::makeRoom();  this seems sensible to avoid potential
    //   slowdowns due to misalignment.  It relies on chunks themselves being
    //   word-aligned, which is extremely likely.
    //
    // - There is an enum, LInsRepKind, with one member for each of the
    //   LInsXYZ kinds.  Each opcode is categorised with its LInsRepKind value
    //   in LIRopcode.tbl, and this is used in various places.
    //-----------------------------------------------------------------------

    enum LInsRepKind {
        // LRK_XYZ corresponds to class LInsXYZ.
        LRK_Op0,
        LRK_Op1,
        LRK_Op2,
        LRK_Op3,
        LRK_Ld,
        LRK_St,
        LRK_Sk,
        LRK_C,
        LRK_P,
        LRK_I,
        LRK_QorD,
        LRK_Jtbl,
        LRK_None    // this one is used for unused opcode numbers
    };

    class LInsOp0;
    class LInsOp1;
    class LInsOp2;
    class LInsOp3;
    class LInsLd;
    class LInsSt;
    class LInsSk;
    class LInsC;
    class LInsP;
    class LInsI;
    class LInsQorD;
    class LInsJtbl;

    class LIns
    {
    private:
        // SharedFields: fields shared by all LIns kinds.
        //
        // The .inReg, .regnum, .inAr and .arIndex fields form a "reservation"
        // that is used temporarily during assembly to record information
        // relating to register allocation.  See class RegAlloc for more
        // details.  Note: all combinations of .inReg/.inAr are possible, ie.
        // 0/0, 0/1, 1/0, 1/1.
        //
        // The .isResultLive field is only used for instructions that return
        // results.  It indicates if the result is live.  It's set (if
        // appropriate) and used only during the codegen pass.
        //
        struct SharedFields {
            uint32_t inReg:1;           // if 1, 'reg' is active
            uint32_t regnum:7;
            uint32_t inAr:1;            // if 1, 'arIndex' is active
            uint32_t isResultLive:1;    // if 1, the instruction's result is live

            uint32_t arIndex:14;        // index into stack frame;  displ is -4*arIndex

            LOpcode  opcode:8;          // instruction's opcode
        };

        union {
            SharedFields sharedFields;
            // Force sizeof(LIns)==8 and 8-byte alignment on 64-bit machines.
            // This is necessary because sizeof(SharedFields)==4 and we want all
            // instances of LIns to be pointer-aligned.
            void* wholeWord;
        };

        inline void initSharedFields(LOpcode opcode)
        {
            // We must zero .inReg, .inAR and .isResultLive, but zeroing the
            // whole word is easier.  Then we set the opcode.
            wholeWord = 0;
            sharedFields.opcode = opcode;
        }

        // LIns-to-LInsXYZ converters.
        inline LInsOp0* toLInsOp0() const;
        inline LInsOp1* toLInsOp1() const;
        inline LInsOp2* toLInsOp2() const;
        inline LInsOp3* toLInsOp3() const;
        inline LInsLd*  toLInsLd()  const;
        inline LInsSt*  toLInsSt()  const;
        inline LInsSk*  toLInsSk()  const;
        inline LInsC*   toLInsC()   const;
        inline LInsP*   toLInsP()   const;
        inline LInsI*   toLInsI()   const;
        inline LInsQorD* toLInsQorD() const;
        inline LInsJtbl*toLInsJtbl()const;

        void staticSanityCheck();

    public:
        // LIns initializers.
        inline void initLInsOp0(LOpcode opcode);
        inline void initLInsOp1(LOpcode opcode, LIns* oprnd1);
        inline void initLInsOp2(LOpcode opcode, LIns* oprnd1, LIns* oprnd2);
        inline void initLInsOp3(LOpcode opcode, LIns* oprnd1, LIns* oprnd2, LIns* oprnd3);
        inline void initLInsLd(LOpcode opcode, LIns* val, int32_t d, AccSet accSet, LoadQual loadQual);
        inline void initLInsSt(LOpcode opcode, LIns* val, LIns* base, int32_t d, AccSet accSet);
        inline void initLInsSk(LIns* prevLIns);
        // Nb: args[] must be allocated and initialised before being passed in;
        // initLInsC() just copies the pointer into the LInsC.
        inline void initLInsC(LOpcode opcode, LIns** args, const CallInfo* ci);
        inline void initLInsP(int32_t arg, int32_t kind);
        inline void initLInsI(LOpcode opcode, int32_t immI);
        inline void initLInsQorD(LOpcode opcode, uint64_t immQorD);
        inline void initLInsJtbl(LIns* index, uint32_t size, LIns** table);

        LOpcode opcode() const { return sharedFields.opcode; }

        // Generally, void instructions (statements) are always live and
        // non-void instructions (expressions) are live if used by another
        // live instruction.  But there are some trickier cases.
        bool isLive() const {
            return isV() ||
                   sharedFields.isResultLive ||
                   (isCall() && !callInfo()->_isPure) ||    // impure calls are always live
                   isop(LIR_paramp);                        // LIR_paramp is always live
        }
        void setResultLive() {
            NanoAssert(!isV());
            sharedFields.isResultLive = 1;
        }

        // XXX: old reservation manipulating functions.  See bug 538924.
        // Replacement strategy:
        // - deprecated_markAsClear() --> clearReg() and/or clearArIndex()
        // - deprecated_hasKnownReg() --> isInReg()
        // - deprecated_getReg() --> getReg() after checking isInReg()
        //
        void deprecated_markAsClear() {
            sharedFields.inReg = 0;
            sharedFields.inAr = 0;
        }
        bool deprecated_hasKnownReg() {
            NanoAssert(isExtant());
            return isInReg();
        }
        Register deprecated_getReg() {
            NanoAssert(isExtant());
            if (isInReg()) {
                Register r = { sharedFields.regnum };
                return r;
            } else { 
                return deprecated_UnknownReg;
            }
        }
        uint32_t deprecated_getArIndex() {
            NanoAssert(isExtant());
            return ( isInAr() ? sharedFields.arIndex : 0 );
        }

        // Reservation manipulation.
        //
        // "Extant" mean "in existence, still existing, surviving".  In other
        // words, has the value been computed explicitly (not folded into
        // something else) and is it still available (in a register or spill
        // slot) for use?
        bool isExtant() {
            return isInReg() || isInAr();
        }
        bool isInReg() {
            return sharedFields.inReg;
        }
        bool isInRegMask(RegisterMask allow) {
            return isInReg() && (rmask(getReg()) & allow);
        }
        Register getReg() {
            NanoAssert(isInReg());
            Register r = { sharedFields.regnum };
            return r;
        }
        void setReg(Register r) {
            sharedFields.inReg = 1;
            sharedFields.regnum = REGNUM(r);
        }
        void clearReg() {
            sharedFields.inReg = 0;
        }
        bool isInAr() {
            return sharedFields.inAr;
        }
        uint32_t getArIndex() {
            NanoAssert(isInAr());
            return sharedFields.arIndex;
        }
        void setArIndex(uint32_t arIndex) {
            sharedFields.inAr = 1;
            sharedFields.arIndex = arIndex;
        }
        void clearArIndex() {
            sharedFields.inAr = 0;
        }

        // For various instruction kinds.
        inline LIns*    oprnd1() const;
        inline LIns*    oprnd2() const;
        inline LIns*    oprnd3() const;

        // For branches.
        inline LIns*    getTarget() const;
        inline void     setTarget(LIns* label);

        // For guards.
        inline GuardRecord* record() const;

        // For loads.
        inline LoadQual loadQual() const;

        // For loads/stores.
        inline int32_t  disp() const;
        inline MiniAccSet miniAccSet() const;
        inline AccSet   accSet() const;

        // For LInsSk.
        inline LIns*    prevLIns() const;

        // For LInsP.
        inline uint8_t  paramArg()  const;
        inline uint8_t  paramKind() const;

        // For LInsI.
        inline int32_t  immI() const;

        // For LInsQorD.
#ifdef NANOJIT_64BIT
        inline int32_t  immQlo() const;
        inline uint64_t immQ() const;
#endif
        inline int32_t  immDlo() const;
        inline int32_t  immDhi() const;
        inline double   immD() const;
        inline uint64_t immDasQ() const;

        // For LIR_allocp.
        inline int32_t  size()    const;
        inline void     setSize(int32_t nbytes);

        // For LInsC.
        inline LIns*    arg(uint32_t i)         const;  // right-to-left-order: arg(0) is rightmost
        inline uint32_t argc()                  const;
        inline LIns*    callArgN(uint32_t n)    const;
        inline const CallInfo* callInfo()       const;

        // For LIR_jtbl
        inline uint32_t getTableSize() const;
        inline LIns* getTarget(uint32_t index) const;
        inline void setTarget(uint32_t index, LIns* label) const;

        // isLInsXYZ() returns true if the instruction has the LInsXYZ form.
        // Note that there is some overlap with other predicates, eg.
        // isStore()==isLInsSt(), isCall()==isLInsC(), but that's ok;  these
        // ones are used mostly to check that opcodes are appropriate for
        // instruction layouts, the others are used for non-debugging
        // purposes.
        bool isLInsOp0() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_Op0 == repKinds[opcode()];
        }
        bool isLInsOp1() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_Op1 == repKinds[opcode()];
        }
        bool isLInsOp2() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_Op2 == repKinds[opcode()];
        }
        bool isLInsOp3() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_Op3 == repKinds[opcode()];
        }
        bool isLInsLd() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_Ld == repKinds[opcode()];
        }
        bool isLInsSt() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_St == repKinds[opcode()];
        }
        bool isLInsSk() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_Sk == repKinds[opcode()];
        }
        bool isLInsC() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_C == repKinds[opcode()];
        }
        bool isLInsP() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_P == repKinds[opcode()];
        }
        bool isLInsI() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_I == repKinds[opcode()];
        }
        bool isLInsQorD() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_QorD == repKinds[opcode()];
        }
        bool isLInsJtbl() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_Jtbl == repKinds[opcode()];
        }

        // LIns predicates.
        bool isop(LOpcode o) const {
            return opcode() == o;
        }
        bool isRet() const {
            return isRetOpcode(opcode());
        }
        bool isCmp() const {
            return isCmpOpcode(opcode());
        }
        bool isCall() const {
            return isop(LIR_callv) ||
                   isop(LIR_calli) ||
#if defined NANOJIT_64BIT
                   isop(LIR_callq) ||
#endif
                   isop(LIR_calld);
        }
        bool isCmov() const {
            return isCmovOpcode(opcode());
        }
        bool isStore() const {
            return isLInsSt();
        }
        bool isLoad() const {
            return isLInsLd();
        }
        bool isGuard() const {
            return isop(LIR_x) || isop(LIR_xf) || isop(LIR_xt) ||
                   isop(LIR_xbarrier) || isop(LIR_xtbl) ||
                   isop(LIR_addxovi) || isop(LIR_subxovi) || isop(LIR_mulxovi);
        }
        bool isJov() const {
            return
#ifdef NANOJIT_64BIT
                isop(LIR_addjovq) || isop(LIR_subjovq) ||
#endif
                isop(LIR_addjovi) || isop(LIR_subjovi) || isop(LIR_muljovi);
        }
        // True if the instruction is a 32-bit integer immediate.
        bool isImmI() const {
            return isop(LIR_immi);
        }
        // True if the instruction is a 32-bit integer immediate and
        // has the value 'val' when treated as a 32-bit signed integer.
        bool isImmI(int32_t val) const {
            return isImmI() && immI()==val;
        }
#ifdef NANOJIT_64BIT
        // True if the instruction is a 64-bit integer immediate.
        bool isImmQ() const {
            return isop(LIR_immq);
        }
#endif
        // True if the instruction is a pointer-sized integer immediate.
        bool isImmP() const
        {
#ifdef NANOJIT_64BIT
            return isImmQ();
#else
            return isImmI();
#endif
        }
        // True if the instruction is a 64-bit float immediate.
        bool isImmD() const {
            return isop(LIR_immd);
        }
        // True if the instruction is a 64-bit integer or float immediate.
        bool isImmQorD() const {
            return
#ifdef NANOJIT_64BIT
                isImmQ() ||
#endif
                isImmD();
        }
        // True if the instruction an any type of immediate.
        bool isImmAny() const {
            return isImmI() || isImmQorD();
        }

        bool isBranch() const {
            return isop(LIR_jt) || isop(LIR_jf) || isop(LIR_j) || isop(LIR_jtbl) || isJov();
        }

        LTy retType() const {
            return retTypes[opcode()];
        }
        bool isV() const {
            return retType() == LTy_V;
        }
        bool isI() const {
            return retType() == LTy_I;
        }
#ifdef NANOJIT_64BIT
        bool isQ() const {
            return retType() == LTy_Q;
        }
#endif
        bool isD() const {
            return retType() == LTy_D;
        }
        bool isQorD() const {
            return
#ifdef NANOJIT_64BIT
                isQ() ||
#endif
                isD();
        }
        bool isP() const {
#ifdef NANOJIT_64BIT
            return isQ();
#else
            return isI();
#endif
        }

        inline void* immP() const
        {
        #ifdef NANOJIT_64BIT
            return (void*)immQ();
        #else
            return (void*)immI();
        #endif
        }
    };

    typedef SeqBuilder<LIns*> InsList;
    typedef SeqBuilder<char*> StringList;


    // 0-operand form.  Used for LIR_start and LIR_label.
    class LInsOp0
    {
    private:
        friend class LIns;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // 1-operand form.  Used for LIR_reti, unary arithmetic/logic ops, etc.
    class LInsOp1
    {
    private:
        friend class LIns;

        LIns*       oprnd_1;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // 2-operand form.  Used for guards, branches, comparisons, binary
    // arithmetic/logic ops, etc.
    class LInsOp2
    {
    private:
        friend class LIns;

        LIns*       oprnd_2;

        LIns*       oprnd_1;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // 3-operand form.  Used for conditional moves, jov branches, and xov guards.
    class LInsOp3
    {
    private:
        friend class LIns;

        LIns*       oprnd_3;

        LIns*       oprnd_2;

        LIns*       oprnd_1;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for all loads.
    class LInsLd
    {
    private:
        friend class LIns;

        // Nb: the LIR writer pipeline handles things if a displacement
        // exceeds 16 bits.  This is rare, but does happen occasionally.  We
        // could go to 24 bits but then it would happen so rarely that the
        // handler code would be difficult to test and thus untrustworthy.
        //
        // Nb: the types of these bitfields are all 32-bit integers to ensure
        // they are fully packed on Windows, sigh.  Also, 'loadQual' is
        // unsigned to ensure the values 0, 1, and 2 all fit in 2 bits.
        //
        // Nb: explicit signed keyword for bitfield types is required,
        // some compilers may treat them as unsigned without it.
        // See Bugzilla 584219 comment #18
        signed int  disp:16;
        signed int  miniAccSetVal:8;
        uint32_t    loadQual:2;

        LIns*       oprnd_1;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for all stores.
    class LInsSt
    {
    private:
        friend class LIns;

        int16_t     disp;
        MiniAccSetVal miniAccSetVal;

        LIns*       oprnd_2;

        LIns*       oprnd_1;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for LIR_skip.
    class LInsSk
    {
    private:
        friend class LIns;

        LIns*       prevLIns;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for all variants of LIR_call.
    class LInsC
    {
    private:
        friend class LIns;

        // Arguments in reverse order, just like insCall() (ie. args[0] holds
        // the rightmost arg).  The array should be allocated by the same
        // allocator as the LIR buffers, because it has the same lifetime.
        LIns**      args;

        const CallInfo* ci;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for LIR_paramp.
    class LInsP
    {
    private:
        friend class LIns;

        uintptr_t   arg:8;
        uintptr_t   kind:8;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for LIR_immi and LIR_allocp.
    class LInsI
    {
    private:
        friend class LIns;

        int32_t     immI;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for LIR_immq and LIR_immd.
    class LInsQorD
    {
    private:
        friend class LIns;

        int32_t     immQorDlo;

        int32_t     immQorDhi;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for LIR_jtbl.  'oprnd_1' must be a uint32_t index in
    // the range 0 <= index < size; no range check is performed.
    // 'table' is an array of labels.
    class LInsJtbl
    {
    private:
        friend class LIns;

        uint32_t    size;     // number of entries in table
        LIns**      table;    // pointer to table[size] with same lifetime as this LInsJtbl
        LIns*       oprnd_1;  // uint32_t index expression

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; }
    };

    // Used only as a placeholder for OP___ macros for unused opcodes in
    // LIRopcode.tbl.
    class LInsNone
    {
    };

    LInsOp0*  LIns::toLInsOp0()  const { return (LInsOp0* )(uintptr_t(this+1) - sizeof(LInsOp0 )); }
    LInsOp1*  LIns::toLInsOp1()  const { return (LInsOp1* )(uintptr_t(this+1) - sizeof(LInsOp1 )); }
    LInsOp2*  LIns::toLInsOp2()  const { return (LInsOp2* )(uintptr_t(this+1) - sizeof(LInsOp2 )); }
    LInsOp3*  LIns::toLInsOp3()  const { return (LInsOp3* )(uintptr_t(this+1) - sizeof(LInsOp3 )); }
    LInsLd*   LIns::toLInsLd()   const { return (LInsLd*  )(uintptr_t(this+1) - sizeof(LInsLd  )); }
    LInsSt*   LIns::toLInsSt()   const { return (LInsSt*  )(uintptr_t(this+1) - sizeof(LInsSt  )); }
    LInsSk*   LIns::toLInsSk()   const { return (LInsSk*  )(uintptr_t(this+1) - sizeof(LInsSk  )); }
    LInsC*    LIns::toLInsC()    const { return (LInsC*   )(uintptr_t(this+1) - sizeof(LInsC   )); }
    LInsP*    LIns::toLInsP()    const { return (LInsP*   )(uintptr_t(this+1) - sizeof(LInsP   )); }
    LInsI*    LIns::toLInsI()    const { return (LInsI*   )(uintptr_t(this+1) - sizeof(LInsI   )); }
    LInsQorD* LIns::toLInsQorD() const { return (LInsQorD*)(uintptr_t(this+1) - sizeof(LInsQorD)); }
    LInsJtbl* LIns::toLInsJtbl() const { return (LInsJtbl*)(uintptr_t(this+1) - sizeof(LInsJtbl)); }

    void LIns::initLInsOp0(LOpcode opcode) {
        initSharedFields(opcode);
        NanoAssert(isLInsOp0());
    }
    void LIns::initLInsOp1(LOpcode opcode, LIns* oprnd1) {
        initSharedFields(opcode);
        toLInsOp1()->oprnd_1 = oprnd1;
        NanoAssert(isLInsOp1());
    }
    void LIns::initLInsOp2(LOpcode opcode, LIns* oprnd1, LIns* oprnd2) {
        initSharedFields(opcode);
        toLInsOp2()->oprnd_1 = oprnd1;
        toLInsOp2()->oprnd_2 = oprnd2;
        NanoAssert(isLInsOp2());
    }
    void LIns::initLInsOp3(LOpcode opcode, LIns* oprnd1, LIns* oprnd2, LIns* oprnd3) {
        initSharedFields(opcode);
        toLInsOp3()->oprnd_1 = oprnd1;
        toLInsOp3()->oprnd_2 = oprnd2;
        toLInsOp3()->oprnd_3 = oprnd3;
        NanoAssert(isLInsOp3());
    }
    void LIns::initLInsLd(LOpcode opcode, LIns* val, int32_t d, AccSet accSet, LoadQual loadQual) {
        initSharedFields(opcode);
        toLInsLd()->oprnd_1 = val;
        NanoAssert(d == int16_t(d));
        toLInsLd()->disp = int16_t(d);
        toLInsLd()->miniAccSetVal = compressAccSet(accSet).val;
        toLInsLd()->loadQual = loadQual;
        NanoAssert(isLInsLd());
    }
    void LIns::initLInsSt(LOpcode opcode, LIns* val, LIns* base, int32_t d, AccSet accSet) {
        initSharedFields(opcode);
        toLInsSt()->oprnd_1 = val;
        toLInsSt()->oprnd_2 = base;
        NanoAssert(d == int16_t(d));
        toLInsSt()->disp = int16_t(d);
        toLInsSt()->miniAccSetVal = compressAccSet(accSet).val;
        NanoAssert(isLInsSt());
    }
    void LIns::initLInsSk(LIns* prevLIns) {
        initSharedFields(LIR_skip);
        toLInsSk()->prevLIns = prevLIns;
        NanoAssert(isLInsSk());
    }
    void LIns::initLInsC(LOpcode opcode, LIns** args, const CallInfo* ci) {
        initSharedFields(opcode);
        toLInsC()->args = args;
        toLInsC()->ci = ci;
        NanoAssert(isLInsC());
    }
    void LIns::initLInsP(int32_t arg, int32_t kind) {
        initSharedFields(LIR_paramp);
        NanoAssert(isU8(arg) && isU8(kind));
        toLInsP()->arg = arg;
        toLInsP()->kind = kind;
        NanoAssert(isLInsP());
    }
    void LIns::initLInsI(LOpcode opcode, int32_t immI) {
        initSharedFields(opcode);
        toLInsI()->immI = immI;
        NanoAssert(isLInsI());
    }
    void LIns::initLInsQorD(LOpcode opcode, uint64_t immQorD) {
        initSharedFields(opcode);
        toLInsQorD()->immQorDlo = int32_t(immQorD);
        toLInsQorD()->immQorDhi = int32_t(immQorD >> 32);
        NanoAssert(isLInsQorD());
    }
    void LIns::initLInsJtbl(LIns* index, uint32_t size, LIns** table) {
        initSharedFields(LIR_jtbl);
        toLInsJtbl()->oprnd_1 = index;
        toLInsJtbl()->table = table;
        toLInsJtbl()->size = size;
        NanoAssert(isLInsJtbl());
    }

    LIns* LIns::oprnd1() const {
        NanoAssert(isLInsOp1() || isLInsOp2() || isLInsOp3() || isLInsLd() || isLInsSt() || isLInsJtbl());
        return toLInsOp2()->oprnd_1;
    }
    LIns* LIns::oprnd2() const {
        NanoAssert(isLInsOp2() || isLInsOp3() || isLInsSt());
        return toLInsOp2()->oprnd_2;
    }
    LIns* LIns::oprnd3() const {
        NanoAssert(isLInsOp3());
        return toLInsOp3()->oprnd_3;
    }

    LIns* LIns::getTarget() const {
        NanoAssert(isBranch() && !isop(LIR_jtbl));
        if (isJov())
            return oprnd3();
        else
            return oprnd2();
    }

    void LIns::setTarget(LIns* label) {
        NanoAssert(label && label->isop(LIR_label));
        NanoAssert(isBranch() && !isop(LIR_jtbl));
        if (isJov())
            toLInsOp3()->oprnd_3 = label;
        else
            toLInsOp2()->oprnd_2 = label;
    }

    LIns* LIns::getTarget(uint32_t index) const {
        NanoAssert(isop(LIR_jtbl));
        NanoAssert(index < toLInsJtbl()->size);
        return toLInsJtbl()->table[index];
    }

    void LIns::setTarget(uint32_t index, LIns* label) const {
        NanoAssert(label && label->isop(LIR_label));
        NanoAssert(isop(LIR_jtbl));
        NanoAssert(index < toLInsJtbl()->size);
        toLInsJtbl()->table[index] = label;
    }

    GuardRecord *LIns::record() const {
        NanoAssert(isGuard());
        switch (opcode()) {
        case LIR_x:
        case LIR_xt:
        case LIR_xf:
        case LIR_xtbl:
        case LIR_xbarrier:
            return (GuardRecord*)oprnd2();

        case LIR_addxovi:
        case LIR_subxovi:
        case LIR_mulxovi:
            return (GuardRecord*)oprnd3();

        default:
            NanoAssert(0);
            return NULL;
        }
    }

    LoadQual LIns::loadQual() const {
        NanoAssert(isLInsLd());
        return (LoadQual)toLInsLd()->loadQual;
    }

    int32_t LIns::disp() const {
        if (isLInsSt()) {
            return toLInsSt()->disp;
        } else {
            NanoAssert(isLInsLd());
            return toLInsLd()->disp;
        }
    }

    MiniAccSet LIns::miniAccSet() const {
        MiniAccSet miniAccSet;
        if (isLInsSt()) {
            miniAccSet.val = toLInsSt()->miniAccSetVal;
        } else {
            NanoAssert(isLInsLd());
            miniAccSet.val = toLInsLd()->miniAccSetVal;
        }
        return miniAccSet;
    }

    AccSet LIns::accSet() const {
        return decompressMiniAccSet(miniAccSet());
    }

    LIns* LIns::prevLIns() const {
        NanoAssert(isLInsSk());
        return toLInsSk()->prevLIns;
    }

    inline uint8_t LIns::paramArg()  const { NanoAssert(isop(LIR_paramp)); return toLInsP()->arg; }
    inline uint8_t LIns::paramKind() const { NanoAssert(isop(LIR_paramp)); return toLInsP()->kind; }

    inline int32_t LIns::immI()     const { NanoAssert(isImmI());  return toLInsI()->immI; }

#ifdef NANOJIT_64BIT
    inline int32_t LIns::immQlo()   const { NanoAssert(isImmQ()); return toLInsQorD()->immQorDlo; }
    uint64_t       LIns::immQ()     const {
        NanoAssert(isImmQ());
        return (uint64_t(toLInsQorD()->immQorDhi) << 32) | uint32_t(toLInsQorD()->immQorDlo);
    }
#endif
    inline int32_t LIns::immDlo() const { NanoAssert(isImmD()); return toLInsQorD()->immQorDlo; }
    inline int32_t LIns::immDhi() const { NanoAssert(isImmD()); return toLInsQorD()->immQorDhi; }
    double         LIns::immD()    const {
        NanoAssert(isImmD());
        union {
            double f;
            uint64_t q;
        } u;
        u.q = immDasQ();
        return u.f;
    }
    uint64_t       LIns::immDasQ()  const {
        NanoAssert(isImmD());
        return (uint64_t(toLInsQorD()->immQorDhi) << 32) | uint32_t(toLInsQorD()->immQorDlo);
    }

    int32_t LIns::size() const {
        NanoAssert(isop(LIR_allocp));
        return toLInsI()->immI << 2;
    }

    void LIns::setSize(int32_t nbytes) {
        NanoAssert(isop(LIR_allocp));
        NanoAssert(nbytes > 0);
        toLInsI()->immI = (nbytes+3)>>2; // # of required 32bit words
    }

    // Index args in reverse order, i.e. arg(0) returns the rightmost arg.
    // Nb: this must be kept in sync with insCall().
    LIns* LIns::arg(uint32_t i) const
    {
        NanoAssert(isCall());
        NanoAssert(i < callInfo()->count_args());
        return toLInsC()->args[i];  // args[] is in right-to-left order as well
    }

    uint32_t LIns::argc() const {
        return callInfo()->count_args();
    }

    LIns* LIns::callArgN(uint32_t n) const
    {
        return arg(argc()-n-1);
    }

    const CallInfo* LIns::callInfo() const
    {
        NanoAssert(isCall());
        return toLInsC()->ci;
    }

    uint32_t LIns::getTableSize() const
    {
        NanoAssert(isLInsJtbl());
        return toLInsJtbl()->size;
    }

    class LirWriter
    {
    public:
        LirWriter *out;

        LirWriter(LirWriter* out)
            : out(out) {}
        virtual ~LirWriter() {}

        virtual LIns* ins0(LOpcode v) {
            return out->ins0(v);
        }
        virtual LIns* ins1(LOpcode v, LIns* a) {
            return out->ins1(v, a);
        }
        virtual LIns* ins2(LOpcode v, LIns* a, LIns* b) {
            return out->ins2(v, a, b);
        }
        virtual LIns* ins3(LOpcode v, LIns* a, LIns* b, LIns* c) {
            return out->ins3(v, a, b, c);
        }
        virtual LIns* insGuard(LOpcode v, LIns *c, GuardRecord *gr) {
            return out->insGuard(v, c, gr);
        }
        virtual LIns* insGuardXov(LOpcode v, LIns *a, LIns* b, GuardRecord *gr) {
            return out->insGuardXov(v, a, b, gr);
        }
        virtual LIns* insBranch(LOpcode v, LIns* condition, LIns* to) {
            return out->insBranch(v, condition, to);
        }
        virtual LIns* insBranchJov(LOpcode v, LIns* a, LIns* b, LIns* to) {
            return out->insBranchJov(v, a, b, to);
        }
        // arg: 0=first, 1=second, ...
        // kind: 0=arg 1=saved-reg
        virtual LIns* insParam(int32_t arg, int32_t kind) {
            return out->insParam(arg, kind);
        }
        virtual LIns* insImmI(int32_t imm) {
            return out->insImmI(imm);
        }
#ifdef NANOJIT_64BIT
        virtual LIns* insImmQ(uint64_t imm) {
            return out->insImmQ(imm);
        }
#endif
        virtual LIns* insImmD(double d) {
            return out->insImmD(d);
        }
        virtual LIns* insLoad(LOpcode op, LIns* base, int32_t d, AccSet accSet, LoadQual loadQual) {
            return out->insLoad(op, base, d, accSet, loadQual);
        }
        virtual LIns* insStore(LOpcode op, LIns* value, LIns* base, int32_t d, AccSet accSet) {
            return out->insStore(op, value, base, d, accSet);
        }
        // args[] is in reverse order, ie. args[0] holds the rightmost arg.
        virtual LIns* insCall(const CallInfo *call, LIns* args[]) {
            return out->insCall(call, args);
        }
        virtual LIns* insAlloc(int32_t size) {
            NanoAssert(size != 0);
            return out->insAlloc(size);
        }
        virtual LIns* insJtbl(LIns* index, uint32_t size) {
            return out->insJtbl(index, size);
        }
        virtual LIns* insComment(const char* str) {
            return out->insComment(str);
        }

        // convenience functions

        // Inserts a conditional to execute and branches to execute if
        // the condition is true and false respectively.
        LIns* insChoose(LIns* cond, LIns* iftrue, LIns* iffalse, bool use_cmov);

        // Inserts an integer comparison to 0
        LIns* insEqI_0(LIns* oprnd1) {
            return ins2ImmI(LIR_eqi, oprnd1, 0);
        }

        // Inserts a pointer comparison to 0
        LIns* insEqP_0(LIns* oprnd1) {
            return ins2(LIR_eqp, oprnd1, insImmWord(0));
        }

        // Inserts a binary operation where the second operand is an
        // integer immediate.
        LIns* ins2ImmI(LOpcode v, LIns* oprnd1, int32_t imm) {
            return ins2(v, oprnd1, insImmI(imm));
        }

        LIns* insImmP(const void *ptr) {
#ifdef NANOJIT_64BIT
            return insImmQ((uint64_t)ptr);
#else
            return insImmI((int32_t)ptr);
#endif
        }

        LIns* insImmWord(intptr_t value) {
#ifdef NANOJIT_64BIT
            return insImmQ(value);
#else
            return insImmI(value);
#endif
        }

        // Sign-extend integers to native integers. On 32-bit this is a no-op.
        LIns* insI2P(LIns* intIns) {
#ifdef NANOJIT_64BIT
            return ins1(LIR_i2q, intIns);
#else
            return intIns;
#endif
        }

        // Zero-extend integers to native integers. On 32-bit this is a no-op.
        LIns* insUI2P(LIns* uintIns) {
    #ifdef NANOJIT_64BIT
            return ins1(LIR_ui2uq, uintIns);
    #else
            return uintIns;
    #endif
        }

        // Do a load with LoadQual==LOAD_NORMAL.
        LIns* insLoad(LOpcode op, LIns* base, int32_t d, AccSet accSet) {
            return insLoad(op, base, d, accSet, LOAD_NORMAL);
        }

        // Chooses LIR_sti, LIR_stq or LIR_std according to the type of 'value'.
        LIns* insStore(LIns* value, LIns* base, int32_t d, AccSet accSet);
    };


#ifdef NJ_VERBOSE
    extern const char* lirNames[];

    // Maps address ranges to meaningful names.
    class AddrNameMap
    {
        Allocator& allocator;
        class Entry
        {
        public:
            Entry(int) : name(0), size(0), align(0) {}
            Entry(char *n, size_t s, size_t a) : name(n), size(s), align(a) {}
            char* name;
            size_t size:29, align:3;
        };
        TreeMap<const void*, Entry*> names;     // maps code regions to names
    public:
        AddrNameMap(Allocator& allocator);
        void addAddrRange(const void *p, size_t size, size_t align, const char *name);
        void lookupAddr(void *p, char*& name, int32_t& offset);
    };

    // Maps LIR instructions to meaningful names.
    class LirNameMap
    {
    private:
        Allocator& alloc;

        // A small string-wrapper class, required because we need '==' to
        // compare string contents, not string pointers, when strings are used
        // as keys in CountMap.
        struct Str {
            Allocator& alloc;
            char* s;

            Str(Allocator& alloc_, const char* s_) : alloc(alloc_) {
                s = new (alloc) char[1+strlen(s_)];
                strcpy(s, s_);
            }

            bool operator==(const Str& str) const {
                return (0 == strcmp(this->s, str.s));
            }
        };

        // Similar to 'struct Str' -- we need to hash the string's contents,
        // not its pointer.
        template<class K> struct StrHash {
            static size_t hash(const Str &k) {
                // (const void*) cast is required by ARM RVCT 2.2
                return murmurhash((const void*)k.s, strlen(k.s));
            }
        };

        template <class Key, class H=DefaultHash<Key> >
        class CountMap: public HashMap<Key, int, H> {
        public:
            CountMap(Allocator& alloc) : HashMap<Key, int, H>(alloc, 128) {}
            int add(Key k) {
                int c = 1;
                if (this->containsKey(k)) {
                    c = 1+this->get(k);
                }
                this->put(k,c);
                return c;
            }
        };

        CountMap<int> lircounts;
        CountMap<const CallInfo *> funccounts;
        CountMap<Str, StrHash<Str> > namecounts;

        void addNameWithSuffix(LIns* i, const char *s, int suffix, bool ignoreOneSuffix);

        class Entry
        {
        public:
            Entry(int) : name(0) {}
            Entry(char* n) : name(n) {}
            char* name;
        };

        HashMap<LIns*, Entry*> names;

    public:
        LirNameMap(Allocator& alloc)
            : alloc(alloc),
            lircounts(alloc),
            funccounts(alloc),
            namecounts(alloc),
            names(alloc)
        {}

        void        addName(LIns* ins, const char *s);  // gives 'ins' a special name
        const char* createName(LIns* ins);              // gives 'ins' a generic name
        const char* lookupName(LIns* ins);
    };

    // We use big buffers for cases where we need to fit a whole instruction,
    // and smaller buffers for all the others.  These should easily be long
    // enough, but for safety the formatXyz() functions check and won't exceed
    // those limits.
    class InsBuf {
    public:
        static const size_t len = 1000;
        char buf[len];
    };
    class RefBuf {
    public:
        static const size_t len = 200;
        char buf[len];
    };

    class LInsPrinter
    {
    private:
        Allocator& alloc;
        const int EMB_NUM_USED_ACCS;

        char *formatImmI(RefBuf* buf, int32_t c);
#ifdef NANOJIT_64BIT
        char *formatImmQ(RefBuf* buf, uint64_t c);
#endif
        char *formatImmD(RefBuf* buf, double c);
        void formatGuard(InsBuf* buf, LIns* ins);       // defined by the embedder
        void formatGuardXov(InsBuf* buf, LIns* ins);    // defined by the embedder
        static const char* accNames[];                  // defined by the embedder

    public:

        LInsPrinter(Allocator& alloc, int embNumUsedAccs)
            : alloc(alloc), EMB_NUM_USED_ACCS(embNumUsedAccs)
        {
            addrNameMap = new (alloc) AddrNameMap(alloc);
            lirNameMap = new (alloc) LirNameMap(alloc);
        }

        char *formatAddr(RefBuf* buf, void* p);
        char *formatRef(RefBuf* buf, LIns* ref, bool showImmValue = true);
        char *formatIns(InsBuf* buf, LIns* ins);
        char *formatAccSet(RefBuf* buf, AccSet accSet);

        AddrNameMap* addrNameMap;
        LirNameMap* lirNameMap;
    };


    class VerboseWriter : public LirWriter
    {
        InsList code;
        LInsPrinter* printer;
        LogControl* logc;
        const char* const prefix;
        bool const always_flush;
    public:
        VerboseWriter(Allocator& alloc, LirWriter *out, LInsPrinter* printer, LogControl* logc,
                      const char* prefix = "", bool always_flush = false)
            : LirWriter(out), code(alloc), printer(printer), logc(logc), prefix(prefix), always_flush(always_flush)
        {}

        LIns* add(LIns* i) {
            if (i) {
                code.add(i);
                if (always_flush)
                    flush();
            }
            return i;
        }

        LIns* add_flush(LIns* i) {
            if ((i = add(i)) != 0)
                flush();
            return i;
        }

        void flush()
        {
            if (!code.isEmpty()) {
                InsBuf b;
                for (Seq<LIns*>* p = code.get(); p != NULL; p = p->tail)
                    logc->printf("%s    %s\n", prefix, printer->formatIns(&b, p->head));
                code.clear();
            }
        }

        LIns* insGuard(LOpcode op, LIns* cond, GuardRecord *gr) {
            return add_flush(out->insGuard(op,cond,gr));
        }

        LIns* insGuardXov(LOpcode op, LIns* a, LIns* b, GuardRecord *gr) {
            return add(out->insGuardXov(op,a,b,gr));
        }

        LIns* insBranch(LOpcode v, LIns* condition, LIns* to) {
            return add_flush(out->insBranch(v, condition, to));
        }

        LIns* insBranchJov(LOpcode v, LIns* a, LIns* b, LIns* to) {
            return add(out->insBranchJov(v, a, b, to));
        }

        LIns* insJtbl(LIns* index, uint32_t size) {
            return add_flush(out->insJtbl(index, size));
        }

        LIns* ins0(LOpcode v) {
            if (v == LIR_label || v == LIR_start) {
                flush();
            }
            return add(out->ins0(v));
        }

        LIns* ins1(LOpcode v, LIns* a) {
            return isRetOpcode(v) ? add_flush(out->ins1(v, a)) : add(out->ins1(v, a));
        }
        LIns* ins2(LOpcode v, LIns* a, LIns* b) {
            return add(out->ins2(v, a, b));
        }
        LIns* ins3(LOpcode v, LIns* a, LIns* b, LIns* c) {
            return add(out->ins3(v, a, b, c));
        }
        LIns* insCall(const CallInfo *call, LIns* args[]) {
            return add_flush(out->insCall(call, args));
        }
        LIns* insParam(int32_t i, int32_t kind) {
            return add(out->insParam(i, kind));
        }
        LIns* insLoad(LOpcode v, LIns* base, int32_t disp, AccSet accSet, LoadQual loadQual) {
            return add(out->insLoad(v, base, disp, accSet, loadQual));
        }
        LIns* insStore(LOpcode op, LIns* v, LIns* b, int32_t d, AccSet accSet) {
            return add_flush(out->insStore(op, v, b, d, accSet));
        }
        LIns* insAlloc(int32_t size) {
            return add(out->insAlloc(size));
        }
        LIns* insImmI(int32_t imm) {
            return add(out->insImmI(imm));
        }
#ifdef NANOJIT_64BIT
        LIns* insImmQ(uint64_t imm) {
            return add(out->insImmQ(imm));
        }
#endif
        LIns* insImmD(double d) {
            return add(out->insImmD(d));
        }

        LIns* insComment(const char* str) {
            return add_flush(out->insComment(str));
        }
    };

#endif

    class ExprFilter: public LirWriter
    {
    public:
        ExprFilter(LirWriter *out) : LirWriter(out) {}
        LIns* ins1(LOpcode v, LIns* a);
        LIns* ins2(LOpcode v, LIns* a, LIns* b);
        LIns* ins3(LOpcode v, LIns* a, LIns* b, LIns* c);
        LIns* insGuard(LOpcode, LIns* cond, GuardRecord *);
        LIns* insGuardXov(LOpcode, LIns* a, LIns* b, GuardRecord *);
        LIns* insBranch(LOpcode, LIns* cond, LIns* target);
        LIns* insBranchJov(LOpcode, LIns* a, LIns* b, LIns* target);
        LIns* insLoad(LOpcode op, LIns* base, int32_t off, AccSet accSet, LoadQual loadQual);
    private:
        LIns* simplifyOverflowArith(LOpcode op, LIns** opnd1, LIns** opnd2);
    };

    class CseFilter: public LirWriter
    {
        enum NLKind {
            // We divide instruction kinds into groups.  LIns0 isn't present
            // because we don't need to record any 0-ary instructions.  Loads
            // aren't here, they're handled separately.
            LInsImmI = 0,
            LInsImmQ = 1,   // only occurs on 64-bit platforms
            LInsImmD = 2,
            LIns1    = 3,
            LIns2    = 4,
            LIns3    = 5,
            LInsCall = 6,

            LInsFirst = 0,
            LInsLast = 6,
            // Need a value after "last" to outsmart compilers that insist last+1 is impossible.
            LInsInvalid = 7
        };
        #define nextNLKind(kind)  NLKind(kind+1)

        // There is one table for each NLKind.  This lets us size the lists
        // appropriately (some instruction kinds are more common than others).
        // It also lets us have NLKind-specific find/add/grow functions, which
        // are faster than generic versions.
        //
        // Nb: m_listNL and m_capNL sizes must be a power of 2.
        //     Don't start m_capNL too small, or we'll waste time growing and rehashing.
        //     Don't start m_capNL too large, will waste memory.
        //
        LIns**      m_listNL[LInsLast + 1];
        uint32_t    m_capNL[ LInsLast + 1];
        uint32_t    m_usedNL[LInsLast + 1];
        typedef uint32_t (CseFilter::*find_t)(LIns*);
        find_t      m_findNL[LInsLast + 1];

        // Similarly, for loads, there is one table for each CseAcc.  A CseAcc
        // is like a normal access region, but there are two extra possible
        // values: CSE_ACC_CONST, which is where we put all CONST-qualified
        // loads, and CSE_ACC_MULTIPLE, where we put all multi-region loads.
        // All remaining loads are single-region and go in the table entry for
        // their region.
        //
        // This arrangement makes the removal of invalidated loads fast -- we
        // can invalidate all loads from a single region by clearing that
        // region's table.
        //
        typedef uint8_t CseAcc;     // same type as MiniAccSet

        static const uint8_t CSE_NUM_ACCS = NUM_ACCS + 2;

        // These values would be 'static const' except they are defined in
        // terms of EMB_NUM_USED_ACCS which is itself not 'static const'
        // because it's passed in by the embedding.
        const uint8_t EMB_NUM_USED_ACCS;      // number of access regions used by the embedding
        const uint8_t CSE_NUM_USED_ACCS;      // EMB_NUM_USED_ACCS + 2
        const CseAcc CSE_ACC_CONST;           // EMB_NUM_USED_ACCS + 0
        const CseAcc CSE_ACC_MULTIPLE;        // EMB_NUM_USED_ACCS + 1

        // We will only use CSE_NUM_USED_ACCS of these entries, ie. the
        // number of lists allocated depends on the number of access regions
        // in use by the embedding.
        LIns**      m_listL[CSE_NUM_ACCS];
        uint32_t    m_capL[ CSE_NUM_ACCS];
        uint32_t    m_usedL[CSE_NUM_ACCS];

        AccSet      storesSinceLastLoad;    // regions stored to since the last load

        Allocator& alloc;

        // After a conditional guard such as "xf cmp", we know that 'cmp' must
        // be true, else we would have side-exited.  So if we see 'cmp' again
        // we can treat it like a constant.  This table records such
        // comparisons.
        HashMap <LIns*, bool> knownCmpValues;

        // If true, we will not add new instructions to the CSE tables, but we
        // will continue to CSE instructions that match existing table
        // entries.  Load instructions will still be removed if aliasing
        // stores are encountered.
        bool suspended;

        CseAcc miniAccSetToCseAcc(MiniAccSet miniAccSet, LoadQual loadQual) {
            NanoAssert(miniAccSet.val < NUM_ACCS || miniAccSet.val == MINI_ACCSET_MULTIPLE.val);
            return (loadQual == LOAD_CONST) ? CSE_ACC_CONST :
                   (miniAccSet.val == MINI_ACCSET_MULTIPLE.val) ? CSE_ACC_MULTIPLE :
                   miniAccSet.val;
        }

        static uint32_t hash8(uint32_t hash, const uint8_t data);
        static uint32_t hash32(uint32_t hash, const uint32_t data);
        static uint32_t hashptr(uint32_t hash, const void* data);
        static uint32_t hashfinish(uint32_t hash);

        static uint32_t hashImmI(int32_t);
        static uint32_t hashImmQorD(uint64_t);     // not NANOJIT_64BIT-only -- used by findImmD()
        static uint32_t hash1(LOpcode op, LIns*);
        static uint32_t hash2(LOpcode op, LIns*, LIns*);
        static uint32_t hash3(LOpcode op, LIns*, LIns*, LIns*);
        static uint32_t hashLoad(LOpcode op, LIns*, int32_t);
        static uint32_t hashCall(const CallInfo *call, uint32_t argc, LIns* args[]);

        // These versions are used before an LIns has been created.
        LIns* findImmI(int32_t a, uint32_t &k);
#ifdef NANOJIT_64BIT
        LIns* findImmQ(uint64_t a, uint32_t &k);
#endif
        LIns* findImmD(uint64_t d, uint32_t &k);
        LIns* find1(LOpcode v, LIns* a, uint32_t &k);
        LIns* find2(LOpcode v, LIns* a, LIns* b, uint32_t &k);
        LIns* find3(LOpcode v, LIns* a, LIns* b, LIns* c, uint32_t &k);
        LIns* findLoad(LOpcode v, LIns* a, int32_t b, MiniAccSet miniAccSet, LoadQual loadQual,
                       uint32_t &k);
        LIns* findCall(const CallInfo *call, uint32_t argc, LIns* args[], uint32_t &k);

        // These versions are used after an LIns has been created; they are
        // used for rehashing after growing.  They just call onto the
        // multi-arg versions above.
        uint32_t findImmI(LIns* ins);
#ifdef NANOJIT_64BIT
        uint32_t findImmQ(LIns* ins);
#endif
        uint32_t findImmD(LIns* ins);
        uint32_t find1(LIns* ins);
        uint32_t find2(LIns* ins);
        uint32_t find3(LIns* ins);
        uint32_t findCall(LIns* ins);
        uint32_t findLoad(LIns* ins);

        void growNL(NLKind kind);
        void growL(CseAcc cseAcc);

        // 'k' is the index found by findXYZ().
        void addNL(NLKind kind, LIns* ins, uint32_t k);
        void addL(LIns* ins, uint32_t k);

        void clearAll();            // clears all tables
        void clearNL(NLKind);       // clears one non-load table
        void clearL(CseAcc);        // clears one load table

    public:
        CseFilter(LirWriter *out, uint8_t embNumUsedAccs, Allocator&);

        LIns* insImmI(int32_t imm);
#ifdef NANOJIT_64BIT
        LIns* insImmQ(uint64_t q);
#endif
        LIns* insImmD(double d);
        LIns* ins0(LOpcode v);
        LIns* ins1(LOpcode v, LIns*);
        LIns* ins2(LOpcode v, LIns*, LIns*);
        LIns* ins3(LOpcode v, LIns*, LIns*, LIns*);
        LIns* insLoad(LOpcode op, LIns* base, int32_t d, AccSet accSet, LoadQual loadQual);
        LIns* insStore(LOpcode op, LIns* value, LIns* base, int32_t d, AccSet accSet);
        LIns* insCall(const CallInfo *call, LIns* args[]);
        LIns* insGuard(LOpcode op, LIns* cond, GuardRecord *gr);
        LIns* insGuardXov(LOpcode op, LIns* a, LIns* b, GuardRecord *gr);

        // These functions provide control over CSE in the face of control
        // flow.  A suspend()/resume() pair may be put around a synthetic
        // control flow diamond, preventing the inserted label from resetting
        // the CSE state.  A suspend() call must be dominated by a resume()
        // call, else incorrect code could result.
        void suspend() { suspended = true; }
        void resume() { suspended = false; }
    };

    class LirBuffer
    {
        public:
            LirBuffer(Allocator& alloc);
            void        clear();
            uintptr_t   makeRoom(size_t szB);   // make room for an instruction

            debug_only (void validate() const;)
            verbose_only(LInsPrinter* printer;)

            int32_t insCount();
            size_t  byteCount();

            // stats
            struct
            {
                uint32_t lir;    // # instructions
            }
            _stats;

            AbiKind abi;
            LIns *state, *param1, *sp, *rp;
            LIns* savedRegs[NumSavedRegs+1]; // Allocate an extra element in case NumSavedRegs == 0

        protected:
            friend class LirBufWriter;

            /** Each chunk is just a raw area of LIns instances, with no header
                and no more than 8-byte alignment.  The chunk size is somewhat arbitrary. */
            static const size_t CHUNK_SZB = 8000;

            /** Get CHUNK_SZB more memory for LIR instructions. */
            void        chunkAlloc();
            void        moveToNewChunk(uintptr_t addrOfLastLInsOnCurrentChunk);

            Allocator&  _allocator;
            uintptr_t   _unused;   // next unused instruction slot in the current LIR chunk
            uintptr_t   _limit;    // one past the last usable byte of the current LIR chunk
            size_t      _bytesAllocated;
    };

    class LirBufWriter : public LirWriter
    {
        LirBuffer*              _buf;        // underlying buffer housing the instructions
        const Config&           _config;

        public:
            LirBufWriter(LirBuffer* buf, const Config& config)
                : LirWriter(0), _buf(buf), _config(config) {
            }

            // LirWriter interface
            LIns*   insLoad(LOpcode op, LIns* base, int32_t disp, AccSet accSet, LoadQual loadQual);
            LIns*   insStore(LOpcode op, LIns* o1, LIns* o2, int32_t disp, AccSet accSet);
            LIns*   ins0(LOpcode op);
            LIns*   ins1(LOpcode op, LIns* o1);
            LIns*   ins2(LOpcode op, LIns* o1, LIns* o2);
            LIns*   ins3(LOpcode op, LIns* o1, LIns* o2, LIns* o3);
            LIns*   insParam(int32_t i, int32_t kind);
            LIns*   insImmI(int32_t imm);
#ifdef NANOJIT_64BIT
            LIns*   insImmQ(uint64_t imm);
#endif
            LIns*   insImmD(double d);
            LIns*   insCall(const CallInfo *call, LIns* args[]);
            LIns*   insGuard(LOpcode op, LIns* cond, GuardRecord *gr);
            LIns*   insGuardXov(LOpcode op, LIns* a, LIns* b, GuardRecord *gr);
            LIns*   insBranch(LOpcode v, LIns* condition, LIns* to);
            LIns*   insBranchJov(LOpcode v, LIns* a, LIns* b, LIns* to);
            LIns*   insAlloc(int32_t size);
            LIns*   insJtbl(LIns* index, uint32_t size);
            LIns*   insComment(const char* str);
    };

    class LirFilter
    {
    public:
        LirFilter *in;
        LirFilter(LirFilter *in) : in(in) {}
        virtual ~LirFilter(){}

        // It's crucial that once this reaches the LIR_start at the beginning
        // of the buffer, that it just keeps returning that LIR_start LIns on
        // any subsequent calls.
        virtual LIns* read() {
            return in->read();
        }
        virtual LIns* finalIns() {
            return in->finalIns();
        }
    };

    // concrete
    class LirReader : public LirFilter
    {
        LIns* _ins;         // next instruction to be read;  invariant: is never a skip
        LIns* _finalIns;    // final instruction in the stream;  ie. the first one to be read

    public:
        LirReader(LIns* ins) : LirFilter(0), _ins(ins), _finalIns(ins)
        {
            // The last instruction for a fragment shouldn't be a skip.
            // (Actually, if the last *inserted* instruction exactly fills up
            // a chunk, a new chunk will be created, and thus the last *written*
            // instruction will be a skip -- the one needed for the
            // cross-chunk link.  But the last *inserted* instruction is what
            // is recorded and used to initialise each LirReader, and that is
            // what is seen here, and therefore this assertion holds.)
            NanoAssert(ins && !ins->isop(LIR_skip));
        }
        virtual ~LirReader() {}

        // Returns next instruction and advances to the prior instruction.
        // Invariant: never returns a skip.
        LIns* read();

        LIns* finalIns() {
            return _finalIns;
        }
    };

    verbose_only(void live(LirFilter* in, Allocator& alloc, Fragment* frag, LogControl*);)

    // WARNING: StackFilter assumes that all stack entries are eight bytes.
    // Some of its optimisations aren't valid if that isn't true.  See
    // StackFilter::read() for more details.
    class StackFilter: public LirFilter
    {
        LIns* sp;
        BitSet stk;
        int top;
        int getTop(LIns* br);

    public:
        StackFilter(LirFilter *in, Allocator& alloc, LIns* sp);
        LIns* read();
    };

    // This type is used to perform a simple interval analysis of 32-bit
    // add/sub/mul.  It lets us avoid overflow checks in some cases.
    struct Interval
    {
        // The bounds are 64-bit integers so that any overflow from a 32-bit
        // operation can be safely detected.
        //
        // If 'hasOverflowed' is false, 'lo' and 'hi' must be in the range
        // I32_MIN..I32_MAX.  If 'hasOverflowed' is true, 'lo' and 'hi' should
        // not be trusted (and in debug builds we set them both to a special
        // value UNTRUSTWORTHY that is outside the I32_MIN..I32_MAX range to
        // facilitate sanity checking).
        //
        int64_t lo;
        int64_t hi;
        bool hasOverflowed;

        static const int64_t I32_MIN = int64_t(int32_t(0x80000000));
        static const int64_t I32_MAX = int64_t(int32_t(0x7fffffff));

#ifdef DEBUG
        static const int64_t UNTRUSTWORTHY = int64_t(0xdeafdeadbeeffeedLL);

        bool isSane() {
            return (hasOverflowed && lo == UNTRUSTWORTHY && hi == UNTRUSTWORTHY) ||
                   (!hasOverflowed && lo <= hi && I32_MIN <= lo && hi <= I32_MAX);
        }
#endif

        Interval(int64_t lo_, int64_t hi_) {
            if (lo_ < I32_MIN || I32_MAX < hi_) {
                hasOverflowed = true;
#ifdef DEBUG
                lo = UNTRUSTWORTHY;
                hi = UNTRUSTWORTHY;
#endif
            } else {
                hasOverflowed = false;
                lo = lo_;
                hi = hi_;
            }
            NanoAssert(isSane());
        }

        static Interval OverflowInterval() {
            Interval interval(0, 0);
#ifdef DEBUG
            interval.lo = UNTRUSTWORTHY;
            interval.hi = UNTRUSTWORTHY;
#endif
            interval.hasOverflowed = true;
            return interval;
        }

        static Interval of(LIns* ins, int32_t lim);

        static Interval add(Interval x, Interval y);
        static Interval sub(Interval x, Interval y);
        static Interval mul(Interval x, Interval y);

        bool canBeZero() {
            NanoAssert(isSane());
            return hasOverflowed || (lo <= 0 && 0 <= hi);
        }

        bool canBeNegative() {
            NanoAssert(isSane());
            return hasOverflowed || (lo < 0);
        }
    };

#if NJ_SOFTFLOAT_SUPPORTED
    struct SoftFloatOps
    {
        const CallInfo* opmap[LIR_sentinel];
        SoftFloatOps();
    };

    extern const SoftFloatOps softFloatOps;

    // Replaces fpu ops with function calls, for platforms lacking float
    // hardware (eg. some ARM machines).
    class SoftFloatFilter: public LirWriter
    {
    public:
        static const CallInfo* opmap[LIR_sentinel];

        SoftFloatFilter(LirWriter *out);
        LIns *split(LIns *a);
        LIns *split(const CallInfo *call, LIns* args[]);
        LIns *callD1(const CallInfo *call, LIns *a);
        LIns *callD2(const CallInfo *call, LIns *a, LIns *b);
        LIns *cmpD(const CallInfo *call, LIns *a, LIns *b);
        LIns *ins1(LOpcode op, LIns *a);
        LIns *ins2(LOpcode op, LIns *a, LIns *b);
        LIns *insCall(const CallInfo *ci, LIns* args[]);
    };
#endif

#ifdef DEBUG
    // This class does thorough checking of LIR.  It checks *implicit* LIR
    // instructions, ie. LIR instructions specified via arguments -- to
    // methods like insLoad() -- that have not yet been converted into
    // *explicit* LIns objects in a LirBuffer.  The reason for this is that if
    // we wait until the LIR instructions are explicit, they will have gone
    // through the entire writer pipeline and been optimised.  By checking
    // implicit LIR instructions we can check the LIR code at the start of the
    // writer pipeline, exactly as it is generated by the compiler front-end.
    //
    // A general note about the errors produced by this class:  for
    // TraceMonkey, they won't include special names for instructions that
    // have them unless TMFLAGS is specified.
    class ValidateWriter : public LirWriter
    {
    private:
        LInsPrinter* printer;
        const char* whereInPipeline;

        const char* type2string(LTy type);
        void typeCheckArgs(LOpcode op, int nArgs, LTy formals[], LIns* args[]);
        void errorStructureShouldBe(LOpcode op, const char* argDesc, int argN, LIns* arg,
                                    const char* shouldBeDesc);
        void errorAccSet(const char* what, AccSet accSet, const char* shouldDesc);
        void errorLoadQual(const char* what, LoadQual loadQual);
        void checkLInsHasOpcode(LOpcode op, int argN, LIns* ins, LOpcode op2);
        void checkLInsIsACondOrConst(LOpcode op, int argN, LIns* ins);
        void checkLInsIsNull(LOpcode op, int argN, LIns* ins);
        void checkAccSet(LOpcode op, LIns* base, int32_t disp, AccSet accSet);   // defined by the embedder

        // These can be set by the embedder and used in checkAccSet().
        void** checkAccSetExtras;

    public:
        ValidateWriter(LirWriter* out, LInsPrinter* printer, const char* where);
        void setCheckAccSetExtras(void** extras) { checkAccSetExtras = extras; }

        LIns* insLoad(LOpcode op, LIns* base, int32_t d, AccSet accSet, LoadQual loadQual);
        LIns* insStore(LOpcode op, LIns* value, LIns* base, int32_t d, AccSet accSet);
        LIns* ins0(LOpcode v);
        LIns* ins1(LOpcode v, LIns* a);
        LIns* ins2(LOpcode v, LIns* a, LIns* b);
        LIns* ins3(LOpcode v, LIns* a, LIns* b, LIns* c);
        LIns* insParam(int32_t arg, int32_t kind);
        LIns* insImmI(int32_t imm);
#ifdef NANOJIT_64BIT
        LIns* insImmQ(uint64_t imm);
#endif
        LIns* insImmD(double d);
        LIns* insCall(const CallInfo *call, LIns* args[]);
        LIns* insGuard(LOpcode v, LIns *c, GuardRecord *gr);
        LIns* insGuardXov(LOpcode v, LIns* a, LIns* b, GuardRecord* gr);
        LIns* insBranch(LOpcode v, LIns* condition, LIns* to);
        LIns* insBranchJov(LOpcode v, LIns* a, LIns* b, LIns* to);
        LIns* insAlloc(int32_t size);
        LIns* insJtbl(LIns* index, uint32_t size);
    };

    // This just checks things that aren't possible to check in
    // ValidateWriter, eg. whether all branch targets are set and are labels.
    class ValidateReader: public LirFilter {
    public:
        ValidateReader(LirFilter* in);
        LIns* read();
    };
#endif

#ifdef NJ_VERBOSE
    /* A listing filter for LIR, going through backwards.  It merely
       passes its input to its output, but notes it down too.  When
       finish() is called, prints out what went through.  Is intended to be
       used to print arbitrary intermediate transformation stages of
       LIR. */
    class ReverseLister : public LirFilter
    {
        Allocator&   _alloc;
        LInsPrinter* _printer;
        const char*  _title;
        StringList   _strs;
        LogControl*  _logc;
        LIns*        _prevIns;
    public:
        ReverseLister(LirFilter* in, Allocator& alloc,
                      LInsPrinter* printer, LogControl* logc, const char* title)
            : LirFilter(in)
            , _alloc(alloc)
            , _printer(printer)
            , _title(title)
            , _strs(alloc)
            , _logc(logc)
            , _prevIns(NULL)
        { }

        void finish();
        LIns* read();
    };
#endif

}
#endif // __nanojit_LIR__
