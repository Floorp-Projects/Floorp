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
#define OP___(op, number, repKind, retType) \
        LIR_##op = (number),
#include "LIRopcode.tbl"
        LIR_sentinel,
#undef OP___

#ifdef NANOJIT_64BIT
#  define PTR_SIZE(a,b)  b
#else
#  define PTR_SIZE(a,b)  a
#endif

        // pointer op aliases
        LIR_ldp     = PTR_SIZE(LIR_ld,     LIR_ldq),
        LIR_ldcp    = PTR_SIZE(LIR_ldc,    LIR_ldqc),
        LIR_stpi    = PTR_SIZE(LIR_sti,    LIR_stqi),
        LIR_piadd   = PTR_SIZE(LIR_add,    LIR_qiadd),
        LIR_piand   = PTR_SIZE(LIR_and,    LIR_qiand),
        LIR_pilsh   = PTR_SIZE(LIR_lsh,    LIR_qilsh),
        LIR_pirsh   = PTR_SIZE(LIR_rsh,    LIR_qirsh),
        LIR_pursh   = PTR_SIZE(LIR_ush,    LIR_qursh),
        LIR_pcmov   = PTR_SIZE(LIR_cmov,   LIR_qcmov),
        LIR_pior    = PTR_SIZE(LIR_or,     LIR_qior),
        LIR_pxor    = PTR_SIZE(LIR_xor,    LIR_qxor),
        LIR_addp    = PTR_SIZE(LIR_iaddp,  LIR_qaddp),
        LIR_peq     = PTR_SIZE(LIR_eq,     LIR_qeq),
        LIR_plt     = PTR_SIZE(LIR_lt,     LIR_qlt),
        LIR_pgt     = PTR_SIZE(LIR_gt,     LIR_qgt),
        LIR_ple     = PTR_SIZE(LIR_le,     LIR_qle),
        LIR_pge     = PTR_SIZE(LIR_ge,     LIR_qge),
        LIR_pult    = PTR_SIZE(LIR_ult,    LIR_qult),
        LIR_pugt    = PTR_SIZE(LIR_ugt,    LIR_qugt),
        LIR_pule    = PTR_SIZE(LIR_ule,    LIR_qule),
        LIR_puge    = PTR_SIZE(LIR_uge,    LIR_quge),
        LIR_alloc   = PTR_SIZE(LIR_ialloc, LIR_qalloc),
        LIR_pcall   = PTR_SIZE(LIR_icall,  LIR_qcall),
        LIR_param   = PTR_SIZE(LIR_iparam, LIR_qparam),
        LIR_plive   = PTR_SIZE(LIR_live,   LIR_qlive),
        LIR_pret    = PTR_SIZE(LIR_ret,    LIR_qret)
    };

    struct GuardRecord;
    struct SideExit;

    enum AbiKind {
        ABI_FASTCALL,
        ABI_THISCALL,
        ABI_STDCALL,
        ABI_CDECL
    };

    enum ArgSize {
        ARGSIZE_NONE = 0,
        ARGSIZE_F = 1,      // double (64bit)
        ARGSIZE_I = 2,      // int32_t
#ifdef NANOJIT_64BIT
        ARGSIZE_Q = 3,      // uint64_t
#endif
        ARGSIZE_U = 6,      // uint32_t
        ARGSIZE_MASK_ANY = 7,
        ARGSIZE_MASK_INT = 2,
        ARGSIZE_SHIFT = 3,

        // aliases
        ARGSIZE_P = PTR_SIZE(ARGSIZE_I, ARGSIZE_Q), // pointer
        ARGSIZE_LO = ARGSIZE_I, // int32_t
        ARGSIZE_B = ARGSIZE_I, // bool
        ARGSIZE_V = ARGSIZE_NONE  // void
    };

    enum IndirectCall {
        CALL_INDIRECT = 0
    };

    struct CallInfo
    {
        uintptr_t   _address;
        uint32_t    _argtypes:27;    // 9 3-bit fields indicating arg type, by ARGSIZE above (including ret type): a1 a2 a3 a4 a5 ret
        uint8_t     _cse:1;          // true if no side effects
        uint8_t     _fold:1;         // true if no side effects
        AbiKind     _abi:3;
        verbose_only ( const char* _name; )

        uint32_t _count_args(uint32_t mask) const;
        // Nb: uses right-to-left order, eg. sizes[0] is the size of the right-most arg.
        uint32_t get_sizes(ArgSize* sizes) const;

        inline ArgSize returnType() const {
            return ArgSize(_argtypes & ARGSIZE_MASK_ANY);
        }

        // Note that this indexes arguments *backwards*, that is to
        // get the Nth arg, you have to ask for index (numargs - N).
        // See mozilla bug 525815 for fixing this.
        inline ArgSize argType(uint32_t arg) const {
            return ArgSize((_argtypes >> (ARGSIZE_SHIFT * (arg+1))) & ARGSIZE_MASK_ANY);
        }

        inline bool isIndirect() const {
            return _address < 256;
        }
        inline uint32_t count_args() const {
            return _count_args(ARGSIZE_MASK_ANY);
        }
        inline uint32_t count_iargs() const {
            return _count_args(ARGSIZE_MASK_INT);
        }
        // fargs = args - iargs
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

    inline bool isCseOpcode(LOpcode op) {
        return
#if defined NANOJIT_64BIT
            (op >= LIR_quad && op <= LIR_quge) ||
#else
            (op >= LIR_i2f && op <= LIR_float) ||       // XXX: yuk;  use a table (bug 542932)
#endif
            (op >= LIR_int && op <= LIR_uge);
    }
    inline bool isRetOpcode(LOpcode op) {
        return 
#if defined NANOJIT_64BIT
            op == LIR_qret ||
#endif
            op == LIR_ret || op == LIR_fret;
    }
    inline bool isCmovOpcode(LOpcode op) {
        return 
#if defined NANOJIT_64BIT
            op == LIR_qcmov ||
#endif
            op == LIR_cmov;
    }
    inline LOpcode getCallOpcode(const CallInfo* ci) {
        LOpcode op = LIR_pcall;
        switch (ci->returnType()) {
        case ARGSIZE_NONE:  op = LIR_pcall; break;
        case ARGSIZE_I:
        case ARGSIZE_U:     op = LIR_icall; break;
        case ARGSIZE_F:     op = LIR_fcall; break;
#ifdef NANOJIT_64BIT
        case ARGSIZE_Q:     op = LIR_qcall; break;
#endif
        default:            NanoAssert(0);  break;
        }
        return op;
    }

    LOpcode f64arith_to_i32arith(LOpcode op);
#ifdef NANOJIT_64BIT
    LOpcode i32cmp_to_i64cmp(LOpcode op);
#endif

    // Array holding the 'repKind' field from LIRopcode.tbl.
    extern const uint8_t repKinds[];

    enum LTy {
        LTy_Void,   // no value/no type
        LTy_I32,    // 32-bit integer
#ifdef NANOJIT_64BIT
        LTy_I64,    // 64-bit integer
#endif
        LTy_F64,    // 64-bit float

        LTy_Ptr  = PTR_SIZE(LTy_I32, LTy_I64)   // word-sized integer
    };

    // Array holding the 'retType' field from LIRopcode.tbl.
    extern const LTy retTypes[];

    inline RegisterMask rmask(Register r)
    {
        return RegisterMask(1) << r;
    }

    //-----------------------------------------------------------------------
    // Low-level instructions.  This is a bit complicated, because we have a
    // variable-width representation to minimise space usage.
    //
    // - Instruction size is always an integral multiple of word size.
    //
    // - Every instruction has at least one word, holding the opcode and the
    //   reservation info.  That word is in class LIns.
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
        LRK_Sti,
        LRK_Sk,
        LRK_C,
        LRK_P,
        LRK_I,
        LRK_N64,
        LRK_Jtbl,
        LRK_None    // this one is used for unused opcode numbers
    };

    class LInsOp0;
    class LInsOp1;
    class LInsOp2;
    class LInsOp3;
    class LInsLd;
    class LInsSti;
    class LInsSk;
    class LInsC;
    class LInsP;
    class LInsI;
    class LInsN64;
    class LInsJtbl;

    class LIns
    {
    private:
        // LastWord: fields shared by all LIns kinds.  The .inReg, .reg,
        // .inAr and .arIndex fields form a "reservation" that is used
        // temporarily during assembly to record information relating to
        // register allocation.  See class RegAlloc for more details.
        //
        // Note: all combinations of .inReg/.inAr are possible, ie. 0/0, 0/1,
        // 1/0, 1/1.
        struct LastWord {
            uint32_t inReg:1;       // if 1, 'reg' is active
            Register reg:7;
            uint32_t inAr:1;        // if 1, 'arIndex' is active
            uint32_t arIndex:15;    // index into stack frame;  displ is -4*arIndex

            LOpcode  opcode:8;      // instruction's opcode
        };

        union {
            LastWord lastWord;
            // Force sizeof(LIns)==8 and 8-byte alignment on 64-bit machines.
            // This is necessary because sizeof(LastWord)==4 and we want all
            // instances of LIns to be pointer-aligned.
            void* dummy;
        };

        // LIns-to-LInsXYZ converters.
        inline LInsOp0* toLInsOp0() const;
        inline LInsOp1* toLInsOp1() const;
        inline LInsOp2* toLInsOp2() const;
        inline LInsOp3* toLInsOp3() const;
        inline LInsLd*  toLInsLd()  const;
        inline LInsSti* toLInsSti() const;
        inline LInsSk*  toLInsSk()  const;
        inline LInsC*   toLInsC()   const;
        inline LInsP*   toLInsP()   const;
        inline LInsI*   toLInsI()   const;
        inline LInsN64* toLInsN64() const;
        inline LInsJtbl*toLInsJtbl()const;

        void staticSanityCheck();

    public:
        // LIns initializers.
        inline void initLInsOp0(LOpcode opcode);
        inline void initLInsOp1(LOpcode opcode, LIns* oprnd1);
        inline void initLInsOp2(LOpcode opcode, LIns* oprnd1, LIns* oprnd2);
        inline void initLInsOp3(LOpcode opcode, LIns* oprnd1, LIns* oprnd2, LIns* oprnd3);
        inline void initLInsLd(LOpcode opcode, LIns* val, int32_t d);
        inline void initLInsSti(LOpcode opcode, LIns* val, LIns* base, int32_t d);
        inline void initLInsSk(LIns* prevLIns);
        // Nb: args[] must be allocated and initialised before being passed in;
        // initLInsC() just copies the pointer into the LInsC.
        inline void initLInsC(LOpcode opcode, LIns** args, const CallInfo* ci);
        inline void initLInsP(int32_t arg, int32_t kind);
        inline void initLInsI(LOpcode opcode, int32_t imm32);
        inline void initLInsN64(LOpcode opcode, int64_t imm64);
        inline void initLInsJtbl(LIns* index, uint32_t size, LIns** table);

        LOpcode opcode() const { return lastWord.opcode; }

        // XXX: old reservation manipulating functions.  See bug 538924.
        // Replacement strategy:
        // - deprecated_markAsClear() --> clearReg() and/or clearArIndex()
        // - deprecated_hasKnownReg() --> isInReg()
        // - deprecated_getReg() --> getReg() after checking isInReg()
        //
        void deprecated_markAsClear() {
            lastWord.inReg = 0;
            lastWord.inAr = 0;
        }
        bool deprecated_hasKnownReg() {
            NanoAssert(isUsed());
            return isInReg();
        }
        Register deprecated_getReg() {
            NanoAssert(isUsed());
            return ( isInReg() ? lastWord.reg : deprecated_UnknownReg );
        }
        uint32_t deprecated_getArIndex() {
            NanoAssert(isUsed());
            return ( isInAr() ? lastWord.arIndex : 0 );
        }

        // Reservation manipulation.
        bool isUsed() {
            return isInReg() || isInAr();
        }
        bool isInReg() {
            return lastWord.inReg;
        }
        bool isInRegMask(RegisterMask allow) {
            return isInReg() && (rmask(getReg()) & allow);
        }
        Register getReg() {
            NanoAssert(isInReg());
            return lastWord.reg;
        }
        void setReg(Register r) {
            lastWord.inReg = 1;
            lastWord.reg = r;
        }
        void clearReg() {
            lastWord.inReg = 0;
        }
        bool isInAr() {
            return lastWord.inAr;
        }
        uint32_t getArIndex() {
            NanoAssert(isInAr());
            return lastWord.arIndex;
        }
        void setArIndex(uint32_t arIndex) {
            lastWord.inAr = 1;
            lastWord.arIndex = arIndex;
        }
        void clearArIndex() {
            lastWord.inAr = 0;
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

        // Displacement for LInsLd/LInsSti
        inline int32_t  disp() const;

        // For LInsSk.
        inline LIns*    prevLIns() const;

        // For LInsP.
        inline uint8_t  paramArg()  const;
        inline uint8_t  paramKind() const;

        // For LInsI.
        inline int32_t  imm32() const;

        // For LInsN64.
        inline int32_t  imm64_0() const;
        inline int32_t  imm64_1() const;
        inline uint64_t imm64()   const;
        inline double   imm64f()  const;

        // For LIR_alloc.
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
        // isStore()==isLInsSti(), isCall()==isLInsC(), but that's ok;  these
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
        bool isLInsSti() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_Sti == repKinds[opcode()];
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
        bool isLInsN64() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_N64 == repKinds[opcode()];
        }
        bool isLInsJtbl() const {
            NanoAssert(LRK_None != repKinds[opcode()]);
            return LRK_Jtbl == repKinds[opcode()];
        }

        // LIns predicates.
        bool isCse() const {
            return isCseOpcode(opcode()) || (isCall() && callInfo()->_cse);
        }
        bool isRet() const {
            return isRetOpcode(opcode());
        }
        bool isLive() const {
            LOpcode op = opcode();
            return 
#if defined NANOJIT_64BIT
                op == LIR_qlive ||
#endif
                op == LIR_live || op == LIR_flive;
        }
        bool isop(LOpcode o) const {
            return opcode() == o;
        }
        bool isCond() const {
            return isop(LIR_ov) || isCmp();
        }
        bool isOverflowable() const {
            return isop(LIR_neg) || isop(LIR_add) || isop(LIR_sub) || isop(LIR_mul);
        }
        bool isCmp() const {
            LOpcode op = opcode();
            return (op >= LIR_eq  && op <= LIR_uge) ||
#if defined NANOJIT_64BIT
                   (op >= LIR_qeq && op <= LIR_quge) ||
#endif
                   (op >= LIR_feq && op <= LIR_fge);
        }
        bool isCall() const {
            return 
#if defined NANOJIT_64BIT
                isop(LIR_qcall) ||
#endif
                isop(LIR_icall) || isop(LIR_fcall);
        }
        bool isCmov() const {
            return isCmovOpcode(opcode());
        }
        bool isStore() const {
            return isLInsSti();
        }
        bool isLoad() const {
            return isLInsLd();
        }
        bool isGuard() const {
            return isop(LIR_x) || isop(LIR_xf) || isop(LIR_xt) ||
                   isop(LIR_xbarrier) || isop(LIR_xtbl);
        }
        // True if the instruction is a 32-bit or smaller constant integer.
        bool isconst() const {
            return isop(LIR_int);
        }
        // True if the instruction is a 32-bit or smaller constant integer and
        // has the value val when treated as a 32-bit signed integer.
        bool isconstval(int32_t val) const {
            return isconst() && imm32()==val;
        }
        // True if the instruction is a constant quad value.
        bool isconstq() const {
            return
#ifdef NANOJIT_64BIT
                isop(LIR_quad) || 
#endif
                isop(LIR_float);
        }
        // True if the instruction is a constant pointer value.
        bool isconstp() const
        {
#ifdef NANOJIT_64BIT
            return isconstq();
#else
            return isconst();
#endif
        }
        // True if the instruction is a constant float value.
        bool isconstf() const {
            return isop(LIR_float);
        }

        bool isBranch() const {
            return isop(LIR_jt) || isop(LIR_jf) || isop(LIR_j) || isop(LIR_jtbl);
        }

        LTy retType() const {
            return retTypes[opcode()];
        }
        bool isVoid() const {
            return retType() == LTy_Void;
        }
        bool isI32() const {
            return retType() == LTy_I32;
        }
#ifdef NANOJIT_64BIT
        bool isI64() const {
            return retType() == LTy_I64;
        }
#endif
        bool isF64() const {
            return retType() == LTy_F64;
        }
        bool isN64() const {
            return 
#ifdef NANOJIT_64BIT
                isI64() ||       
#endif
                isF64();
        }
        bool isPtr() const {
#ifdef NANOJIT_64BIT
            return isI64();
#else
            return isI32();
#endif
        }

        // Return true if removal of 'ins' from a LIR fragment could
        // possibly change the behaviour of that fragment, even if any
        // value computed by 'ins' is not used later in the fragment.
        // In other words, can 'ins' possibly alter control flow or memory?
        // Note, this assumes that loads will never fault and hence cannot
        // affect the control flow.
        bool isStmt() {
            NanoAssert(!isop(LIR_start) && !isop(LIR_skip));
            // All instructions with Void retType are statements.  And some
            // calls are statements too.
            if (isCall())
                return !isCse();
            else
                return isVoid();
        }

        inline void* constvalp() const
        {
        #ifdef NANOJIT_64BIT
            return (void*)imm64();
        #else
            return (void*)imm32();
        #endif
        }
    };

    typedef LIns* LInsp;
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

    // 1-operand form.  Used for LIR_ret, LIR_ov, unary arithmetic/logic ops,
    // etc.
    class LInsOp1
    {
    private:
        friend class LIns;

        LIns*       oprnd_1;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // 2-operand form.  Used for loads, guards, branches, comparisons, binary
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

    // 3-operand form.  Used for conditional moves.
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

        int32_t     disp;

        LIns*       oprnd_1;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for LIR_sti and LIR_stqi.
    class LInsSti
    {
    private:
        friend class LIns;

        int32_t     disp;

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

    // Used for LIR_iparam, LIR_qparam.
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

    // Used for LIR_int and LIR_alloc.
    class LInsI
    {
    private:
        friend class LIns;

        int32_t     imm32;

        LIns        ins;

    public:
        LIns* getLIns() { return &ins; };
    };

    // Used for LIR_quad and LIR_float.
    class LInsN64
    {
    private:
        friend class LIns;

        int32_t     imm64_0;

        int32_t     imm64_1;

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

    LInsOp0* LIns::toLInsOp0() const { return (LInsOp0*)( uintptr_t(this+1) - sizeof(LInsOp0) ); }
    LInsOp1* LIns::toLInsOp1() const { return (LInsOp1*)( uintptr_t(this+1) - sizeof(LInsOp1) ); }
    LInsOp2* LIns::toLInsOp2() const { return (LInsOp2*)( uintptr_t(this+1) - sizeof(LInsOp2) ); }
    LInsOp3* LIns::toLInsOp3() const { return (LInsOp3*)( uintptr_t(this+1) - sizeof(LInsOp3) ); }
    LInsLd*  LIns::toLInsLd()  const { return (LInsLd* )( uintptr_t(this+1) - sizeof(LInsLd ) ); }
    LInsSti* LIns::toLInsSti() const { return (LInsSti*)( uintptr_t(this+1) - sizeof(LInsSti) ); }
    LInsSk*  LIns::toLInsSk()  const { return (LInsSk* )( uintptr_t(this+1) - sizeof(LInsSk ) ); }
    LInsC*   LIns::toLInsC()   const { return (LInsC*  )( uintptr_t(this+1) - sizeof(LInsC  ) ); }
    LInsP*   LIns::toLInsP()   const { return (LInsP*  )( uintptr_t(this+1) - sizeof(LInsP  ) ); }
    LInsI*   LIns::toLInsI()   const { return (LInsI*  )( uintptr_t(this+1) - sizeof(LInsI  ) ); }
    LInsN64* LIns::toLInsN64() const { return (LInsN64*)( uintptr_t(this+1) - sizeof(LInsN64) ); }
    LInsJtbl*LIns::toLInsJtbl()const { return (LInsJtbl*)(uintptr_t(this+1) - sizeof(LInsJtbl)); }

    void LIns::initLInsOp0(LOpcode opcode) {
        clearReg();
        clearArIndex();
        lastWord.opcode = opcode;
        NanoAssert(isLInsOp0());
    }
    void LIns::initLInsOp1(LOpcode opcode, LIns* oprnd1) {
        clearReg();
        clearArIndex();
        lastWord.opcode = opcode;
        toLInsOp1()->oprnd_1 = oprnd1;
        NanoAssert(isLInsOp1());
    }
    void LIns::initLInsOp2(LOpcode opcode, LIns* oprnd1, LIns* oprnd2) {
        clearReg();
        clearArIndex();
        lastWord.opcode = opcode;
        toLInsOp2()->oprnd_1 = oprnd1;
        toLInsOp2()->oprnd_2 = oprnd2;
        NanoAssert(isLInsOp2());
    }
    void LIns::initLInsOp3(LOpcode opcode, LIns* oprnd1, LIns* oprnd2, LIns* oprnd3) {
        clearReg();
        clearArIndex();
        lastWord.opcode = opcode;
        toLInsOp3()->oprnd_1 = oprnd1;
        toLInsOp3()->oprnd_2 = oprnd2;
        toLInsOp3()->oprnd_3 = oprnd3;
        NanoAssert(isLInsOp3());
    }
    void LIns::initLInsLd(LOpcode opcode, LIns* val, int32_t d) {
        clearReg();
        clearArIndex();
        lastWord.opcode = opcode;
        toLInsLd()->oprnd_1 = val;
        toLInsLd()->disp = d;
        NanoAssert(isLInsLd());
    }
    void LIns::initLInsSti(LOpcode opcode, LIns* val, LIns* base, int32_t d) {
        clearReg();
        clearArIndex();
        lastWord.opcode = opcode;
        toLInsSti()->oprnd_1 = val;
        toLInsSti()->oprnd_2 = base;
        toLInsSti()->disp = d;
        NanoAssert(isLInsSti());
    }
    void LIns::initLInsSk(LIns* prevLIns) {
        clearReg();
        clearArIndex();
        lastWord.opcode = LIR_skip;
        toLInsSk()->prevLIns = prevLIns;
        NanoAssert(isLInsSk());
    }
    void LIns::initLInsC(LOpcode opcode, LIns** args, const CallInfo* ci) {
        clearReg();
        clearArIndex();
        lastWord.opcode = opcode;
        toLInsC()->args = args;
        toLInsC()->ci = ci;
        NanoAssert(isLInsC());
    }
    void LIns::initLInsP(int32_t arg, int32_t kind) {
        clearReg();
        clearArIndex();
        lastWord.opcode = LIR_param;
        NanoAssert(isU8(arg) && isU8(kind));
        toLInsP()->arg = arg;
        toLInsP()->kind = kind;
        NanoAssert(isLInsP());
    }
    void LIns::initLInsI(LOpcode opcode, int32_t imm32) {
        clearReg();
        clearArIndex();
        lastWord.opcode = opcode;
        toLInsI()->imm32 = imm32;
        NanoAssert(isLInsI());
    }
    void LIns::initLInsN64(LOpcode opcode, int64_t imm64) {
        clearReg();
        clearArIndex();
        lastWord.opcode = opcode;
        toLInsN64()->imm64_0 = int32_t(imm64);
        toLInsN64()->imm64_1 = int32_t(imm64 >> 32);
        NanoAssert(isLInsN64());
    }
    void LIns::initLInsJtbl(LIns* index, uint32_t size, LIns** table) {
        clearReg();
        clearArIndex();
        lastWord.opcode = LIR_jtbl;
        toLInsJtbl()->oprnd_1 = index;
        toLInsJtbl()->table = table;
        toLInsJtbl()->size = size;
        NanoAssert(isLInsJtbl());
    }

    LIns* LIns::oprnd1() const {
        NanoAssert(isLInsOp1() || isLInsOp2() || isLInsOp3() || isLInsLd() || isLInsSti() || isLInsJtbl());
        return toLInsOp2()->oprnd_1;
    }
    LIns* LIns::oprnd2() const {
        NanoAssert(isLInsOp2() || isLInsOp3() || isLInsSti());
        return toLInsOp2()->oprnd_2;
    }
    LIns* LIns::oprnd3() const {
        NanoAssert(isLInsOp3());
        return toLInsOp3()->oprnd_3;
    }

    LIns* LIns::getTarget() const {
        NanoAssert(isBranch() && !isop(LIR_jtbl));
        return oprnd2();
    }

    void LIns::setTarget(LIns* label) {
        NanoAssert(label && label->isop(LIR_label));
        NanoAssert(isBranch() && !isop(LIR_jtbl));
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
        return (GuardRecord*)oprnd2();
    }

    int32_t LIns::disp() const {
        if (isLInsSti()) {
            return toLInsSti()->disp;
        } else {
            NanoAssert(isLInsLd());
            return toLInsLd()->disp;
        }
    }

    LIns* LIns::prevLIns() const {
        NanoAssert(isLInsSk());
        return toLInsSk()->prevLIns;
    }

    inline uint8_t LIns::paramArg()  const { NanoAssert(isop(LIR_param)); return toLInsP()->arg; }
    inline uint8_t LIns::paramKind() const { NanoAssert(isop(LIR_param)); return toLInsP()->kind; }

    inline int32_t LIns::imm32()     const { NanoAssert(isconst());  return toLInsI()->imm32; }

    inline int32_t LIns::imm64_0()   const { NanoAssert(isconstq()); return toLInsN64()->imm64_0; }
    inline int32_t LIns::imm64_1()   const { NanoAssert(isconstq()); return toLInsN64()->imm64_1; }
    uint64_t       LIns::imm64()     const {
        NanoAssert(isconstq());
        return (uint64_t(toLInsN64()->imm64_1) << 32) | uint32_t(toLInsN64()->imm64_0);
    }
    double         LIns::imm64f()    const {
        union {
            double f;
            uint64_t q;
        } u;
        u.q = imm64();
        return u.f;
    }

    int32_t LIns::size() const {
        NanoAssert(isop(LIR_alloc));
        return toLInsI()->imm32 << 2;
    }

    void LIns::setSize(int32_t nbytes) {
        NanoAssert(isop(LIR_alloc));
        NanoAssert(nbytes > 0);
        toLInsI()->imm32 = (nbytes+3)>>2; // # of required 32bit words
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

        virtual LInsp ins0(LOpcode v) {
            return out->ins0(v);
        }
        virtual LInsp ins1(LOpcode v, LIns* a) {
            return out->ins1(v, a);
        }
        virtual LInsp ins2(LOpcode v, LIns* a, LIns* b) {
            return out->ins2(v, a, b);
        }
        virtual LInsp ins3(LOpcode v, LIns* a, LIns* b, LIns* c) {
            return out->ins3(v, a, b, c);
        }
        virtual LInsp insGuard(LOpcode v, LIns *c, GuardRecord *gr) {
            return out->insGuard(v, c, gr);
        }
        virtual LInsp insBranch(LOpcode v, LInsp condition, LInsp to) {
            return out->insBranch(v, condition, to);
        }
        // arg: 0=first, 1=second, ...
        // kind: 0=arg 1=saved-reg
        virtual LInsp insParam(int32_t arg, int32_t kind) {
            return out->insParam(arg, kind);
        }
        virtual LInsp insImm(int32_t imm) {
            return out->insImm(imm);
        }
#ifdef NANOJIT_64BIT
        virtual LInsp insImmq(uint64_t imm) {
            return out->insImmq(imm);
        }
#endif
        virtual LInsp insImmf(double d) {
            return out->insImmf(d);
        }
        virtual LInsp insLoad(LOpcode op, LIns* base, int32_t d) {
            return out->insLoad(op, base, d);
        }
        virtual LInsp insStore(LOpcode op, LIns* value, LIns* base, int32_t d) {
            return out->insStore(op, value, base, d);
        }
        // args[] is in reverse order, ie. args[0] holds the rightmost arg.
        virtual LInsp insCall(const CallInfo *call, LInsp args[]) {
            return out->insCall(call, args);
        }
        virtual LInsp insAlloc(int32_t size) {
            NanoAssert(size != 0);
            return out->insAlloc(size);
        }
        virtual LInsp insJtbl(LIns* index, uint32_t size) {
            return out->insJtbl(index, size);
        }

        // convenience functions

        // Inserts a conditional to execute and branches to execute if
        // the condition is true and false respectively.
        LIns*        ins_choose(LIns* cond, LIns* iftrue, LIns* iffalse, bool use_cmov);
        // Inserts an integer comparison to 0
        LIns*        ins_eq0(LIns* oprnd1);
        // Inserts a pointer comparison to 0
        LIns*        ins_peq0(LIns* oprnd1);
        // Inserts a binary operation where the second operand is an
        // integer immediate.
        LIns*        ins2i(LOpcode op, LIns *oprnd1, int32_t);
#if NJ_SOFTFLOAT_SUPPORTED
        LIns*        qjoin(LInsp lo, LInsp hi);
#endif
        LIns*        insImmPtr(const void *ptr);
        LIns*        insImmWord(intptr_t ptr);
        // Sign or zero extend integers to native integers. On 32-bit this is a no-op.
        LIns*        ins_i2p(LIns* intIns);
        LIns*        ins_u2p(LIns* uintIns);
        // choose LIR_sti or LIR_stqi based on size of value
        LIns*        insStorei(LIns* value, LIns* base, int32_t d);
    };


#ifdef NJ_VERBOSE
    extern const char* lirNames[];

    /**
     * map address ranges to meaningful names.
     */
    class LabelMap
    {
        Allocator& allocator;
        class Entry
        {
        public:
            Entry(int) : name(0), size(0), align(0) {}
            Entry(char *n, size_t s, size_t a) : name(n),size(s),align(a) {}
            char* name;
            size_t size:29, align:3;
        };
        TreeMap<const void*, Entry*> names;
        LogControl *logc;
        char buf[5000], *end;
        void formatAddr(const void *p, char *buf);
    public:
        LabelMap(Allocator& allocator, LogControl* logc);
        void add(const void *p, size_t size, size_t align, const char *name);
        const char *dup(const char *);
        const char *format(const void *p);
    };

    class LirNameMap
    {
        Allocator& alloc;

        template <class Key>
        class CountMap: public HashMap<Key, int> {
        public:
            CountMap(Allocator& alloc) : HashMap<Key, int>(alloc) {}
            int add(Key k) {
                int c = 1;
                if (containsKey(k)) {
                    c = 1+get(k);
                }
                put(k,c);
                return c;
            }
        };
        CountMap<int> lircounts;
        CountMap<const CallInfo *> funccounts;

        class Entry
        {
        public:
            Entry(int) : name(0) {}
            Entry(char* n) : name(n) {}
            char* name;
        };
        HashMap<LInsp, Entry*> names;
        void formatImm(int32_t c, char *buf);
        void formatImmq(uint64_t c, char *buf);

    public:
        LabelMap *labels;
        LirNameMap(Allocator& alloc, LabelMap *lm)
            : alloc(alloc),
            lircounts(alloc),
            funccounts(alloc),
            names(alloc),
            labels(lm)
        {}

        void addName(LInsp i, const char *s);
        void copyName(LInsp i, const char *s, int suffix);
        const char *formatRef(LIns *ref);
        const char *formatIns(LInsp i);
        void formatGuard(LInsp i, char *buf);
    };


    class VerboseWriter : public LirWriter
    {
        InsList code;
        LirNameMap* names;
        LogControl* logc;
        const char* const prefix;
        bool const always_flush;
    public:
        VerboseWriter(Allocator& alloc, LirWriter *out,
                      LirNameMap* names, LogControl* logc, const char* prefix = "", bool always_flush = false)
            : LirWriter(out), code(alloc), names(names), logc(logc), prefix(prefix), always_flush(always_flush)
        {}

        LInsp add(LInsp i) {
            if (i) {
                code.add(i);
                if (always_flush)
                    flush();
            }
            return i;
        }

        LInsp add_flush(LInsp i) {
            if ((i = add(i)) != 0)
                flush();
            return i;
        }

        void flush()
        {
            if (!code.isEmpty()) {
                int32_t count = 0;
                for (Seq<LIns*>* p = code.get(); p != NULL; p = p->tail) {
                    logc->printf("%s    %s\n",prefix,names->formatIns(p->head));
                    count++;
                }
                code.clear();
                if (count > 1)
                    logc->printf("\n");
            }
        }

        LIns* insGuard(LOpcode op, LInsp cond, GuardRecord *gr) {
            return add_flush(out->insGuard(op,cond,gr));
        }

        LIns* insBranch(LOpcode v, LInsp condition, LInsp to) {
            return add_flush(out->insBranch(v, condition, to));
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

        LIns* ins1(LOpcode v, LInsp a) {
            return isRetOpcode(v) ? add_flush(out->ins1(v, a)) : add(out->ins1(v, a));
        }
        LIns* ins2(LOpcode v, LInsp a, LInsp b) {
            return add(out->ins2(v, a, b));
        }
        LIns* ins3(LOpcode v, LInsp a, LInsp b, LInsp c) {
            return add(out->ins3(v, a, b, c));
        }
        LIns* insCall(const CallInfo *call, LInsp args[]) {
            return add_flush(out->insCall(call, args));
        }
        LIns* insParam(int32_t i, int32_t kind) {
            return add(out->insParam(i, kind));
        }
        LIns* insLoad(LOpcode v, LInsp base, int32_t disp) {
            return add(out->insLoad(v, base, disp));
        }
        LIns* insStore(LOpcode op, LInsp v, LInsp b, int32_t d) {
            return add(out->insStore(op, v, b, d));
        }
        LIns* insAlloc(int32_t size) {
            return add(out->insAlloc(size));
        }
        LIns* insImm(int32_t imm) {
            return add(out->insImm(imm));
        }
#ifdef NANOJIT_64BIT
        LIns* insImmq(uint64_t imm) {
            return add(out->insImmq(imm));
        }
#endif
        LIns* insImmf(double d) {
            return add(out->insImmf(d));
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
        LIns* insGuard(LOpcode, LIns *cond, GuardRecord *);
        LIns* insBranch(LOpcode, LIns *cond, LIns *target);
        LIns* insLoad(LOpcode op, LInsp base, int32_t off);
    };

    enum LInsHashKind {
        // We divide instruction kinds into groups for the use of LInsHashSet.
        // LIns0 isn't present because we don't need to record any 0-ary
        // instructions.
        LInsImm   = 0,
        LInsImmq  = 1,  // only occurs on 64-bit platforms
        LInsImmf  = 2,
        LIns1     = 3,
        LIns2     = 4,
        LIns3     = 5,
        LInsLoad  = 6,
        LInsCall  = 7,

        LInsFirst = 0,
        LInsLast = 7,
        // need a value after "last" to outsmart compilers that will insist last+1 is impossible
        LInsInvalid = 8
    };
    #define nextKind(kind)  LInsHashKind(kind+1)

    // @todo, this could be replaced by a generic HashMap or HashSet, if we had one
    class LInsHashSet
    {
        // Must be a power of 2.
        // Don't start too small, or we'll waste time growing and rehashing.
        // Don't start too large, will waste memory.
        static const uint32_t kInitialCap[LInsLast + 1];

        // There is one list for each instruction kind.  This lets us size the
        // lists appropriately (some instructions are more common than others).
        // It also lets us have kind-specific find/add/grow functions, which
        // are faster than generic versions.
        LInsp *m_list[LInsLast + 1];
        uint32_t m_cap[LInsLast + 1];
        uint32_t m_used[LInsLast + 1];
        typedef uint32_t (LInsHashSet::*find_t)(LInsp);
        find_t m_find[LInsLast + 1];
        Allocator& alloc;

        static uint32_t hashImm(int32_t);
        static uint32_t hashImmq(uint64_t);     // not NANOJIT_64BIT only used by findImmf()
        static uint32_t hash1(LOpcode v, LInsp);
        static uint32_t hash2(LOpcode v, LInsp, LInsp);
        static uint32_t hash3(LOpcode v, LInsp, LInsp, LInsp);
        static uint32_t hashLoad(LOpcode v, LInsp, int32_t);
        static uint32_t hashCall(const CallInfo *call, uint32_t argc, LInsp args[]);

        // These private versions are used after an LIns has been created;
        // they are used for rehashing after growing.
        uint32_t findImm(LInsp ins);
#ifdef NANOJIT_64BIT
        uint32_t findImmq(LInsp ins);
#endif
        uint32_t findImmf(LInsp ins);
        uint32_t find1(LInsp ins);
        uint32_t find2(LInsp ins);
        uint32_t find3(LInsp ins);
        uint32_t findLoad(LInsp ins);
        uint32_t findCall(LInsp ins);

        void grow(LInsHashKind kind);

    public:
        // kInitialCaps[i] holds the initial size for m_list[i].
        LInsHashSet(Allocator&, uint32_t kInitialCaps[]);

        // These public versions are used before an LIns has been created.
        LInsp findImm(int32_t a, uint32_t &k);
#ifdef NANOJIT_64BIT
        LInsp findImmq(uint64_t a, uint32_t &k);
#endif
        LInsp findImmf(uint64_t d, uint32_t &k);
        LInsp find1(LOpcode v, LInsp a, uint32_t &k);
        LInsp find2(LOpcode v, LInsp a, LInsp b, uint32_t &k);
        LInsp find3(LOpcode v, LInsp a, LInsp b, LInsp c, uint32_t &k);
        LInsp findLoad(LOpcode v, LInsp a, int32_t b, uint32_t &k);
        LInsp findCall(const CallInfo *call, uint32_t argc, LInsp args[], uint32_t &k);

        // 'k' is the index found by findXYZ().
        LInsp add(LInsHashKind kind, LInsp ins, uint32_t k);

        void clear();
    };

    class CseFilter: public LirWriter
    {
    private:
        LInsHashSet* exprs;

    public:
        CseFilter(LirWriter *out, Allocator&);

        LIns* insImm(int32_t imm);
#ifdef NANOJIT_64BIT
        LIns* insImmq(uint64_t q);
#endif
        LIns* insImmf(double d);
        LIns* ins0(LOpcode v);
        LIns* ins1(LOpcode v, LInsp);
        LIns* ins2(LOpcode v, LInsp, LInsp);
        LIns* ins3(LOpcode v, LInsp, LInsp, LInsp);
        LIns* insLoad(LOpcode op, LInsp cond, int32_t d);
        LIns* insCall(const CallInfo *call, LInsp args[]);
        LIns* insGuard(LOpcode op, LInsp cond, GuardRecord *gr);
    };

    class LirBuffer
    {
        public:
            LirBuffer(Allocator& alloc);
            void        clear();
            uintptr_t   makeRoom(size_t szB);   // make room for an instruction

            debug_only (void validate() const;)
            verbose_only(LirNameMap* names;)

            int32_t insCount();
            size_t  byteCount();

            // stats
            struct
            {
                uint32_t lir;    // # instructions
            }
            _stats;

            AbiKind abi;
            LInsp state,param1,sp,rp;
            LInsp savedRegs[NumSavedRegs];

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
            LInsp   insLoad(LOpcode op, LInsp base, int32_t disp);
            LInsp   insStore(LOpcode op, LInsp o1, LInsp o2, int32_t disp);
            LInsp   ins0(LOpcode op);
            LInsp   ins1(LOpcode op, LInsp o1);
            LInsp   ins2(LOpcode op, LInsp o1, LInsp o2);
            LInsp   ins3(LOpcode op, LInsp o1, LInsp o2, LInsp o3);
            LInsp   insParam(int32_t i, int32_t kind);
            LInsp   insImm(int32_t imm);
#ifdef NANOJIT_64BIT
            LInsp   insImmq(uint64_t imm);
#endif
            LInsp   insImmf(double d);
            LInsp   insCall(const CallInfo *call, LInsp args[]);
            LInsp   insGuard(LOpcode op, LInsp cond, GuardRecord *gr);
            LInsp   insBranch(LOpcode v, LInsp condition, LInsp to);
            LInsp   insAlloc(int32_t size);
            LInsp   insJtbl(LIns* index, uint32_t size);
    };

    class LirFilter
    {
    public:
        LirFilter *in;
        LirFilter(LirFilter *in) : in(in) {}
        virtual ~LirFilter(){}

        virtual LInsp read() {
            return in->read();
        }
        virtual LInsp pos() {
            return in->pos();
        }
    };

    // concrete
    class LirReader : public LirFilter
    {
        LInsp _i; // next instruction to be read;  invariant: is never a skip

    public:
        LirReader(LInsp i) : LirFilter(0), _i(i)
        {
            // The last instruction for a fragment shouldn't be a skip.
            // (Actually, if the last *inserted* instruction exactly fills up
            // a chunk, a new chunk will be created, and thus the last *written*
            // instruction will be a skip -- the one needed for the
            // cross-chunk link.  But the last *inserted* instruction is what
            // is recorded and used to initialise each LirReader, and that is
            // what is seen here, and therefore this assertion holds.)
            NanoAssert(i && !i->isop(LIR_skip));
        }
        virtual ~LirReader() {}

        // Returns next instruction and advances to the prior instruction.
        // Invariant: never returns a skip.
        LInsp read();

        // Returns next instruction.  Invariant: never returns a skip.
        LInsp pos() {
            return _i;
        }
    };

    verbose_only(void live(LirFilter* in, Allocator& alloc, Fragment* frag, LogControl*);)

    class StackFilter: public LirFilter
    {
        LInsp sp;
        LInsp rp;
        BitSet spStk;
        BitSet rpStk;
        int spTop;
        int rpTop;
        void getTops(LInsp br, int& spTop, int& rpTop);

    public:
        StackFilter(LirFilter *in, Allocator& alloc, LInsp sp, LInsp rp);
        bool ignoreStore(LInsp ins, int top, BitSet* stk);
        LInsp read();
    };

    // eliminate redundant loads by watching for stores & mutator calls
    class LoadFilter: public LirWriter
    {
    public:
        LInsp sp, rp;
        LInsHashSet* exprs;

        void clear(LInsp p);

    public:
        LoadFilter(LirWriter *out, Allocator& alloc)
            : LirWriter(out), sp(NULL), rp(NULL)
        {
            uint32_t kInitialCaps[LInsLast + 1];
            kInitialCaps[LInsImm]   = 1;
            kInitialCaps[LInsImmq]  = 1;
            kInitialCaps[LInsImmf]  = 1;
            kInitialCaps[LIns1]     = 1;
            kInitialCaps[LIns2]     = 1;
            kInitialCaps[LIns3]     = 1;
            kInitialCaps[LInsLoad]  = 64;
            kInitialCaps[LInsCall]  = 1;
            exprs = new (alloc) LInsHashSet(alloc, kInitialCaps);
        }

        LInsp ins0(LOpcode);
        LInsp insLoad(LOpcode, LInsp base, int32_t disp);
        LInsp insStore(LOpcode op, LInsp v, LInsp b, int32_t d);
        LInsp insCall(const CallInfo *call, LInsp args[]);
    };

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
        LIns *split(const CallInfo *call, LInsp args[]);
        LIns *fcall1(const CallInfo *call, LIns *a);
        LIns *fcall2(const CallInfo *call, LIns *a, LIns *b);
        LIns *fcmp(const CallInfo *call, LIns *a, LIns *b);
        LIns *ins1(LOpcode op, LIns *a);
        LIns *ins2(LOpcode op, LIns *a, LIns *b);
        LIns *insCall(const CallInfo *ci, LInsp args[]);
    };


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
        const char* _whereInPipeline;

        const char* type2string(LTy type);
        void typeCheckArgs(LOpcode op, int nArgs, LTy formals[], LIns* args[]);
        void errorStructureShouldBe(LOpcode op, const char* argDesc, int argN, LIns* arg,
                                    const char* shouldBeDesc);
        void checkLInsHasOpcode(LOpcode op, int argN, LIns* ins, LOpcode op2);
        void checkLInsIsACondOrConst(LOpcode op, int argN, LIns* ins);
        void checkLInsIsNull(LOpcode op, int argN, LIns* ins);
        void checkLInsIsOverflowable(LOpcode op, int argN, LIns* ins);
        void checkOprnd1ImmediatelyPrecedes(LIns* ins);

    public:
        ValidateWriter(LirWriter* out, const char* stageName);
        LIns* insLoad(LOpcode op, LIns* base, int32_t d);
        LIns* insStore(LOpcode op, LIns* value, LIns* base, int32_t d);
        LIns* ins0(LOpcode v);
        LIns* ins1(LOpcode v, LIns* a);
        LIns* ins2(LOpcode v, LIns* a, LIns* b);
        LIns* ins3(LOpcode v, LIns* a, LIns* b, LIns* c);
        LIns* insParam(int32_t arg, int32_t kind);
        LIns* insImm(int32_t imm);
#ifdef NANOJIT_64BIT
        LIns* insImmq(uint64_t imm);
#endif
        LIns* insImmf(double d);
        LIns* insCall(const CallInfo *call, LIns* args[]);
        LIns* insGuard(LOpcode v, LIns *c, GuardRecord *gr);
        LIns* insBranch(LOpcode v, LIns* condition, LIns* to);
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

        void finish();
        LInsp read();
    };
#endif

}
#endif // __nanojit_LIR__
