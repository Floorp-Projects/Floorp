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

namespace nanojit
{
    using namespace avmplus;
    #ifdef FEATURE_NANOJIT

    const uint8_t repKinds[] = {
#define OP___(op, number, repKind, retType, isCse) \
        LRK_##repKind,
#include "LIRopcode.tbl"
#undef OP___
        0
    };

    const LTy retTypes[] = {
#define OP___(op, number, repKind, retType, isCse) \
        LTy_##retType,
#include "LIRopcode.tbl"
#undef OP___
        LTy_Void
    };

    const int8_t isCses[] = {
#define OP___(op, number, repKind, retType, isCse) \
        isCse,
#include "LIRopcode.tbl"
#undef OP___
        0
    };

    // LIR verbose specific
    #ifdef NJ_VERBOSE

    const char* lirNames[] = {
#define OP___(op, number, repKind, retType, isCse) \
        #op,
#include "LIRopcode.tbl"
#undef OP___
        NULL
    };

    #endif /* NANOJIT_VERBOSE */

    uint32_t CallInfo::count_args() const
    {
        uint32_t argc = 0;
        uint32_t argt = _typesig;
        argt >>= ARGTYPE_SHIFT;         // remove retType
        while (argt) {
            argc++;
            argt >>= ARGTYPE_SHIFT;
        }
        return argc;
    }

    uint32_t CallInfo::count_int32_args() const
    {
        uint32_t argc = 0;
        uint32_t argt = _typesig;
        argt >>= ARGTYPE_SHIFT;     // remove retType
        while (argt) {
            ArgType a = ArgType(argt & ARGTYPE_MASK);
            if (a == ARGTYPE_I || a == ARGTYPE_U)
                argc++;
            argt >>= ARGTYPE_SHIFT;
        }
        return argc;
    }

    uint32_t CallInfo::getArgTypes(ArgType* argTypes) const
    {
        uint32_t argc = 0;
        uint32_t argt = _typesig;
        argt >>= ARGTYPE_SHIFT;         // remove retType
        while (argt) {
            ArgType a = ArgType(argt & ARGTYPE_MASK);
            argTypes[argc] = a;
            argc++;
            argt >>= ARGTYPE_SHIFT;
        }
        return argc;
    }

    // implementation
#ifdef NJ_VERBOSE
    void ReverseLister::finish()
    {
        _logc->printf("\n");
        _logc->printf("=== BEGIN %s ===\n", _title);
        int j = 0;
        for (Seq<char*>* p = _strs.get(); p != NULL; p = p->tail)
            _logc->printf("  %02d: %s\n", j++, p->head);
        _logc->printf("=== END %s ===\n", _title);
        _logc->printf("\n");
    }

    LInsp ReverseLister::read()
    {
        // This check is necessary to avoid printing the LIR_start multiple
        // times due to lookahead in Assembler::gen().
        if (_prevIns && _prevIns->isop(LIR_start))
            return _prevIns;
        LInsp ins = in->read();
        InsBuf b;
        const char* str = _printer->formatIns(&b, ins);
        char* cpy = new (_alloc) char[strlen(str)+1];
        VMPI_strcpy(cpy, str);
        _strs.insert(cpy);
        _prevIns = ins;
        return ins;
    }
#endif

#ifdef NJ_PROFILE
    // @todo fixup move to nanojit.h
    #undef counter_value
    #define counter_value(x)        x
#endif /* NJ_PROFILE */

    // LCompressedBuffer
    LirBuffer::LirBuffer(Allocator& alloc) :
#ifdef NJ_VERBOSE
          printer(NULL),
#endif
          abi(ABI_FASTCALL), state(NULL), param1(NULL), sp(NULL), rp(NULL),
          _allocator(alloc)
    {
        clear();
    }

    void LirBuffer::clear()
    {
        // clear the stats, etc
        _unused = 0;
        _limit = 0;
        _bytesAllocated = 0;
        _stats.lir = 0;
        for (int i = 0; i < NumSavedRegs; ++i)
            savedRegs[i] = NULL;
        chunkAlloc();
    }

    void LirBuffer::chunkAlloc()
    {
        _unused = (uintptr_t) _allocator.alloc(CHUNK_SZB);
        NanoAssert(_unused != 0); // Allocator.alloc() never returns null. See Allocator.h
        _limit = _unused + CHUNK_SZB;
    }

    int32_t LirBuffer::insCount()
    {
        return _stats.lir;
    }

    size_t LirBuffer::byteCount()
    {
        return _bytesAllocated - (_limit - _unused);
    }

    // Allocate a new page, and write the first instruction to it -- a skip
    // linking to last instruction of the previous page.
    void LirBuffer::moveToNewChunk(uintptr_t addrOfLastLInsOnCurrentChunk)
    {
        chunkAlloc();
        // Link LIR stream back to prior instruction.
        // Unlike all the ins*() functions, we don't call makeRoom() here
        // because we know we have enough space, having just started a new
        // page.
        LInsSk* insSk = (LInsSk*)_unused;
        LIns*   ins   = insSk->getLIns();
        ins->initLInsSk((LInsp)addrOfLastLInsOnCurrentChunk);
        _unused += sizeof(LInsSk);
        verbose_only(_stats.lir++);
    }

    // Make room for a single instruction.
    uintptr_t LirBuffer::makeRoom(size_t szB)
    {
        // Make sure the size is ok
        NanoAssert(0 == szB % sizeof(void*));
        NanoAssert(sizeof(LIns) <= szB && szB <= sizeof(LInsSti));  // LInsSti is the biggest one
        NanoAssert(_unused < _limit);

        debug_only( bool moved = false; )

        // If the instruction won't fit on the current chunk, get a new chunk
        if (_unused + szB > _limit) {
            uintptr_t addrOfLastLInsOnChunk = _unused - sizeof(LIns);
            moveToNewChunk(addrOfLastLInsOnChunk);
            debug_only( moved = true; )
        }

        // We now know that we are on a chunk that has the requested amount of
        // room: record the starting address of the requested space and bump
        // the pointer.
        uintptr_t startOfRoom = _unused;
        _unused += szB;
        verbose_only(_stats.lir++);             // count the instruction

        // If there's no more space on this chunk, move to a new one.
        // (This will only occur if the asked-for size filled up exactly to
        // the end of the chunk.)  This ensures that next time we enter this
        // function, _unused won't be pointing one byte past the end of
        // the chunk, which would break everything.
        if (_unused >= _limit) {
            // Check we used exactly the remaining space
            NanoAssert(_unused == _limit);
            NanoAssert(!moved);     // shouldn't need to moveToNewChunk twice
            uintptr_t addrOfLastLInsOnChunk = _unused - sizeof(LIns);
            moveToNewChunk(addrOfLastLInsOnChunk);
        }

        // Make sure it's word-aligned.
        NanoAssert(0 == startOfRoom % sizeof(void*));
        return startOfRoom;
    }

    LInsp LirBufWriter::insStore(LOpcode op, LInsp val, LInsp base, int32_t d, AccSet accSet)
    {
        if (isS16(d)) {
            LInsSti* insSti = (LInsSti*)_buf->makeRoom(sizeof(LInsSti));
            LIns*    ins    = insSti->getLIns();
            ins->initLInsSti(op, val, base, d, accSet);
            return ins;
        } else {
            // If the displacement is more than 16 bits, put it in a separate instruction.
            return insStore(op, val, ins2(LIR_piadd, base, insImmWord(d)), 0, accSet);
        }
    }

    LInsp LirBufWriter::ins0(LOpcode op)
    {
        LInsOp0* insOp0 = (LInsOp0*)_buf->makeRoom(sizeof(LInsOp0));
        LIns*    ins    = insOp0->getLIns();
        ins->initLInsOp0(op);
        return ins;
    }

    LInsp LirBufWriter::ins1(LOpcode op, LInsp o1)
    {
        LInsOp1* insOp1 = (LInsOp1*)_buf->makeRoom(sizeof(LInsOp1));
        LIns*    ins    = insOp1->getLIns();
        ins->initLInsOp1(op, o1);
        return ins;
    }

    LInsp LirBufWriter::ins2(LOpcode op, LInsp o1, LInsp o2)
    {
        LInsOp2* insOp2 = (LInsOp2*)_buf->makeRoom(sizeof(LInsOp2));
        LIns*    ins    = insOp2->getLIns();
        ins->initLInsOp2(op, o1, o2);
        return ins;
    }

    LInsp LirBufWriter::ins3(LOpcode op, LInsp o1, LInsp o2, LInsp o3)
    {
        LInsOp3* insOp3 = (LInsOp3*)_buf->makeRoom(sizeof(LInsOp3));
        LIns*    ins    = insOp3->getLIns();
        ins->initLInsOp3(op, o1, o2, o3);
        return ins;
    }

    LInsp LirBufWriter::insLoad(LOpcode op, LInsp base, int32_t d, AccSet accSet)
    {
        if (isS16(d)) {
            LInsLd* insLd = (LInsLd*)_buf->makeRoom(sizeof(LInsLd));
            LIns*   ins   = insLd->getLIns();
            ins->initLInsLd(op, base, d, accSet);
            return ins;
        } else {
            // If the displacement is more than 16 bits, put it in a separate instruction.
            // Note that CseFilter::insLoad() also does this, so this will
            // only occur if CseFilter has been removed from the pipeline.
            return insLoad(op, ins2(LIR_piadd, base, insImmWord(d)), 0, accSet);
        }
    }

    LInsp LirBufWriter::insGuard(LOpcode op, LInsp c, GuardRecord *gr)
    {
        debug_only( if (LIR_x == op || LIR_xbarrier == op) NanoAssert(!c); )
        return ins2(op, c, (LIns*)gr);
    }

    LInsp LirBufWriter::insGuardXov(LOpcode op, LInsp a, LInsp b, GuardRecord *gr)
    {
        return ins3(op, a, b, (LIns*)gr);
    }

    LInsp LirBufWriter::insBranch(LOpcode op, LInsp condition, LInsp toLabel)
    {
        NanoAssert((op == LIR_j && !condition) ||
                   ((op == LIR_jf || op == LIR_jt) && condition));
        return ins2(op, condition, toLabel);
    }

    LIns* LirBufWriter::insJtbl(LIns* index, uint32_t size)
    {
        LInsJtbl* insJtbl = (LInsJtbl*) _buf->makeRoom(sizeof(LInsJtbl));
        LIns**    table   = new (_buf->_allocator) LIns*[size];
        LIns*     ins     = insJtbl->getLIns();
        VMPI_memset(table, 0, size * sizeof(LIns*));
        ins->initLInsJtbl(index, size, table);
        return ins;
    }

    LInsp LirBufWriter::insAlloc(int32_t size)
    {
        size = (size+3)>>2; // # of required 32bit words
        LInsI* insI = (LInsI*)_buf->makeRoom(sizeof(LInsI));
        LIns*  ins  = insI->getLIns();
        ins->initLInsI(LIR_alloc, size);
        return ins;
    }

    LInsp LirBufWriter::insParam(int32_t arg, int32_t kind)
    {
        LInsP* insP = (LInsP*)_buf->makeRoom(sizeof(LInsP));
        LIns*  ins  = insP->getLIns();
        ins->initLInsP(arg, kind);
        if (kind) {
            NanoAssert(arg < NumSavedRegs);
            _buf->savedRegs[arg] = ins;
        }
        return ins;
    }

    LInsp LirBufWriter::insImm(int32_t imm)
    {
        LInsI* insI = (LInsI*)_buf->makeRoom(sizeof(LInsI));
        LIns*  ins  = insI->getLIns();
        ins->initLInsI(LIR_int, imm);
        return ins;
    }

#ifdef NANOJIT_64BIT
    LInsp LirBufWriter::insImmq(uint64_t imm)
    {
        LInsN64* insN64 = (LInsN64*)_buf->makeRoom(sizeof(LInsN64));
        LIns*    ins    = insN64->getLIns();
        ins->initLInsN64(LIR_quad, imm);
        return ins;
    }
#endif

    LInsp LirBufWriter::insImmf(double d)
    {
        LInsN64* insN64 = (LInsN64*)_buf->makeRoom(sizeof(LInsN64));
        LIns*    ins    = insN64->getLIns();
        union {
            double d;
            uint64_t q;
        } u;
        u.d = d;
        ins->initLInsN64(LIR_float, u.q);
        return ins;
    }

    // Reads the next non-skip instruction.
    LInsp LirReader::read()
    {
        static const uint8_t insSizes[] = {
        // LIR_start is treated specially -- see below.
#define OP___(op, number, repKind, retType, isCse) \
            ((number) == LIR_start ? 0 : sizeof(LIns##repKind)),
#include "LIRopcode.tbl"
#undef OP___
            0
        };

        // Check the invariant: _ins never points to a skip.
        NanoAssert(_ins && !_ins->isop(LIR_skip));

        // Step back one instruction.  Use a table lookup rather than a switch
        // to avoid branch mispredictions.  LIR_start is given a special size
        // of zero so that we don't step back past the start of the block.
        // (Callers of this function should stop once they see a LIR_start.)
        LInsp ret = _ins;
        _ins = (LInsp)(uintptr_t(_ins) - insSizes[_ins->opcode()]);

        // Ensure _ins doesn't end up pointing to a skip.
        while (_ins->isop(LIR_skip)) {
            NanoAssert(_ins->prevLIns() != _ins);
            _ins = _ins->prevLIns();
        }

        return ret;
    }

    LOpcode f64arith_to_i32arith(LOpcode op)
    {
        switch (op) {
        case LIR_fneg:  return LIR_neg;
        case LIR_fadd:  return LIR_add;
        case LIR_fsub:  return LIR_sub;
        case LIR_fmul:  return LIR_mul;
        default:        NanoAssert(0); return LIR_skip;
        }
    }

#ifdef NANOJIT_64BIT
    LOpcode i32cmp_to_i64cmp(LOpcode op)
    {
        switch (op) {
        case LIR_eq:    return LIR_qeq;
        case LIR_lt:    return LIR_qlt;
        case LIR_gt:    return LIR_qgt;
        case LIR_le:    return LIR_qle;
        case LIR_ge:    return LIR_qge;
        case LIR_ult:   return LIR_qult;
        case LIR_ugt:   return LIR_qugt;
        case LIR_ule:   return LIR_qule;
        case LIR_uge:   return LIR_quge;
        default:        NanoAssert(0); return LIR_skip;
        }
    }
#endif

    LOpcode f64cmp_to_i32cmp(LOpcode op)
    {
        switch (op) {
        case LIR_feq:    return LIR_eq;
        case LIR_flt:    return LIR_lt;
        case LIR_fgt:    return LIR_gt;
        case LIR_fle:    return LIR_le;
        case LIR_fge:    return LIR_ge;
        default:        NanoAssert(0); return LIR_skip;
        }
    }

    LOpcode f64cmp_to_u32cmp(LOpcode op)
    {
        switch (op) {
        case LIR_feq:    return LIR_eq;
        case LIR_flt:    return LIR_ult;
        case LIR_fgt:    return LIR_ugt;
        case LIR_fle:    return LIR_ule;
        case LIR_fge:    return LIR_uge;
        default:        NanoAssert(0); return LIR_skip;
        }
    }

    // This is never called, but that's ok because it contains only static
    // assertions.
    void LIns::staticSanityCheck()
    {
        // LIns must be word-sized.
        NanoStaticAssert(sizeof(LIns) == 1*sizeof(void*));

        // LInsXYZ have expected sizes too.
        NanoStaticAssert(sizeof(LInsOp0) == 1*sizeof(void*));
        NanoStaticAssert(sizeof(LInsOp1) == 2*sizeof(void*));
        NanoStaticAssert(sizeof(LInsOp2) == 3*sizeof(void*));
        NanoStaticAssert(sizeof(LInsOp3) == 4*sizeof(void*));
        NanoStaticAssert(sizeof(LInsLd)  == 3*sizeof(void*));
        NanoStaticAssert(sizeof(LInsSti) == 4*sizeof(void*));
        NanoStaticAssert(sizeof(LInsSk)  == 2*sizeof(void*));
        NanoStaticAssert(sizeof(LInsC)   == 3*sizeof(void*));
        NanoStaticAssert(sizeof(LInsP)   == 2*sizeof(void*));
        NanoStaticAssert(sizeof(LInsI)   == 2*sizeof(void*));
    #if defined NANOJIT_64BIT
        NanoStaticAssert(sizeof(LInsN64) == 2*sizeof(void*));
    #else
        NanoStaticAssert(sizeof(LInsN64) == 3*sizeof(void*));
    #endif

        // oprnd_1 must be in the same position in LIns{Op1,Op2,Op3,Ld,Sti}
        // because oprnd1() is used for all of them.
        NanoStaticAssert( (offsetof(LInsOp1, ins) - offsetof(LInsOp1, oprnd_1)) ==
                          (offsetof(LInsOp2, ins) - offsetof(LInsOp2, oprnd_1)) );
        NanoStaticAssert( (offsetof(LInsOp2, ins) - offsetof(LInsOp2, oprnd_1)) ==
                          (offsetof(LInsOp3, ins) - offsetof(LInsOp3, oprnd_1)) );
        NanoStaticAssert( (offsetof(LInsOp3, ins) - offsetof(LInsOp3, oprnd_1)) ==
                          (offsetof(LInsLd,  ins) - offsetof(LInsLd,  oprnd_1)) );
        NanoStaticAssert( (offsetof(LInsLd,  ins) - offsetof(LInsLd,  oprnd_1)) ==
                          (offsetof(LInsSti, ins) - offsetof(LInsSti, oprnd_1)) );

        // oprnd_2 must be in the same position in LIns{Op2,Op3,Sti}
        // because oprnd2() is used for both of them.
        NanoStaticAssert( (offsetof(LInsOp2, ins) - offsetof(LInsOp2, oprnd_2)) ==
                          (offsetof(LInsOp3, ins) - offsetof(LInsOp3, oprnd_2)) );
        NanoStaticAssert( (offsetof(LInsOp3, ins) - offsetof(LInsOp3, oprnd_2)) ==
                          (offsetof(LInsSti, ins) - offsetof(LInsSti, oprnd_2)) );
    }

    bool insIsS16(LInsp i)
    {
        if (i->isconst()) {
            int c = i->imm32();
            return isS16(c);
        }
        if (i->isCmov()) {
            return insIsS16(i->oprnd2()) && insIsS16(i->oprnd3());
        }
        if (i->isCmp())
            return true;
        // many other possibilities too.
        return false;
    }

    LIns* ExprFilter::ins1(LOpcode v, LIns* oprnd)
    {
        switch (v) {
#ifdef NANOJIT_64BIT
        case LIR_q2i:
            if (oprnd->isconstq())
                return insImm(oprnd->imm64_0());
            break;
#endif
#if NJ_SOFTFLOAT_SUPPORTED
        case LIR_qlo:
            if (oprnd->isconstf())
                return insImm(oprnd->imm64_0());
            if (oprnd->isop(LIR_qjoin))
                return oprnd->oprnd1();
            break;
        case LIR_qhi:
            if (oprnd->isconstf())
                return insImm(oprnd->imm64_1());
            if (oprnd->isop(LIR_qjoin))
                return oprnd->oprnd2();
            break;
#endif
        case LIR_not:
            if (oprnd->isconst())
                return insImm(~oprnd->imm32());
        involution:
            if (v == oprnd->opcode())
                return oprnd->oprnd1();
            break;
        case LIR_neg:
            if (oprnd->isconst())
                return insImm(-oprnd->imm32());
            if (oprnd->isop(LIR_sub)) // -(a-b) = b-a
                return out->ins2(LIR_sub, oprnd->oprnd2(), oprnd->oprnd1());
            goto involution;
        case LIR_fneg:
            if (oprnd->isconstf())
                return insImmf(-oprnd->imm64f());
            if (oprnd->isop(LIR_fsub))
                return out->ins2(LIR_fsub, oprnd->oprnd2(), oprnd->oprnd1());
            goto involution;
        case LIR_i2f:
            if (oprnd->isconst())
                return insImmf(oprnd->imm32());
            break;
        case LIR_f2i:
            if (oprnd->isconstf())
                return insImm(int32_t(oprnd->imm64f()));
            break;
        case LIR_u2f:
            if (oprnd->isconst())
                return insImmf(uint32_t(oprnd->imm32()));
            break;
        default:
            ;
        }

        return out->ins1(v, oprnd);
    }

    // This is an ugly workaround for an apparent compiler
    // bug; in VC2008, compiling with optimization on
    // will produce spurious errors if this code is inlined
    // into ExprFilter::ins2(). See https://bugzilla.mozilla.org/show_bug.cgi?id=538504
    inline double do_join(int32_t c1, int32_t c2)
    {
        union {
            double d;
            uint64_t u64;
        } u;
        u.u64 = uint32_t(c1) | uint64_t(c2)<<32;
        return u.d;
    }

    LIns* ExprFilter::ins2(LOpcode v, LIns* oprnd1, LIns* oprnd2)
    {
        NanoAssert(oprnd1 && oprnd2);
        if (oprnd1 == oprnd2)
        {
            switch (v) {
            case LIR_xor:
            case LIR_sub:
            case LIR_ult:
            case LIR_ugt:
            case LIR_gt:
            case LIR_lt:
                return insImm(0);
            case LIR_or:
            case LIR_and:
                return oprnd1;
            case LIR_le:
            case LIR_ule:
            case LIR_ge:
            case LIR_uge:
                // x <= x == 1; x >= x == 1
                return insImm(1);
            default:
                ;
            }
        }
        if (oprnd1->isconst() && oprnd2->isconst())
        {
            int32_t c1 = oprnd1->imm32();
            int32_t c2 = oprnd2->imm32();
            double d;
            int32_t r;

            switch (v) {
#if NJ_SOFTFLOAT_SUPPORTED
            case LIR_qjoin:
                return insImmf(do_join(c1, c2));
#endif
            case LIR_eq:
                return insImm(c1 == c2);
            case LIR_lt:
                return insImm(c1 < c2);
            case LIR_gt:
                return insImm(c1 > c2);
            case LIR_le:
                return insImm(c1 <= c2);
            case LIR_ge:
                return insImm(c1 >= c2);
            case LIR_ult:
                return insImm(uint32_t(c1) < uint32_t(c2));
            case LIR_ugt:
                return insImm(uint32_t(c1) > uint32_t(c2));
            case LIR_ule:
                return insImm(uint32_t(c1) <= uint32_t(c2));
            case LIR_uge:
                return insImm(uint32_t(c1) >= uint32_t(c2));
            case LIR_rsh:
                return insImm(int32_t(c1) >> int32_t(c2));
            case LIR_lsh:
                return insImm(int32_t(c1) << int32_t(c2));
            case LIR_ush:
                return insImm(uint32_t(c1) >> int32_t(c2));
            case LIR_or:
                return insImm(uint32_t(c1) | int32_t(c2));
            case LIR_and:
                return insImm(uint32_t(c1) & int32_t(c2));
            case LIR_xor:
                return insImm(uint32_t(c1) ^ int32_t(c2));
            case LIR_add:
                d = double(c1) + double(c2);
            fold:
                r = int32_t(d);
                if (r == d)
                    return insImm(r);
                break;
            case LIR_sub:
                d = double(c1) - double(c2);
                goto fold;
            case LIR_mul:
                d = double(c1) * double(c2);
                goto fold;
            CASE86(LIR_div:)
            CASE86(LIR_mod:)
                #if defined NANOJIT_IA32 || defined NANOJIT_X64
                // We can't easily fold div and mod, since folding div makes it
                // impossible to calculate the mod that refers to it. The
                // frontend shouldn't emit div and mod with constant operands.
                NanoAssert(0);
                #endif
            default:
                ;
            }
        }
        else if (oprnd1->isconstf() && oprnd2->isconstf())
        {
            double c1 = oprnd1->imm64f();
            double c2 = oprnd2->imm64f();
            switch (v) {
            case LIR_feq:
                return insImm(c1 == c2);
            case LIR_flt:
                return insImm(c1 < c2);
            case LIR_fgt:
                return insImm(c1 > c2);
            case LIR_fle:
                return insImm(c1 <= c2);
            case LIR_fge:
                return insImm(c1 >= c2);
            case LIR_fadd:
                return insImmf(c1 + c2);
            case LIR_fsub:
                return insImmf(c1 - c2);
            case LIR_fmul:
                return insImmf(c1 * c2);
            case LIR_fdiv:
                return insImmf(c1 / c2);
            default:
                ;
            }
        }
        else if (oprnd1->isconst() && !oprnd2->isconst())
        {
            LIns* t;
            switch (v) {
            case LIR_add:
            case LIR_mul:
            case LIR_fadd:
            case LIR_fmul:
            case LIR_xor:
            case LIR_or:
            case LIR_and:
            case LIR_eq:
                // move const to rhs
                t = oprnd2;
                oprnd2 = oprnd1;
                oprnd1 = t;
                break;
            default:
                if (isICmpOpcode(v)) {
                    // move const to rhs, swap the operator
                    LIns *t = oprnd2;
                    oprnd2 = oprnd1;
                    oprnd1 = t;
                    v = invertICmpOpcode(v);
                }
                break;
            }
        }

        if (oprnd2->isconst())
        {
            int c = oprnd2->imm32();
            switch (v) {
            case LIR_add:
                if (oprnd1->isop(LIR_add) && oprnd1->oprnd2()->isconst()) {
                    // add(add(x,c1),c2) => add(x,c1+c2)
                    c += oprnd1->oprnd2()->imm32();
                    oprnd2 = insImm(c);
                    oprnd1 = oprnd1->oprnd1();
                }
                break;
            case LIR_sub:
                if (oprnd1->isop(LIR_add) && oprnd1->oprnd2()->isconst()) {
                    // sub(add(x,c1),c2) => add(x,c1-c2)
                    c = oprnd1->oprnd2()->imm32() - c;
                    oprnd2 = insImm(c);
                    oprnd1 = oprnd1->oprnd1();
                    v = LIR_add;
                }
                break;
            case LIR_rsh:
                if (c == 16 && oprnd1->isop(LIR_lsh) &&
                    oprnd1->oprnd2()->isconstval(16) &&
                    insIsS16(oprnd1->oprnd1())) {
                    // rsh(lhs(x,16),16) == x, if x is S16
                    return oprnd1->oprnd1();
                }
                break;
            default:
                ;
            }

            if (c == 0) {
                switch (v) {
                case LIR_add:
                case LIR_or:
                case LIR_xor:
                case LIR_sub:
                case LIR_lsh:
                case LIR_rsh:
                case LIR_ush:
                    return oprnd1;
                case LIR_and:
                case LIR_mul:
                    return oprnd2;
                case LIR_eq:
                    if (oprnd1->isop(LIR_or) &&
                        oprnd1->oprnd2()->isconst() &&
                        oprnd1->oprnd2()->imm32() != 0) {
                        // (x or c) != 0 if c != 0
                        return insImm(0);
                    }
                default:
                    ;
                }
            } else if (c == -1 || (c == 1 && oprnd1->isCmp())) {
                switch (v) {
                case LIR_or:
                    // x | -1 = -1, cmp | 1 = 1
                    return oprnd2;
                case LIR_and:
                    // x & -1 = x, cmp & 1 = cmp
                    return oprnd1;
                default:
                    ;
                }
            } else if (c == 1 && v == LIR_mul) {
                return oprnd1;
            }
        }

#if NJ_SOFTFLOAT_SUPPORTED
        LInsp ins;
        if (v == LIR_qjoin && oprnd1->isop(LIR_qlo) && oprnd2->isop(LIR_qhi) &&
            (ins = oprnd1->oprnd1()) == oprnd2->oprnd1()) {
            // qjoin(qlo(x),qhi(x)) == x
            return ins;
        }
#endif

        return out->ins2(v, oprnd1, oprnd2);
    }

    LIns* ExprFilter::ins3(LOpcode v, LIns* oprnd1, LIns* oprnd2, LIns* oprnd3)
    {
        NanoAssert(oprnd1 && oprnd2 && oprnd3);
        NanoAssert(isCmovOpcode(v));
        if (oprnd2 == oprnd3) {
            // c ? a : a => a
            return oprnd2;
        }
        if (oprnd1->isconst()) {
            // const ? x : y => return x or y depending on const
            return oprnd1->imm32() ? oprnd2 : oprnd3;
        }
        if (oprnd1->isop(LIR_eq) &&
            ((oprnd1->oprnd2() == oprnd2 && oprnd1->oprnd1() == oprnd3) ||
             (oprnd1->oprnd1() == oprnd2 && oprnd1->oprnd2() == oprnd3))) {
            // (y == x) ? x : y  =>  y
            // (x == y) ? x : y  =>  y
            return oprnd3;
        }

        return out->ins3(v, oprnd1, oprnd2, oprnd3);
    }

    LIns* ExprFilter::insGuard(LOpcode v, LInsp c, GuardRecord *gr)
    {
        if (v == LIR_xt || v == LIR_xf) {
            if (c->isconst()) {
                if ((v == LIR_xt && !c->imm32()) || (v == LIR_xf && c->imm32())) {
                    return 0; // no guard needed
                }
                else {
#ifdef JS_TRACER
                    // We're emitting a guard that will always fail. Any code
                    // emitted after this guard is dead code. We could
                    // silently optimize out the rest of the emitted code, but
                    // this could indicate a performance problem or other bug,
                    // so assert in debug builds.
                    NanoAssertMsg(0, "Constantly false guard detected");
#endif
                    return out->insGuard(LIR_x, NULL, gr);
                }
            }
            else {
                while (c->isop(LIR_eq) && c->oprnd1()->isCmp() &&
                    c->oprnd2()->isconstval(0)) {
                    // xt(eq(cmp,0)) => xf(cmp)   or   xf(eq(cmp,0)) => xt(cmp)
                    v = invertCondGuardOpcode(v);
                    c = c->oprnd1();
                }
            }
        }
        return out->insGuard(v, c, gr);
    }

    LIns* ExprFilter::insGuardXov(LOpcode op, LInsp oprnd1, LInsp oprnd2, GuardRecord *gr)
    {
        if (oprnd1->isconst() && oprnd2->isconst()) {
            int32_t c1 = oprnd1->imm32();
            int32_t c2 = oprnd2->imm32();
            double d = 0.0;

            switch (op) {
            case LIR_addxov:    d = double(c1) + double(c2);    break;
            case LIR_subxov:    d = double(c1) - double(c2);    break;
            case LIR_mulxov:    d = double(c1) * double(c2);    break;
            default:            NanoAssert(0);                  break;
            }
            int32_t r = int32_t(d);
            if (r == d)
                return insImm(r);

        } else if (oprnd1->isconst() && !oprnd2->isconst()) {
            switch (op) {
            case LIR_addxov:
            case LIR_mulxov: {
                // move const to rhs
                LIns* t = oprnd2;
                oprnd2 = oprnd1;
                oprnd1 = t;
                break;
            }
            case LIR_subxov:
                break;
            default:
                NanoAssert(0);
            }
        }

        if (oprnd2->isconst()) {
            int c = oprnd2->imm32();
            if (c == 0) {
                switch (op) {
                case LIR_addxov:
                case LIR_subxov:
                    return oprnd1;
                case LIR_mulxov:
                    return oprnd2;
                default:
                    ;
                }
            } else if (c == 1 && op == LIR_mulxov) {
                return oprnd1;
            }
        }

        return out->insGuardXov(op, oprnd1, oprnd2, gr);
    }

    LIns* ExprFilter::insBranch(LOpcode v, LIns *c, LIns *t)
    {
        switch (v) {
        case LIR_jt:
        case LIR_jf:
            while (c->isop(LIR_eq) && c->oprnd1()->isCmp() && c->oprnd2()->isconstval(0)) {
                // jt(eq(cmp,0)) => jf(cmp)   or   jf(eq(cmp,0)) => jt(cmp)
                v = invertCondJmpOpcode(v);
                c = c->oprnd1();
            }
            break;
        default:
            ;
        }
        return out->insBranch(v, c, t);
    }

    LIns* ExprFilter::insLoad(LOpcode op, LIns* base, int32_t off, AccSet accSet) {
        if (base->isconstp() && !isS8(off)) {
            // if the effective address is constant, then transform:
            // ld const[bigconst] => ld (const+bigconst)[0]
            // note: we don't do this optimization for <8bit field offsets,
            // under the assumption that we're more likely to CSE-match the
            // constant base address if we dont const-fold small offsets.
            uintptr_t p = (uintptr_t)base->constvalp() + off;
            return out->insLoad(op, insImmPtr((void*)p), 0, accSet);
        }
        return out->insLoad(op, base, off, accSet);
    }

    LIns* LirWriter::insStorei(LIns* value, LIns* base, int32_t d, AccSet accSet)
    {
        // Determine which kind of store should be used for 'value' based on
        // its type.
        LOpcode op = LOpcode(0);
        switch (value->retType()) {
        case LTy_I32:   op = LIR_sti;   break;
#ifdef NANOJIT_64BIT
        case LTy_I64:   op = LIR_stqi;  break;
#endif
        case LTy_F64:   op = LIR_stfi;  break;
        case LTy_Void:  NanoAssert(0);  break;
        default:        NanoAssert(0);  break;
        }
        return insStore(op, value, base, d, accSet);
    }

    LIns* LirWriter::ins_choose(LIns* cond, LIns* iftrue, LIns* iffalse, bool use_cmov)
    {
        // 'cond' must be a conditional, unless it has been optimized to 0 or
        // 1.  In that case make it an ==0 test and flip the branches.  It'll
        // get constant-folded by ExprFilter subsequently.
        if (!cond->isCmp()) {
            NanoAssert(cond->isconst());
            cond = ins_eq0(cond);
            LInsp tmp = iftrue;
            iftrue = iffalse;
            iffalse = tmp;
        }

        if (use_cmov) {
            LOpcode op = LIR_cmov;
            if (iftrue->isI32() && iffalse->isI32()) {
                op = LIR_cmov;
#ifdef NANOJIT_64BIT
            } else if (iftrue->isI64() && iffalse->isI64()) {
                op = LIR_qcmov;
#endif
            } else if (iftrue->isF64() && iffalse->isF64()) {
                NanoAssertMsg(0, "LIR_fcmov doesn't exist yet, sorry");
            } else {
                NanoAssert(0);  // type error
            }
            return ins3(op, cond, iftrue, iffalse);
        }

        LInsp ncond = ins1(LIR_neg, cond); // cond ? -1 : 0
        return ins2(LIR_or,
                    ins2(LIR_and, iftrue, ncond),
                    ins2(LIR_and, iffalse, ins1(LIR_not, ncond)));
    }

    LIns* LirBufWriter::insCall(const CallInfo *ci, LInsp args[])
    {
        LOpcode op = getCallOpcode(ci);
#if NJ_SOFTFLOAT_SUPPORTED
        // SoftFloat: convert LIR_fcall to LIR_icall.
        if (_config.soft_float && op == LIR_fcall)
            op = LIR_icall;
#endif

        int32_t argc = ci->count_args();
        NanoAssert(argc <= (int)MAXARGS);

        // Allocate space for and copy the arguments.  We use the same
        // allocator as the normal LIR buffers so it has the same lifetime.
        // Nb: this must be kept in sync with arg().
        LInsp* args2 = (LInsp*)_buf->_allocator.alloc(argc * sizeof(LInsp));
        memcpy(args2, args, argc * sizeof(LInsp));

        // Allocate and write the call instruction.
        LInsC* insC = (LInsC*)_buf->makeRoom(sizeof(LInsC));
        LIns*  ins  = insC->getLIns();
        ins->initLInsC(op, args2, ci);
        return ins;
    }

    using namespace avmplus;

    StackFilter::StackFilter(LirFilter *in, Allocator& alloc, LInsp sp)
        : LirFilter(in), sp(sp), stk(alloc), top(0)
    {}

    // If we see a sequence like this:
    //
    //   sti sp[0]
    //   ...
    //   sti sp[0]
    //
    // where '...' contains no guards, we can remove the first store.  Also,
    // because stack entries are eight bytes each (we check this), if we have
    // this:
    //
    //   stfi sp[0]
    //   ...
    //   sti sp[0]
    //
    // we can again remove the first store -- even though the second store
    // doesn't clobber the high four bytes -- because we know the entire value
    // stored by the first store is dead.
    //
    LInsp StackFilter::read()
    {
        for (;;) {
            LInsp ins = in->read();

            if (ins->isStore()) {
                LInsp base = ins->oprnd2();
                if (base == sp) {
                    // 'disp' must be eight-aligned because each stack entry is 8 bytes.
                    NanoAssert((ins->disp() & 0x7) == 0);

                    int d = ins->disp() >> 3;
                    if (d >= top) {
                        continue;
                    } else {
                        d = top - d;
                        if (stk.get(d)) {
                            continue;
                        } else {
                            stk.set(d);
                        }
                    }
                }
            }
            /*
             * NB: If there is a backward branch other than the loop-restart branch, this is
             * going to be wrong. Unfortunately there doesn't seem to be an easy way to detect
             * such branches. Just do not create any.
             */
            else if (ins->isGuard()) {
                stk.reset();
                top = getTop(ins);
                top >>= 3;
            }

            return ins;
        }
    }

    //
    // inlined/separated version of SuperFastHash
    // This content is copyrighted by Paul Hsieh, For reference see : http://www.azillionmonkeys.com/qed/hash.html
    //
    inline uint32_t _hash8(uint32_t hash, const uint8_t data)
    {
        hash += data;
        hash ^= hash << 10;
        hash += hash >> 1;
        return hash;
    }

    inline uint32_t _hash32(uint32_t hash, const uint32_t data)
    {
        const uint32_t dlo = data & 0xffff;
        const uint32_t dhi = data >> 16;
        hash += dlo;
        const uint32_t tmp = (dhi << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        hash += hash >> 11;
        return hash;
    }

    inline uint32_t _hashptr(uint32_t hash, const void* data)
    {
#ifdef NANOJIT_64BIT
        hash = _hash32(hash, uint32_t(uintptr_t(data) >> 32));
        hash = _hash32(hash, uint32_t(uintptr_t(data)));
        return hash;
#else
        return _hash32(hash, uint32_t(data));
#endif
    }

    inline uint32_t _hashfinish(uint32_t hash)
    {
        /* Force "avalanching" of final 127 bits */
        hash ^= hash << 3;
        hash += hash >> 5;
        hash ^= hash << 4;
        hash += hash >> 17;
        hash ^= hash << 25;
        hash += hash >> 6;
        return hash;
    }

    LInsHashSet::LInsHashSet(Allocator& alloc, uint32_t kInitialCaps[]) : alloc(alloc)
    {
        for (LInsHashKind kind = LInsFirst; kind <= LInsLast; kind = nextKind(kind)) {
            m_cap[kind] = kInitialCaps[kind];
            m_list[kind] = new (alloc) LInsp[m_cap[kind]];
        }
        clear();
        m_find[LInsImm]          = &LInsHashSet::findImm;
        m_find[LInsImmq]         = PTR_SIZE(NULL, &LInsHashSet::findImmq);
        m_find[LInsImmf]         = &LInsHashSet::findImmf;
        m_find[LIns1]            = &LInsHashSet::find1;
        m_find[LIns2]            = &LInsHashSet::find2;
        m_find[LIns3]            = &LInsHashSet::find3;
        m_find[LInsCall]         = &LInsHashSet::findCall;
        m_find[LInsLoadReadOnly] = &LInsHashSet::findLoadReadOnly;
        m_find[LInsLoadStack]    = &LInsHashSet::findLoadStack;
        m_find[LInsLoadRStack]   = &LInsHashSet::findLoadRStack;
        m_find[LInsLoadOther]    = &LInsHashSet::findLoadOther;
        m_find[LInsLoadMultiple] = &LInsHashSet::findLoadMultiple;
    }

    void LInsHashSet::clear(LInsHashKind kind) {
        VMPI_memset(m_list[kind], 0, sizeof(LInsp)*m_cap[kind]);
        m_used[kind] = 0;
    }

    void LInsHashSet::clear() {
        for (LInsHashKind kind = LInsFirst; kind <= LInsLast; kind = nextKind(kind)) {
            clear(kind);
        }
    }

    inline uint32_t LInsHashSet::hashImm(int32_t a) {
        return _hashfinish(_hash32(0,a));
    }

    inline uint32_t LInsHashSet::hashImmq(uint64_t a) {
        uint32_t hash = _hash32(0, uint32_t(a >> 32));
        return _hashfinish(_hash32(hash, uint32_t(a)));
    }

    inline uint32_t LInsHashSet::hash1(LOpcode op, LInsp a) {
        uint32_t hash = _hash8(0,uint8_t(op));
        return _hashfinish(_hashptr(hash, a));
    }

    inline uint32_t LInsHashSet::hash2(LOpcode op, LInsp a, LInsp b) {
        uint32_t hash = _hash8(0,uint8_t(op));
        hash = _hashptr(hash, a);
        return _hashfinish(_hashptr(hash, b));
    }

    inline uint32_t LInsHashSet::hash3(LOpcode op, LInsp a, LInsp b, LInsp c) {
        uint32_t hash = _hash8(0,uint8_t(op));
        hash = _hashptr(hash, a);
        hash = _hashptr(hash, b);
        return _hashfinish(_hashptr(hash, c));
    }

    NanoStaticAssert(sizeof(AccSet) == 1);  // required for hashLoad to work properly

    // Nb: no need to hash the load's AccSet because each region's loads go in
    // a different hash table.
    inline uint32_t LInsHashSet::hashLoad(LOpcode op, LInsp a, int32_t d, AccSet accSet) {
        uint32_t hash = _hash8(0,uint8_t(op));
        hash = _hashptr(hash, a);
        hash = _hash32(hash, d);
        return _hashfinish(_hash8(hash, accSet));
    }

    inline uint32_t LInsHashSet::hashCall(const CallInfo *ci, uint32_t argc, LInsp args[]) {
        uint32_t hash = _hashptr(0, ci);
        for (int32_t j=argc-1; j >= 0; j--)
            hash = _hashptr(hash,args[j]);
        return _hashfinish(hash);
    }

    void LInsHashSet::grow(LInsHashKind kind)
    {
        const uint32_t oldcap = m_cap[kind];
        m_cap[kind] <<= 1;
        LInsp *oldlist = m_list[kind];
        m_list[kind] = new (alloc) LInsp[m_cap[kind]];
        VMPI_memset(m_list[kind], 0, m_cap[kind] * sizeof(LInsp));
        find_t find = m_find[kind];
        for (uint32_t i = 0; i < oldcap; i++) {
            LInsp ins = oldlist[i];
            if (!ins) continue;
            uint32_t j = (this->*find)(ins);
            NanoAssert(!m_list[kind][j]);
            m_list[kind][j] = ins;
        }
    }

    void LInsHashSet::add(LInsHashKind kind, LInsp ins, uint32_t k)
    {
        NanoAssert(!m_list[kind][k]);
        m_used[kind]++;
        m_list[kind][k] = ins;
        if ((m_used[kind] * 4) >= (m_cap[kind] * 3)) {  // load factor of 0.75
            grow(kind);
        }
    }

    LInsp LInsHashSet::findImm(int32_t a, uint32_t &k)
    {
        LInsHashKind kind = LInsImm;
        const uint32_t bitmask = m_cap[kind] - 1;
        k = hashImm(a) & bitmask;
        uint32_t n = 1;
        while (true) {
            LInsp ins = m_list[kind][k];
            if (!ins)
                return NULL;
            NanoAssert(ins->isconst());
            if (ins->imm32() == a)
                return ins;
            // Quadratic probe:  h(k,i) = h(k) + 0.5i + 0.5i^2, which gives the
            // sequence h(k), h(k)+1, h(k)+3, h(k)+6, h+10, ...  This is a
            // good sequence for 2^n-sized tables as the values h(k,i) for i
            // in [0,m − 1] are all distinct so termination is guaranteed.
            // See http://portal.acm.org/citation.cfm?id=360737 and
            // http://en.wikipedia.org/wiki/Quadratic_probing (fetched
            // 06-Nov-2009) for more details.
            k = (k + n) & bitmask;
            n += 1;
        }
    }

    uint32_t LInsHashSet::findImm(LInsp ins)
    {
        uint32_t k;
        findImm(ins->imm32(), k);
        return k;
    }

#ifdef NANOJIT_64BIT
    LInsp LInsHashSet::findImmq(uint64_t a, uint32_t &k)
    {
        LInsHashKind kind = LInsImmq;
        const uint32_t bitmask = m_cap[kind] - 1;
        k = hashImmq(a) & bitmask;
        uint32_t n = 1;
        while (true) {
            LInsp ins = m_list[kind][k];
            if (!ins)
                return NULL;
            NanoAssert(ins->isconstq());
            if (ins->imm64() == a)
                return ins;
            k = (k + n) & bitmask;
            n += 1;
        }
    }

    uint32_t LInsHashSet::findImmq(LInsp ins)
    {
        uint32_t k;
        findImmq(ins->imm64(), k);
        return k;
    }
#endif

    LInsp LInsHashSet::findImmf(uint64_t a, uint32_t &k)
    {
        LInsHashKind kind = LInsImmf;
        const uint32_t bitmask = m_cap[kind] - 1;
        k = hashImmq(a) & bitmask;
        uint32_t n = 1;
        while (true) {
            LInsp ins = m_list[kind][k];
            if (!ins)
                return NULL;
            NanoAssert(ins->isconstf());
            if (ins->imm64() == a)
                return ins;
            k = (k + n) & bitmask;
            n += 1;
        }
    }

    uint32_t LInsHashSet::findImmf(LInsp ins)
    {
        uint32_t k;
        findImmf(ins->imm64(), k);
        return k;
    }

    LInsp LInsHashSet::find1(LOpcode op, LInsp a, uint32_t &k)
    {
        LInsHashKind kind = LIns1;
        const uint32_t bitmask = m_cap[kind] - 1;
        k = hash1(op, a) & bitmask;
        uint32_t n = 1;
        while (true) {
            LInsp ins = m_list[kind][k];
            if (!ins)
                return NULL;
            if (ins->isop(op) && ins->oprnd1() == a)
                return ins;
            k = (k + n) & bitmask;
            n += 1;
        }
    }

    uint32_t LInsHashSet::find1(LInsp ins)
    {
        uint32_t k;
        find1(ins->opcode(), ins->oprnd1(), k);
        return k;
    }

    LInsp LInsHashSet::find2(LOpcode op, LInsp a, LInsp b, uint32_t &k)
    {
        LInsHashKind kind = LIns2;
        const uint32_t bitmask = m_cap[kind] - 1;
        k = hash2(op, a, b) & bitmask;
        uint32_t n = 1;
        while (true) {
            LInsp ins = m_list[kind][k];
            if (!ins)
                return NULL;
            if (ins->isop(op) && ins->oprnd1() == a && ins->oprnd2() == b)
                return ins;
            k = (k + n) & bitmask;
            n += 1;
        }
    }

    uint32_t LInsHashSet::find2(LInsp ins)
    {
        uint32_t k;
        find2(ins->opcode(), ins->oprnd1(), ins->oprnd2(), k);
        return k;
    }

    LInsp LInsHashSet::find3(LOpcode op, LInsp a, LInsp b, LInsp c, uint32_t &k)
    {
        LInsHashKind kind = LIns3;
        const uint32_t bitmask = m_cap[kind] - 1;
        k = hash3(op, a, b, c) & bitmask;
        uint32_t n = 1;
        while (true) {
            LInsp ins = m_list[kind][k];
            if (!ins)
                return NULL;
            if (ins->isop(op) && ins->oprnd1() == a && ins->oprnd2() == b && ins->oprnd3() == c)
                return ins;
            k = (k + n) & bitmask;
            n += 1;
        }
    }

    uint32_t LInsHashSet::find3(LInsp ins)
    {
        uint32_t k;
        find3(ins->opcode(), ins->oprnd1(), ins->oprnd2(), ins->oprnd3(), k);
        return k;
    }

    LInsp LInsHashSet::findLoad(LOpcode op, LInsp a, int32_t d, AccSet accSet, LInsHashKind kind,
                                uint32_t &k)
    {
        (void)accSet;
        const uint32_t bitmask = m_cap[kind] - 1;
        k = hashLoad(op, a, d, accSet) & bitmask;
        uint32_t n = 1;
        while (true) {
            LInsp ins = m_list[kind][k];
            if (!ins)
                return NULL;
            NanoAssert(ins->accSet() == accSet);
            if (ins->isop(op) && ins->oprnd1() == a && ins->disp() == d)
                return ins;
            k = (k + n) & bitmask;
            n += 1;
        }
    }

    uint32_t LInsHashSet::findLoadReadOnly(LInsp ins)
    {
        uint32_t k;
        findLoad(ins->opcode(), ins->oprnd1(), ins->disp(), ins->accSet(), LInsLoadReadOnly, k);
        return k;
    }

    uint32_t LInsHashSet::findLoadStack(LInsp ins)
    {
        uint32_t k;
        findLoad(ins->opcode(), ins->oprnd1(), ins->disp(), ins->accSet(), LInsLoadStack, k);
        return k;
    }

    uint32_t LInsHashSet::findLoadRStack(LInsp ins)
    {
        uint32_t k;
        findLoad(ins->opcode(), ins->oprnd1(), ins->disp(), ins->accSet(), LInsLoadRStack, k);
        return k;
    }

    uint32_t LInsHashSet::findLoadOther(LInsp ins)
    {
        uint32_t k;
        findLoad(ins->opcode(), ins->oprnd1(), ins->disp(), ins->accSet(), LInsLoadOther, k);
        return k;
    }

    uint32_t LInsHashSet::findLoadMultiple(LInsp ins)
    {
        uint32_t k;
        findLoad(ins->opcode(), ins->oprnd1(), ins->disp(), ins->accSet(), LInsLoadMultiple, k);
        return k;
    }

    bool argsmatch(LInsp ins, uint32_t argc, LInsp args[])
    {
        for (uint32_t j=0; j < argc; j++)
            if (ins->arg(j) != args[j])
                return false;
        return true;
    }

    LInsp LInsHashSet::findCall(const CallInfo *ci, uint32_t argc, LInsp args[], uint32_t &k)
    {
        LInsHashKind kind = LInsCall;
        const uint32_t bitmask = m_cap[kind] - 1;
        k = hashCall(ci, argc, args) & bitmask;
        uint32_t n = 1;
        while (true) {
            LInsp ins = m_list[kind][k];
            if (!ins)
                return NULL;
            if (ins->isCall() && ins->callInfo() == ci && argsmatch(ins, argc, args))
                return ins;
            k = (k + n) & bitmask;
            n += 1;
        }
    }

    uint32_t LInsHashSet::findCall(LInsp ins)
    {
        LInsp args[MAXARGS];
        uint32_t argc = ins->argc();
        NanoAssert(argc < MAXARGS);
        for (uint32_t j=0; j < argc; j++)
            args[j] = ins->arg(j);
        uint32_t k;
        findCall(ins->callInfo(), argc, args, k);
        return k;
    }

#ifdef NJ_VERBOSE
    class RetiredEntry
    {
    public:
        Seq<LIns*>* live;
        LIns* i;
        RetiredEntry(): live(NULL), i(NULL) {}
    };

    class LiveTable
    {
        Allocator& alloc;
    public:
        HashMap<LIns*, LIns*> live;
        SeqBuilder<RetiredEntry*> retired;
        int retiredCount;
        int maxlive;
        LiveTable(Allocator& alloc)
            : alloc(alloc)
            , live(alloc)
            , retired(alloc)
            , retiredCount(0)
            , maxlive(0)
        { }

        void add(LInsp ins, LInsp use) {
            if (!ins->isImmAny() && !live.containsKey(ins)) {
                NanoAssert(size_t(ins->opcode()) < sizeof(lirNames) / sizeof(lirNames[0]));
                live.put(ins,use);
            }
        }

        void retire(LInsp i) {
            RetiredEntry *e = new (alloc) RetiredEntry();
            e->i = i;
            SeqBuilder<LIns*> livelist(alloc);
            HashMap<LIns*, LIns*>::Iter iter(live);
            int live_count = 0;
            while (iter.next()) {
                LIns* ins = iter.key();
                if (!ins->isStore() && !ins->isGuard()) {
                    live_count++;
                    livelist.insert(ins);
                }
            }
            e->live = livelist.get();
            if (live_count > maxlive)
                maxlive = live_count;

            live.remove(i);
            retired.insert(e);
            retiredCount++;
        }

        bool contains(LInsp i) {
            return live.containsKey(i);
        }
    };

    /*
     * traverse the LIR buffer and discover which instructions are live
     * by starting from instructions with side effects (stores, calls, branches)
     * and marking instructions used by them.  Works bottom-up, in one pass.
     * if showLiveRefs == true, also print the set of live expressions next to
     * each instruction
     */
    void live(LirFilter* in, Allocator& alloc, Fragment *frag, LogControl *logc)
    {
        // traverse backwards to find live exprs and a few other stats.

        LiveTable live(alloc);
        uint32_t exits = 0;
        int total = 0;
        if (frag->lirbuf->state)
            live.add(frag->lirbuf->state, in->finalIns());
        for (LInsp ins = in->read(); !ins->isop(LIR_start); ins = in->read())
        {
            total++;

            // first handle side-effect instructions
            if (ins->isStmt())
            {
                live.add(ins, 0);
                if (ins->isGuard())
                    exits++;
            }

            // now propagate liveness
            if (live.contains(ins))
            {
                live.retire(ins);

                switch (ins->opcode()) {
                case LIR_skip:
                    NanoAssertMsg(0, "Shouldn't see LIR_skip");
                    break;

                case LIR_start:
                case LIR_regfence:
                case LIR_param:
                case LIR_x:
                case LIR_xbarrier:
                case LIR_j:
                case LIR_label:
                case LIR_int:
                CASE64(LIR_quad:)
                case LIR_float:
                case LIR_alloc:
                    // No operands, do nothing.
                    break;

                case LIR_ld:
                CASE64(LIR_ldq:)
                case LIR_ldf:
                case LIR_ldzb:
                case LIR_ldzs:
                case LIR_ldsb:
                case LIR_ldss:
                case LIR_ld32f:
                case LIR_ret:
                CASE64(LIR_qret:)
                case LIR_fret:
                case LIR_live:
                CASE64(LIR_qlive:)
                case LIR_flive:
                case LIR_xt:
                case LIR_xf:
                case LIR_xtbl:
                case LIR_jt:
                case LIR_jf:
                case LIR_jtbl:
                case LIR_neg:
                case LIR_fneg:
                case LIR_not:
                CASESF(LIR_qlo:)
                CASESF(LIR_qhi:)
                CASE64(LIR_i2q:)
                CASE64(LIR_u2q:)
                case LIR_i2f:
                case LIR_u2f:
                CASE64(LIR_q2i:)
                case LIR_f2i:
                CASE86(LIR_mod:)
                    live.add(ins->oprnd1(), ins);
                    break;

                case LIR_sti:
                CASE64(LIR_stqi:)
                case LIR_stfi:
                case LIR_stb:
                case LIR_sts:
                case LIR_st32f:
                case LIR_eq:
                case LIR_lt:
                case LIR_gt:
                case LIR_le:
                case LIR_ge:
                case LIR_ult:
                case LIR_ugt:
                case LIR_ule:
                case LIR_uge:
                case LIR_feq:
                case LIR_flt:
                case LIR_fgt:
                case LIR_fle:
                case LIR_fge:
                CASE64(LIR_qeq:)
                CASE64(LIR_qlt:)
                CASE64(LIR_qgt:)
                CASE64(LIR_qle:)
                CASE64(LIR_qge:)
                CASE64(LIR_qult:)
                CASE64(LIR_qugt:)
                CASE64(LIR_qule:)
                CASE64(LIR_quge:)
                case LIR_lsh:
                case LIR_rsh:
                case LIR_ush:
                CASE64(LIR_qilsh:)
                CASE64(LIR_qirsh:)
                CASE64(LIR_qursh:)
                case LIR_add:
                case LIR_sub:
                case LIR_mul:
                case LIR_addxov:
                case LIR_subxov:
                case LIR_mulxov:
                CASE86(LIR_div:)
                case LIR_fadd:
                case LIR_fsub:
                case LIR_fmul:
                case LIR_fdiv:
                CASE64(LIR_qiadd:)
                case LIR_and:
                case LIR_or:
                case LIR_xor:
                CASE64(LIR_qiand:)
                CASE64(LIR_qior:)
                CASE64(LIR_qxor:)
                CASESF(LIR_qjoin:)
                case LIR_file:
                case LIR_line:
                    live.add(ins->oprnd1(), ins);
                    live.add(ins->oprnd2(), ins);
                    break;

                case LIR_cmov:
                CASE64(LIR_qcmov:)
                    live.add(ins->oprnd1(), ins);
                    live.add(ins->oprnd2(), ins);
                    live.add(ins->oprnd3(), ins);
                    break;

                case LIR_icall:
                case LIR_fcall:
                CASE64(LIR_qcall:)
                    for (int i = 0, argc = ins->argc(); i < argc; i++)
                        live.add(ins->arg(i), ins);
                    break;

#if NJ_SOFTFLOAT_SUPPORTED
                case LIR_callh:
                    live.add(ins->oprnd1(), ins);
                    break;
#endif

                default:
                    NanoAssertMsgf(0, "unhandled opcode: %d", ins->opcode());
                    break;
                }
            }
        }

        logc->printf("  Live instruction count %d, total %u, max pressure %d\n",
                     live.retiredCount, total, live.maxlive);
        if (exits > 0)
            logc->printf("  Side exits %u\n", exits);
        logc->printf("  Showing LIR instructions with live-after variables\n");
        logc->printf("\n");

        // print live exprs, going forwards
        LInsPrinter *printer = frag->lirbuf->printer;
        bool newblock = true;
        for (Seq<RetiredEntry*>* p = live.retired.get(); p != NULL; p = p->tail) {
            RetiredEntry* e = p->head;
            InsBuf ib;
            RefBuf rb;
            char livebuf[4000], *s=livebuf;
            *s = 0;
            if (!newblock && e->i->isop(LIR_label)) {
                logc->printf("\n");
            }
            newblock = false;
            for (Seq<LIns*>* p = e->live; p != NULL; p = p->tail) {
                VMPI_strcpy(s, printer->formatRef(&rb, p->head));
                s += VMPI_strlen(s);
                *s++ = ' '; *s = 0;
                NanoAssert(s < livebuf+sizeof(livebuf));
            }
            /* If the LIR insn is pretty short, print it and its
               live-after set on the same line.  If not, put
               live-after set on a new line, suitably indented. */
            const char* insn_text = printer->formatIns(&ib, e->i);
            if (VMPI_strlen(insn_text) >= 30-2) {
                logc->printf("  %-30s\n  %-30s %s\n", insn_text, "", livebuf);
            } else {
                logc->printf("  %-30s %s\n", insn_text, livebuf);
            }

            if (e->i->isGuard() || e->i->isBranch() || e->i->isRet()) {
                logc->printf("\n");
                newblock = true;
            }
        }
    }

    void LirNameMap::addNameWithSuffix(LInsp ins, const char *name, int suffix,
                                       bool ignoreOneSuffix) {
        // The lookup may succeed, ie. we may already have a name for this
        // instruction.  This can happen because of CSE.  Eg. if we have this:
        //
        //   ins = addName("foo", insImm(0))
        //
        // that assigns the name "foo1" to 'ins'.  If we later do this:
        //
        //   ins2 = addName("foo", insImm(0))
        //
        // then CSE will cause 'ins' and 'ins2' to be equal.  So 'ins2'
        // already has a name ("foo1") and there's no need to generate a new
        // name "foo2".
        //
        if (!names.containsKey(ins)) {
            const int N = 100;
            char name2[N];
            if (suffix == 1 && ignoreOneSuffix) {
                VMPI_snprintf(name2, N, "%s", name);                // don't add '1' suffix
            } else if (VMPI_isdigit(name[VMPI_strlen(name)-1])) {
                VMPI_snprintf(name2, N, "%s_%d", name, suffix);     // use '_' to avoid confusion
            } else {
                VMPI_snprintf(name2, N, "%s%d", name, suffix);      // normal case
            }

            char *copy = new (alloc) char[VMPI_strlen(name2)+1];
            VMPI_strcpy(copy, name2);
            Entry *e = new (alloc) Entry(copy);
            names.put(ins, e);
        }
    }

    void LirNameMap::addName(LInsp ins, const char* name) {
        addNameWithSuffix(ins, name, namecounts.add(name), /*ignoreOneSuffix*/true);
    }

    const char* LirNameMap::createName(LInsp ins) {
        if (ins->isCall()) {
#if NJ_SOFTFLOAT_SUPPORTED
            if (ins->isop(LIR_callh)) {
                ins = ins->oprnd1();    // we've presumably seen the other half already
            } else
#endif
            {
                addNameWithSuffix(ins, ins->callInfo()->_name, funccounts.add(ins->callInfo()),
                                  /*ignoreOneSuffix*/false);
            }
        } else {
            addNameWithSuffix(ins, lirNames[ins->opcode()], lircounts.add(ins->opcode()),
                              /*ignoreOneSuffix*/false);

        }
        return names.get(ins)->name;
    }

    const char* LirNameMap::lookupName(LInsp ins)
    {
        Entry* e = names.get(ins);
        return e ? e->name : NULL;
    }


    char* LInsPrinter::formatAccSet(RefBuf* buf, AccSet accSet) {
        int i = 0;
        // 'c' is short for "const", because 'r' is used for RSTACK.
        if (accSet & ACC_READONLY) { buf->buf[i++] = 'c'; accSet &= ~ACC_READONLY; }
        if (accSet & ACC_STACK)    { buf->buf[i++] = 's'; accSet &= ~ACC_STACK; }
        if (accSet & ACC_RSTACK)   { buf->buf[i++] = 'r'; accSet &= ~ACC_RSTACK; }
        if (accSet & ACC_OTHER)    { buf->buf[i++] = 'o'; accSet &= ~ACC_OTHER; }
        // This assertion will fail if we add a new accSet value but
        // forget to handle it here.
        NanoAssert(accSet == 0);
        buf->buf[i] = 0;
        NanoAssert(size_t(i) < buf->len);
        return buf->buf;
    }

    void LInsPrinter::formatImm(RefBuf* buf, int32_t c) {
        if (-10000 < c || c < 10000) {
            VMPI_snprintf(buf->buf, buf->len, "%d", c);
        } else {
#if !defined NANOJIT_64BIT
            formatAddr(buf, (void*)c);
#else
            VMPI_snprintf(buf->buf, buf->len, "0x%x", (unsigned int)c);
#endif
        }
    }

    void LInsPrinter::formatImmq(RefBuf* buf, uint64_t c) {
        if (-10000 < (int64_t)c || c < 10000) {
            VMPI_snprintf(buf->buf, buf->len, "%dLL", (int)c);
        } else {
#if defined NANOJIT_64BIT
            formatAddr(buf, (void*)c);
#else
            VMPI_snprintf(buf->buf, buf->len, "0x%llxLL", c);
#endif
        }
    }

    char* LInsPrinter::formatAddr(RefBuf* buf, void* p)
    {
        char*   name;
        int32_t offset;
        addrNameMap->lookupAddr(p, name, offset);

        if (name) {
            if (offset != 0) {
                VMPI_snprintf(buf->buf, buf->len, "%p %s+%d", p, name, offset);
            } else {
                VMPI_snprintf(buf->buf, buf->len, "%p %s", p, name);
            }
        } else {
            VMPI_snprintf(buf->buf, buf->len, "%p", p);
        }

        return buf->buf;
    }

    char* LInsPrinter::formatRef(RefBuf* buf, LIns *ref)
    {
        // - If 'ref' already has a name, use it.
        // - Otherwise, if it's a constant, use the constant.
        // - Otherwise, give it a name and use it.
        const char* name = lirNameMap->lookupName(ref);
        if (name) {
            VMPI_snprintf(buf->buf, buf->len, "%s", name);
        }
        else if (ref->isconst()) {
            formatImm(buf, ref->imm32());
        }
#ifdef NANOJIT_64BIT
        else if (ref->isconstq()) {
            formatImmq(buf, ref->imm64());
        }
#endif
        else if (ref->isconstf()) {
            VMPI_snprintf(buf->buf, buf->len, "%g", ref->imm64f());
        }
        else {
            name = lirNameMap->createName(ref);
            VMPI_snprintf(buf->buf, buf->len, "%s", name);
        }
        return buf->buf;
    }

    char* LInsPrinter::formatIns(InsBuf* buf, LIns* i)
    {
        char *s = buf->buf;
        size_t n = buf->len;
        RefBuf b1, b2, b3, b4;
        LOpcode op = i->opcode();
        switch (op)
        {
            case LIR_int:
                VMPI_snprintf(s, n, "%s = %s %d", formatRef(&b1, i), lirNames[op], i->imm32());
                break;

            case LIR_alloc:
                VMPI_snprintf(s, n, "%s = %s %d", formatRef(&b1, i), lirNames[op], i->size());
                break;

#ifdef NANOJIT_64BIT
            case LIR_quad:
                VMPI_snprintf(s, n, "%s = %s %X:%X", formatRef(&b1, i), lirNames[op],
                             i->imm64_1(), i->imm64_0());
                break;
#endif

            case LIR_float:
                VMPI_snprintf(s, n, "%s = %s %g", formatRef(&b1, i), lirNames[op], i->imm64f());
                break;

            case LIR_start:
            case LIR_regfence:
                VMPI_snprintf(s, n, "%s", lirNames[op]);
                break;

            case LIR_icall:
            case LIR_fcall:
            CASE64(LIR_qcall:) {
                const CallInfo* call = i->callInfo();
                int32_t argc = i->argc();
                int32_t m = int32_t(n);     // Windows doesn't have 'ssize_t'
                if (call->isIndirect())
                    m -= VMPI_snprintf(s, m, "%s = %s.%s [%s] ( ", formatRef(&b1, i), lirNames[op],
                                       formatAccSet(&b2, call->_storeAccSet),
                                       formatRef(&b3, i->arg(--argc)));
                else
                    m -= VMPI_snprintf(s, m, "%s = %s.%s #%s ( ", formatRef(&b1, i), lirNames[op],
                                       formatAccSet(&b2, call->_storeAccSet), call->_name);
                if (m < 0) break;
                for (int32_t j = argc - 1; j >= 0; j--) {
                    s += VMPI_strlen(s);
                    m -= VMPI_snprintf(s, m, "%s ",formatRef(&b2, i->arg(j)));
                    if (m < 0) break;
                }
                s += VMPI_strlen(s);
                m -= VMPI_snprintf(s, m, ")");
                break;
            }

            case LIR_jtbl: {
                int32_t m = int32_t(n);     // Windows doesn't have 'ssize_t'
                m -= VMPI_snprintf(s, m, "%s %s [ ", lirNames[op], formatRef(&b1, i->oprnd1()));
                if (m < 0) break;
                for (uint32_t j = 0, sz = i->getTableSize(); j < sz; j++) {
                    LIns* target = i->getTarget(j);
                    s += VMPI_strlen(s);
                    m -= VMPI_snprintf(s, m, "%s ", target ? formatRef(&b2, target) : "unpatched");
                    if (m < 0) break;
                }
                s += VMPI_strlen(s);
                m -= VMPI_snprintf(s, m, "]");
                break;
            }

            case LIR_param: {
                uint32_t arg = i->paramArg();
                if (!i->paramKind()) {
                    if (arg < sizeof(Assembler::argRegs)/sizeof(Assembler::argRegs[0])) {
                        VMPI_snprintf(s, n, "%s = %s %d %s", formatRef(&b1, i), lirNames[op],
                            arg, gpn(Assembler::argRegs[arg]));
                    } else {
                        VMPI_snprintf(s, n, "%s = %s %d", formatRef(&b1, i), lirNames[op], arg);
                    }
                } else {
                    VMPI_snprintf(s, n, "%s = %s %d %s", formatRef(&b1, i), lirNames[op],
                        arg, gpn(Assembler::savedRegs[arg]));
                }
                break;
            }

            case LIR_label:
                VMPI_snprintf(s, n, "%s:", formatRef(&b1, i));
                break;

            case LIR_jt:
            case LIR_jf:
                VMPI_snprintf(s, n, "%s %s -> %s", lirNames[op], formatRef(&b1, i->oprnd1()),
                    i->oprnd2() ? formatRef(&b2, i->oprnd2()) : "unpatched");
                break;

            case LIR_j:
                VMPI_snprintf(s, n, "%s -> %s", lirNames[op],
                    i->oprnd2() ? formatRef(&b1, i->oprnd2()) : "unpatched");
                break;

            case LIR_live:
            case LIR_flive:
            CASE64(LIR_qlive:)
            case LIR_ret:
            CASE64(LIR_qret:)
            case LIR_fret:
                VMPI_snprintf(s, n, "%s %s", lirNames[op], formatRef(&b1, i->oprnd1()));
                break;

            CASESF(LIR_callh:)
            case LIR_neg:
            case LIR_fneg:
            case LIR_i2f:
            case LIR_u2f:
            CASESF(LIR_qlo:)
            CASESF(LIR_qhi:)
            case LIR_not:
            CASE86(LIR_mod:)
            CASE64(LIR_i2q:)
            CASE64(LIR_u2q:)
            CASE64(LIR_q2i:)
            case LIR_f2i:
                VMPI_snprintf(s, n, "%s = %s %s", formatRef(&b1, i), lirNames[op],
                             formatRef(&b2, i->oprnd1()));
                break;

            case LIR_x:
            case LIR_xt:
            case LIR_xf:
            case LIR_xbarrier:
            case LIR_xtbl:
                formatGuard(buf, i);
                break;

            case LIR_addxov:
            case LIR_subxov:
            case LIR_mulxov:
                formatGuardXov(buf, i);
                break;

            case LIR_add:       CASE64(LIR_qiadd:)
            case LIR_sub:
            case LIR_mul:
            CASE86(LIR_div:)
            case LIR_fadd:
            case LIR_fsub:
            case LIR_fmul:
            case LIR_fdiv:
            case LIR_and:       CASE64(LIR_qiand:)
            case LIR_or:        CASE64(LIR_qior:)
            case LIR_xor:       CASE64(LIR_qxor:)
            case LIR_lsh:       CASE64(LIR_qilsh:)
            case LIR_rsh:       CASE64(LIR_qirsh:)
            case LIR_ush:       CASE64(LIR_qursh:)
            case LIR_eq:        CASE64(LIR_qeq:)
            case LIR_lt:        CASE64(LIR_qlt:)
            case LIR_le:        CASE64(LIR_qle:)
            case LIR_gt:        CASE64(LIR_qgt:)
            case LIR_ge:        CASE64(LIR_qge:)
            case LIR_ult:       CASE64(LIR_qult:)
            case LIR_ule:       CASE64(LIR_qule:)
            case LIR_ugt:       CASE64(LIR_qugt:)
            case LIR_uge:       CASE64(LIR_quge:)
            case LIR_feq:
            case LIR_flt:
            case LIR_fle:
            case LIR_fgt:
            case LIR_fge:
#if NJ_SOFTFLOAT_SUPPORTED
            case LIR_qjoin:
#endif
                VMPI_snprintf(s, n, "%s = %s %s, %s", formatRef(&b1, i), lirNames[op],
                    formatRef(&b2, i->oprnd1()),
                    formatRef(&b3, i->oprnd2()));
                break;

            CASE64(LIR_qcmov:)
            case LIR_cmov:
                VMPI_snprintf(s, n, "%s = %s %s ? %s : %s", formatRef(&b1, i), lirNames[op],
                    formatRef(&b2, i->oprnd1()),
                    formatRef(&b3, i->oprnd2()),
                    formatRef(&b4, i->oprnd3()));
                break;

            case LIR_ld:
            CASE64(LIR_ldq:)
            case LIR_ldf:
            case LIR_ldzb:
            case LIR_ldzs:
            case LIR_ldsb:
            case LIR_ldss:
            case LIR_ld32f:
                VMPI_snprintf(s, n, "%s = %s.%s %s[%d]", formatRef(&b1, i), lirNames[op],
                    formatAccSet(&b2, i->accSet()),
                    formatRef(&b3, i->oprnd1()),
                    i->disp());
                break;

            case LIR_sti:
            CASE64(LIR_stqi:)
            case LIR_stfi:
            case LIR_stb:
            case LIR_sts:
            case LIR_st32f:
                VMPI_snprintf(s, n, "%s.%s %s[%d] = %s", lirNames[op],
                    formatAccSet(&b1, i->accSet()),
                    formatRef(&b2, i->oprnd2()),
                    i->disp(),
                    formatRef(&b3, i->oprnd1()));
                break;

            default:
                NanoAssertMsgf(0, "Can't handle opcode %s\n", lirNames[op]);
                break;
        }
        return buf->buf;
    }
#endif


    CseFilter::CseFilter(LirWriter *out, Allocator& alloc)
        : LirWriter(out), storesSinceLastLoad(ACC_NONE)
    {
        uint32_t kInitialCaps[LInsLast + 1];
        kInitialCaps[LInsImm]          = 128;
        kInitialCaps[LInsImmq]         = PTR_SIZE(0, 16);
        kInitialCaps[LInsImmf]         = 16;
        kInitialCaps[LIns1]            = 256;
        kInitialCaps[LIns2]            = 512;
        kInitialCaps[LIns3]            = 16;
        kInitialCaps[LInsCall]         = 64;
        kInitialCaps[LInsLoadReadOnly] = 16;
        kInitialCaps[LInsLoadStack]    = 16;
        kInitialCaps[LInsLoadRStack]   = 16;
        kInitialCaps[LInsLoadOther]    = 16;
        kInitialCaps[LInsLoadMultiple] = 16;
        exprs = new (alloc) LInsHashSet(alloc, kInitialCaps);
    }

    LIns* CseFilter::insImm(int32_t imm)
    {
        uint32_t k;
        LInsp ins = exprs->findImm(imm, k);
        if (!ins) {
            ins = out->insImm(imm);
            exprs->add(LInsImm, ins, k);
        }
        // We assume that downstream stages do not modify the instruction, so
        // that we can insert 'ins' into slot 'k'.  Check this.
        NanoAssert(ins->isop(LIR_int) && ins->imm32() == imm);
        return ins;
    }

#ifdef NANOJIT_64BIT
    LIns* CseFilter::insImmq(uint64_t q)
    {
        uint32_t k;
        LInsp ins = exprs->findImmq(q, k);
        if (!ins) {
            ins = out->insImmq(q);
            exprs->add(LInsImmq, ins, k);
        }
        NanoAssert(ins->isop(LIR_quad) && ins->imm64() == q);
        return ins;
    }
#endif

    LIns* CseFilter::insImmf(double d)
    {
        uint32_t k;
        // We must pun 'd' as a uint64_t otherwise 0 and -0 will be treated as
        // equal, which breaks things (see bug 527288).
        union {
            double d;
            uint64_t u64;
        } u;
        u.d = d;
        LInsp ins = exprs->findImmf(u.u64, k);
        if (!ins) {
            ins = out->insImmf(d);
            exprs->add(LInsImmf, ins, k);
        }
        NanoAssert(ins->isop(LIR_float) && ins->imm64() == u.u64);
        return ins;
    }

    LIns* CseFilter::ins0(LOpcode op)
    {
        if (op == LIR_label)
            exprs->clear();
        return out->ins0(op);
    }

    LIns* CseFilter::ins1(LOpcode op, LInsp a)
    {
        LInsp ins;
        if (isCseOpcode(op)) {
            uint32_t k;
            ins = exprs->find1(op, a, k);
            if (!ins) {
                ins = out->ins1(op, a);
                exprs->add(LIns1, ins, k);
            }
        } else {
            ins = out->ins1(op, a);
        }
        NanoAssert(ins->isop(op) && ins->oprnd1() == a);
        return ins;
    }

    LIns* CseFilter::ins2(LOpcode op, LInsp a, LInsp b)
    {
        LInsp ins;
        NanoAssert(isCseOpcode(op));
        uint32_t k;
        ins = exprs->find2(op, a, b, k);
        if (!ins) {
            ins = out->ins2(op, a, b);
            exprs->add(LIns2, ins, k);
        }
        NanoAssert(ins->isop(op) && ins->oprnd1() == a && ins->oprnd2() == b);
        return ins;
    }

    LIns* CseFilter::ins3(LOpcode op, LInsp a, LInsp b, LInsp c)
    {
        NanoAssert(isCseOpcode(op));
        uint32_t k;
        LInsp ins = exprs->find3(op, a, b, c, k);
        if (!ins) {
            ins = out->ins3(op, a, b, c);
            exprs->add(LIns3, ins, k);
        }
        NanoAssert(ins->isop(op) && ins->oprnd1() == a && ins->oprnd2() == b && ins->oprnd3() == c);
        return ins;
    }

    LIns* CseFilter::insLoad(LOpcode op, LInsp base, int32_t disp, AccSet loadAccSet)
    {
        LInsp ins;
        if (isS16(disp)) {
            // Clear all loads aliased by stores and calls since the last time
            // we were in this function.
            if (storesSinceLastLoad != ACC_NONE) {
                NanoAssert(!(storesSinceLastLoad & ACC_READONLY));  // can't store to READONLY
                if (storesSinceLastLoad & ACC_STACK)  { exprs->clear(LInsLoadStack); }
                if (storesSinceLastLoad & ACC_RSTACK) { exprs->clear(LInsLoadRStack); }
                if (storesSinceLastLoad & ACC_OTHER)  { exprs->clear(LInsLoadOther); }
                // Loads marked with multiple access regions must be treated
                // conservatively -- we always clear all of them.
                exprs->clear(LInsLoadMultiple);
                storesSinceLastLoad = ACC_NONE;
            }

            LInsHashKind kind;
            switch (loadAccSet) {
            case ACC_READONLY:  kind = LInsLoadReadOnly;    break;
            case ACC_STACK:     kind = LInsLoadStack;       break;
            case ACC_RSTACK:    kind = LInsLoadRStack;      break;
            case ACC_OTHER:     kind = LInsLoadOther;       break;
            default:            kind = LInsLoadMultiple;    break;
            }

            uint32_t k;
            ins = exprs->findLoad(op, base, disp, loadAccSet, kind, k);
            if (!ins) {
                ins = out->insLoad(op, base, disp, loadAccSet);
                exprs->add(kind, ins, k);
            }
            NanoAssert(ins->isop(op) && ins->oprnd1() == base && ins->disp() == disp);

        } else {
            // If the displacement is more than 16 bits, put it in a separate
            // instruction.  Nb: LirBufWriter also does this, we do it here
            // too because CseFilter relies on LirBufWriter not changing code.
            ins = insLoad(op, ins2(LIR_piadd, base, insImmWord(disp)), 0, loadAccSet);
        }
        return ins;
    }

    LIns* CseFilter::insStore(LOpcode op, LInsp value, LInsp base, int32_t disp, AccSet accSet)
    {
        LInsp ins;
        if (isS16(disp)) {
            storesSinceLastLoad |= accSet;
            ins = out->insStore(op, value, base, disp, accSet);
            NanoAssert(ins->isop(op) && ins->oprnd1() == value && ins->oprnd2() == base &&
                       ins->disp() == disp && ins->accSet() == accSet);
        } else {
            // If the displacement is more than 16 bits, put it in a separate
            // instruction.  Nb: LirBufWriter also does this, we do it here
            // too because CseFilter relies on LirBufWriter not changing code.
            ins = insStore(op, value, ins2(LIR_piadd, base, insImmWord(disp)), 0, accSet);
        }
        return ins;
    }

    LInsp CseFilter::insGuard(LOpcode op, LInsp c, GuardRecord *gr)
    {
        // LIR_xt and LIR_xf guards are CSEable.  Note that we compare the
        // opcode and condition when determining if two guards are equivalent
        // -- in find1() and hash1() -- but we do *not* compare the
        // GuardRecord.  This works because:
        // - If guard 1 is taken (exits) then guard 2 is never reached, so
        //   guard 2 can be removed.
        // - If guard 1 is not taken then neither is guard 2, so guard 2 can
        //   be removed.
        //
        // The underlying assumptions that are required for this to be safe:
        // - There's never a path from the side exit of guard 1 back to guard
        //   2;  for tree-shaped fragments this should be true.
        // - GuardRecords do not contain information other than what is needed
        //   to execute a successful exit.  That is currently true.
        // - The CSE algorithm will always keep guard 1 and remove guard 2
        //   (not vice versa).  The current algorithm does this.
        //
        LInsp ins;
        if (isCseOpcode(op)) {
            // conditional guard
            uint32_t k;
            ins = exprs->find1(op, c, k);
            if (!ins) {
                ins = out->insGuard(op, c, gr);
                exprs->add(LIns1, ins, k);
            }
        } else {
            ins = out->insGuard(op, c, gr);
        }
        NanoAssert(ins->isop(op) && ins->oprnd1() == c);
        return ins;
    }

    LInsp CseFilter::insGuardXov(LOpcode op, LInsp a, LInsp b, GuardRecord *gr)
    {
        // LIR_*xov are CSEable.  See CseFilter::insGuard() for details.
        NanoAssert(isCseOpcode(op));
        // conditional guard
        uint32_t k;
        LInsp ins = exprs->find2(op, a, b, k);
        if (!ins) {
            ins = out->insGuardXov(op, a, b, gr);
            exprs->add(LIns2, ins, k);
        }
        NanoAssert(ins->isop(op) && ins->oprnd1() == a && ins->oprnd2() == b);
        return ins;
    }

    LInsp CseFilter::insCall(const CallInfo *ci, LInsp args[])
    {
        LInsp ins;
        uint32_t argc = ci->count_args();
        if (ci->_isPure) {
            NanoAssert(ci->_storeAccSet == ACC_NONE);
            uint32_t k;
            ins = exprs->findCall(ci, argc, args, k);
            if (!ins) {
                ins = out->insCall(ci, args);
                exprs->add(LInsCall, ins, k);
            }
        } else {
            // We only need to worry about aliasing if !ci->_isPure.
            storesSinceLastLoad |= ci->_storeAccSet;
            ins = out->insCall(ci, args);
        }
        NanoAssert(ins->isCall() && ins->callInfo() == ci && argsmatch(ins, argc, args));
        return ins;
    }


#if NJ_SOFTFLOAT_SUPPORTED
    static double FASTCALL i2f(int32_t i)           { return i; }
    static double FASTCALL u2f(uint32_t u)          { return u; }
    static double FASTCALL fneg(double a)           { return -a; }
    static double FASTCALL fadd(double a, double b) { return a + b; }
    static double FASTCALL fsub(double a, double b) { return a - b; }
    static double FASTCALL fmul(double a, double b) { return a * b; }
    static double FASTCALL fdiv(double a, double b) { return a / b; }
    static int32_t FASTCALL feq(double a, double b) { return a == b; }
    static int32_t FASTCALL flt(double a, double b) { return a <  b; }
    static int32_t FASTCALL fgt(double a, double b) { return a >  b; }
    static int32_t FASTCALL fle(double a, double b) { return a <= b; }
    static int32_t FASTCALL fge(double a, double b) { return a >= b; }

    #define SIG_F_I     (ARGTYPE_F | ARGTYPE_I << ARGTYPE_SHIFT*1)
    #define SIG_F_U     (ARGTYPE_F | ARGTYPE_U << ARGTYPE_SHIFT*1)
    #define SIG_F_F     (ARGTYPE_F | ARGTYPE_F << ARGTYPE_SHIFT*1)
    #define SIG_F_FF    (ARGTYPE_F | ARGTYPE_F << ARGTYPE_SHIFT*1 | ARGTYPE_F << ARGTYPE_SHIFT*2)
    #define SIG_B_FF    (ARGTYPE_B | ARGTYPE_F << ARGTYPE_SHIFT*1 | ARGTYPE_F << ARGTYPE_SHIFT*2)

    #define SF_CALLINFO(name, typesig) \
        static const CallInfo name##_ci = \
            { (intptr_t)&name, typesig, ABI_FASTCALL, /*isPure*/1, ACC_NONE verbose_only(, #name) }

    SF_CALLINFO(i2f,  SIG_F_I);
    SF_CALLINFO(u2f,  SIG_F_U);
    SF_CALLINFO(fneg, SIG_F_F);
    SF_CALLINFO(fadd, SIG_F_FF);
    SF_CALLINFO(fsub, SIG_F_FF);
    SF_CALLINFO(fmul, SIG_F_FF);
    SF_CALLINFO(fdiv, SIG_F_FF);
    SF_CALLINFO(feq,  SIG_B_FF);
    SF_CALLINFO(flt,  SIG_B_FF);
    SF_CALLINFO(fgt,  SIG_B_FF);
    SF_CALLINFO(fle,  SIG_B_FF);
    SF_CALLINFO(fge,  SIG_B_FF);

    SoftFloatOps::SoftFloatOps()
    {
        memset(opmap, 0, sizeof(opmap));
        opmap[LIR_i2f] = &i2f_ci;
        opmap[LIR_u2f] = &u2f_ci;
        opmap[LIR_fneg] = &fneg_ci;
        opmap[LIR_fadd] = &fadd_ci;
        opmap[LIR_fsub] = &fsub_ci;
        opmap[LIR_fmul] = &fmul_ci;
        opmap[LIR_fdiv] = &fdiv_ci;
        opmap[LIR_feq] = &feq_ci;
        opmap[LIR_flt] = &flt_ci;
        opmap[LIR_fgt] = &fgt_ci;
        opmap[LIR_fle] = &fle_ci;
        opmap[LIR_fge] = &fge_ci;
    }

    const SoftFloatOps softFloatOps;

    SoftFloatFilter::SoftFloatFilter(LirWriter *out) : LirWriter(out)
    {}

    LIns* SoftFloatFilter::split(LIns *a) {
        if (a->isF64() && !a->isop(LIR_qjoin)) {
            // all F64 args must be qjoin's for soft-float
            a = ins2(LIR_qjoin, ins1(LIR_qlo, a), ins1(LIR_qhi, a));
        }
        return a;
    }

    LIns* SoftFloatFilter::split(const CallInfo *call, LInsp args[]) {
        LIns *lo = out->insCall(call, args);
        LIns *hi = out->ins1(LIR_callh, lo);
        return out->ins2(LIR_qjoin, lo, hi);
    }

    LIns* SoftFloatFilter::fcall1(const CallInfo *call, LIns *a) {
        LIns *args[] = { split(a) };
        return split(call, args);
    }

    LIns* SoftFloatFilter::fcall2(const CallInfo *call, LIns *a, LIns *b) {
        LIns *args[] = { split(b), split(a) };
        return split(call, args);
    }

    LIns* SoftFloatFilter::fcmp(const CallInfo *call, LIns *a, LIns *b) {
        LIns *args[] = { split(b), split(a) };
        return out->ins2(LIR_eq, out->insCall(call, args), out->insImm(1));
    }

    LIns* SoftFloatFilter::ins1(LOpcode op, LIns *a) {
        const CallInfo *ci = softFloatOps.opmap[op];
        if (ci)
            return fcall1(ci, a);
        if (op == LIR_fret)
            return out->ins1(op, split(a));
        return out->ins1(op, a);
    }

    LIns* SoftFloatFilter::ins2(LOpcode op, LIns *a, LIns *b) {
        const CallInfo *ci = softFloatOps.opmap[op];
        if (ci) {
            if (isFCmpOpcode(op))
                return fcmp(ci, a, b);
            return fcall2(ci, a, b);
        }
        return out->ins2(op, a, b);
    }

    LIns* SoftFloatFilter::insCall(const CallInfo *ci, LInsp args[]) {
        uint32_t nArgs = ci->count_args();
        for (uint32_t i = 0; i < nArgs; i++)
            args[i] = split(args[i]);

        if (ci->returnType() == ARGTYPE_F) {
            // This function returns a double as two 32bit values, so replace
            // call with qjoin(qhi(call), call).
            return split(ci, args);
        }
        return out->insCall(ci, args);
    }
#endif // NJ_SOFTFLOAT_SUPPORTED


    #endif /* FEATURE_NANOJIT */

#if defined(NJ_VERBOSE)
    AddrNameMap::AddrNameMap(Allocator& a)
        : allocator(a), names(a)
    {}

    void AddrNameMap::addAddrRange(const void *p, size_t size, size_t align, const char *name)
    {
        if (!this || names.containsKey(p))
            return;
        char* copy = new (allocator) char[VMPI_strlen(name)+1];
        VMPI_strcpy(copy, name);
        Entry *e = new (allocator) Entry(copy, size << align, align);
        names.put(p, e);
    }

    void AddrNameMap::lookupAddr(void *p, char*& name, int32_t& offset)
    {
        const void *start = names.findNear(p);
        if (start) {
            Entry *e = names.get(start);
            const void *end = (const char*)start + e->size;
            if (p == start) {
                name = e->name;
                offset = 0;
            }
            else if (p > start && p < end) {
                name = e->name;
                offset = int32_t(intptr_t(p)-intptr_t(start)) >> e->align;
            }
            else {
                name = NULL;
                offset = 0;
            }
        } else {
            name = NULL;
            offset = 0;
        }
    }

    // ---------------------------------------------------------------
    // START debug-logging definitions
    // ---------------------------------------------------------------

    void LogControl::printf( const char* format, ... )
    {
        va_list vargs;
        va_start(vargs, format);
        vfprintf(stdout, format, vargs);
        va_end(vargs);
        // Flush every line immediately so that if crashes occur in generated
        // code we won't lose any output.
        fflush(stdout);
    }

#endif // NJ_VERBOSE


#ifdef FEATURE_NANOJIT
#ifdef DEBUG
    const char* ValidateWriter::type2string(LTy type)
    {
        switch (type) {
        case LTy_Void:                  return "void";
        case LTy_I32:                   return "int32";
#ifdef NANOJIT_64BIT
        case LTy_I64:                   return "int64";
#endif
        case LTy_F64:                   return "float64";
        default:       NanoAssert(0);   return "???";
        }
    }

    void ValidateWriter::typeCheckArgs(LOpcode op, int nArgs, LTy formals[], LIns* args[])
    {
        NanoAssert(nArgs >= 0);

        // Type-check the arguments.
        for (int i = 0; i < nArgs; i++) {
            LTy formal = formals[i];
            LTy actual = args[i]->retType();
            if (formal != actual) {
                // Assert on a type error.  The disadvantage of doing this (as
                // opposed to printing a message and continuing) is that at
                // most one type error will be detected per run.  But type
                // errors should be rare, and assertion failures are certain
                // to be caught by test suites whereas error messages may not
                // be.
                NanoAssertMsgf(0,
                    "LIR type error (%s): arg %d of '%s' is '%s' "
                    "which has type %s (expected %s)",
                    whereInPipeline, i+1, lirNames[op],
                    lirNames[args[i]->opcode()],
                    type2string(actual), type2string(formal));
            }
        }
    }

    void ValidateWriter::errorStructureShouldBe(LOpcode op, const char* argDesc, int argN,
                                                LIns* arg, const char* shouldBeDesc)
    {
        NanoAssertMsgf(0,
            "LIR structure error (%s): %s %d of '%s' is '%s' (expected %s)",
            whereInPipeline, argDesc, argN,
            lirNames[op], lirNames[arg->opcode()], shouldBeDesc);
    }

    void ValidateWriter::errorAccSet(const char* what, AccSet accSet, const char* shouldDesc)
    {
        RefBuf b;
        NanoAssertMsgf(0,
            "LIR AccSet error (%s): '%s' AccSet is '%s'; %s",
            whereInPipeline, what, printer->formatAccSet(&b, accSet), shouldDesc);
    }

    void ValidateWriter::checkLInsIsACondOrConst(LOpcode op, int argN, LIns* ins)
    {
        // We could introduce a LTy_B32 type in the type system but that's a
        // bit weird because its representation is identical to LTy_I32.  It's
        // easier to just do this check structurally.  Also, optimization can
        // cause the condition to become a LIR_int.
        if (!ins->isCmp() && !ins->isconst())
            errorStructureShouldBe(op, "argument", argN, ins, "a condition or 32-bit constant");
    }

    void ValidateWriter::checkLInsIsNull(LOpcode op, int argN, LIns* ins)
    {
        if (ins)
            errorStructureShouldBe(op, "argument", argN, ins, NULL);
    }

    void ValidateWriter::checkLInsHasOpcode(LOpcode op, int argN, LIns* ins, LOpcode op2)
    {
        if (!ins->isop(op2))
            errorStructureShouldBe(op, "argument", argN, ins, lirNames[op2]);
    }

    void ValidateWriter::checkAccSet(LOpcode op, LInsp base, AccSet accSet, AccSet maxAccSet)
    {
        if (accSet == ACC_NONE)
            errorAccSet(lirNames[op], accSet, "it should not equal ACC_NONE");

        if (accSet & ~maxAccSet)
            errorAccSet(lirNames[op], accSet,
                "it should not contain bits that aren't in ACC_LOAD_ANY/ACC_STORE_ANY");

        // Some sanity checking, which is based on the following assumptions:
        // - STACK ones should use 'sp' or 'sp+k' as the base.  (We could look
        //   for more complex patterns, but that feels dangerous.  Better to
        //   keep it really simple.)
        // - RSTACK ones should use 'rp' as the base.
        // - READONLY/OTHER ones should not use 'sp'/'sp+k' or 'rp' as the base.
        //
        // Things that aren't checked:
        // - There's no easy way to check if READONLY ones really are read-only.

        bool isStack = base == sp ||
                      (base->isop(LIR_piadd) && base->oprnd1() == sp && base->oprnd2()->isconstp());
        bool isRStack = base == rp;

        switch (accSet) {
        case ACC_STACK:
            if (!isStack)
                errorAccSet(lirNames[op], accSet, "but it's not a stack access");
            break;

        case ACC_RSTACK:
            if (!isRStack)
                errorAccSet(lirNames[op], accSet, "but it's not an rstack access");
            break;

        case ACC_READONLY:
        case ACC_OTHER:
            if (isStack)
                errorAccSet(lirNames[op], accSet, "but it's a stack access");
            if (isRStack)
                errorAccSet(lirNames[op], accSet, "but it's an rstack access");
            break;

        default:
            break;
        }
    }

    ValidateWriter::ValidateWriter(LirWriter *out, LInsPrinter* printer, const char* where)
        : LirWriter(out), printer(printer), whereInPipeline(where), sp(0), rp(0)
    {}

    LIns* ValidateWriter::insLoad(LOpcode op, LIns* base, int32_t d, AccSet accSet)
    {
        checkAccSet(op, base, accSet, ACC_LOAD_ANY);

        int nArgs = 1;
        LTy formals[1] = { LTy_Ptr };
        LIns* args[1] = { base };

        switch (op) {
        case LIR_ld:
        case LIR_ldf:
        case LIR_ldzb:
        case LIR_ldzs:
        case LIR_ldsb:
        case LIR_ldss:
        case LIR_ld32f:
        CASE64(LIR_ldq:)
            break;
        default:
            NanoAssert(0);
        }

        typeCheckArgs(op, nArgs, formals, args);

        return out->insLoad(op, base, d, accSet);
    }

    LIns* ValidateWriter::insStore(LOpcode op, LIns* value, LIns* base, int32_t d, AccSet accSet)
    {
        checkAccSet(op, base, accSet, ACC_STORE_ANY);

        int nArgs = 2;
        LTy formals[2] = { LTy_Void, LTy_Ptr };     // LTy_Void is overwritten shortly
        LIns* args[2] = { value, base };

        switch (op) {
        case LIR_stb:
        case LIR_sts:
        case LIR_sti:
            formals[0] = LTy_I32;
            break;

#ifdef NANOJIT_64BIT
        case LIR_stqi:
            formals[0] = LTy_I64;
            break;
#endif

        case LIR_stfi:
        case LIR_st32f:
            formals[0] = LTy_F64;
            break;

        default:
            NanoAssert(0);
        }

        typeCheckArgs(op, nArgs, formals, args);

        return out->insStore(op, value, base, d, accSet);
    }

    LIns* ValidateWriter::ins0(LOpcode op)
    {
        switch (op) {
        case LIR_start:
        case LIR_regfence:
        case LIR_label:
            break;
        default:
            NanoAssert(0);
        }

        // No args to type-check.

        return out->ins0(op);
    }

    LIns* ValidateWriter::ins1(LOpcode op, LIns* a)
    {
        int nArgs = 1;
        LTy formals[1];
        LIns* args[1] = { a };

        switch (op) {
        case LIR_neg:
        case LIR_not:
        case LIR_i2f:
        case LIR_u2f:
        case LIR_live:
        case LIR_ret:
            formals[0] = LTy_I32;
            break;

#ifdef NANOJIT_64BIT
        case LIR_i2q:
        case LIR_u2q:
            formals[0] = LTy_I32;
            break;

        case LIR_q2i:
        case LIR_qret:
        case LIR_qlive:
            formals[0] = LTy_I64;
            break;
#endif

#if defined NANOJIT_IA32 || defined NANOJIT_X64
        case LIR_mod:       // see LIRopcode.tbl for why 'mod' is unary
            checkLInsHasOpcode(op, 1, a, LIR_div);
            formals[0] = LTy_I32;
            break;
#endif

#if NJ_SOFTFLOAT_SUPPORTED
        case LIR_qlo:
        case LIR_qhi:
            formals[0] = LTy_F64;
            break;

        case LIR_callh:
            // The operand of a LIR_callh is LIR_icall, even though the
            // function being called has a return type of LTy_F64.
            checkLInsHasOpcode(op, 1, a, LIR_icall);
            formals[0] = LTy_I32;
            break;
#endif

        case LIR_fneg:
        case LIR_fret:
        case LIR_flive:
        case LIR_f2i:
            formals[0] = LTy_F64;
            break;

        case LIR_file:
        case LIR_line:
            // XXX: not sure about these ones.  Ignore for the moment.
            nArgs = 0;
            break;

        default:
            NanoAssertMsgf(0, "%s\n", lirNames[op]);
        }

        typeCheckArgs(op, nArgs, formals, args);

        return out->ins1(op, a);
    }

    LIns* ValidateWriter::ins2(LOpcode op, LIns* a, LIns* b)
    {
        int nArgs = 2;
        LTy formals[2];
        LIns* args[2] = { a, b };

        switch (op) {
        case LIR_add:
        case LIR_sub:
        case LIR_mul:
        CASE86(LIR_div:)
        case LIR_and:
        case LIR_or:
        case LIR_xor:
        case LIR_lsh:
        case LIR_rsh:
        case LIR_ush:
        case LIR_eq:
        case LIR_lt:
        case LIR_gt:
        case LIR_le:
        case LIR_ge:
        case LIR_ult:
        case LIR_ugt:
        case LIR_ule:
        case LIR_uge:
            formals[0] = LTy_I32;
            formals[1] = LTy_I32;
            break;

#if NJ_SOFTFLOAT_SUPPORTED
        case LIR_qjoin:
            formals[0] = LTy_I32;
            formals[1] = LTy_I32;
            break;
#endif

#ifdef NANOJIT_64BIT
        case LIR_qiand:
        case LIR_qior:
        case LIR_qxor:
        case LIR_qiadd:
        case LIR_qeq:
        case LIR_qlt:
        case LIR_qgt:
        case LIR_qle:
        case LIR_qge:
        case LIR_qult:
        case LIR_qugt:
        case LIR_qule:
        case LIR_quge:
            formals[0] = LTy_I64;
            formals[1] = LTy_I64;
            break;

        case LIR_qilsh:
        case LIR_qirsh:
        case LIR_qursh:
            formals[0] = LTy_I64;
            formals[1] = LTy_I32;
            break;
#endif

        case LIR_fadd:
        case LIR_fsub:
        case LIR_fmul:
        case LIR_fdiv:
        case LIR_feq:
        case LIR_fgt:
        case LIR_flt:
        case LIR_fle:
        case LIR_fge:
            formals[0] = LTy_F64;
            formals[1] = LTy_F64;
            break;

        default:
            NanoAssert(0);
        }

        typeCheckArgs(op, nArgs, formals, args);

        return out->ins2(op, a, b);
    }

    LIns* ValidateWriter::ins3(LOpcode op, LIns* a, LIns* b, LIns* c)
    {
        int nArgs = 3;
        LTy formals[3] = { LTy_I32, LTy_Void, LTy_Void };   // LTy_Void gets overwritten
        LIns* args[3] = { a, b, c };

        switch (op) {
        case LIR_cmov:
            checkLInsIsACondOrConst(op, 1, a);
            formals[1] = LTy_I32;
            formals[2] = LTy_I32;
            break;

#ifdef NANOJIT_64BIT
        case LIR_qcmov:
            checkLInsIsACondOrConst(op, 1, a);
            formals[1] = LTy_I64;
            formals[2] = LTy_I64;
            break;
#endif

        default:
            NanoAssert(0);
        }

        typeCheckArgs(op, nArgs, formals, args);

        return out->ins3(op, a, b, c);
    }

    LIns* ValidateWriter::insParam(int32_t arg, int32_t kind)
    {
        return out->insParam(arg, kind);
    }

    LIns* ValidateWriter::insImm(int32_t imm)
    {
        return out->insImm(imm);
    }

#ifdef NANOJIT_64BIT
    LIns* ValidateWriter::insImmq(uint64_t imm)
    {
        return out->insImmq(imm);
    }
#endif

    LIns* ValidateWriter::insImmf(double d)
    {
        return out->insImmf(d);
    }

    LIns* ValidateWriter::insCall(const CallInfo *ci, LIns* args0[])
    {
        ArgType argTypes[MAXARGS];
        uint32_t nArgs = ci->getArgTypes(argTypes);
        LTy formals[MAXARGS];
        LIns* args[MAXARGS];    // in left-to-right order, unlike args0[]

        LOpcode op = getCallOpcode(ci);

        if (ci->_isPure && ci->_storeAccSet != ACC_NONE)
            errorAccSet(ci->_name, ci->_storeAccSet, "it should be ACC_NONE for pure functions");

        if (ci->_storeAccSet & ~ACC_STORE_ANY)
            errorAccSet(lirNames[op], ci->_storeAccSet,
                "it should not contain bits that aren't in ACC_STORE_ANY");

        // This loop iterates over the args from right-to-left (because arg()
        // and getArgTypes() use right-to-left order), but puts the results
        // into formals[] and args[] in left-to-right order so that arg
        // numbers in error messages make sense to the user.
        for (uint32_t i = 0; i < nArgs; i++) {
            uint32_t i2 = nArgs - i - 1;    // converts right-to-left to left-to-right
            switch (argTypes[i]) {
            case ARGTYPE_I:
            case ARGTYPE_U:         formals[i2] = LTy_I32;   break;
#ifdef NANOJIT_64BIT
            case ARGTYPE_Q:         formals[i2] = LTy_I64;   break;
#endif
            case ARGTYPE_F:         formals[i2] = LTy_F64;   break;
            default: NanoAssertMsgf(0, "%d %s\n", argTypes[i],ci->_name); formals[i2] = LTy_Void;  break;
            }
            args[i2] = args0[i];
        }

        typeCheckArgs(op, nArgs, formals, args);

        return out->insCall(ci, args0);
    }

    LIns* ValidateWriter::insGuard(LOpcode op, LIns *cond, GuardRecord *gr)
    {
        int nArgs = -1;     // init to shut compilers up
        LTy formals[1];
        LIns* args[1];

        switch (op) {
        case LIR_x:
        case LIR_xbarrier:
            checkLInsIsNull(op, 1, cond);
            nArgs = 0;
            break;

        case LIR_xt:
        case LIR_xf:
            checkLInsIsACondOrConst(op, 1, cond);
            nArgs = 1;
            formals[0] = LTy_I32;
            args[0] = cond;
            break;

        case LIR_xtbl:
            nArgs = 1;
            formals[0] = LTy_I32;   // unlike xt/xf/jt/jf, this is an index, not a condition
            args[0] = cond;
            break;

        default:
            NanoAssert(0);
        }

        typeCheckArgs(op, nArgs, formals, args);

        return out->insGuard(op, cond, gr);
    }

    LIns* ValidateWriter::insGuardXov(LOpcode op, LIns* a, LIns* b, GuardRecord* gr)
    {
        int nArgs = 2;
        LTy formals[2] = { LTy_I32, LTy_I32 };
        LIns* args[2] = { a, b };

        switch (op) {
        case LIR_addxov:
        case LIR_subxov:
        case LIR_mulxov:
            break;

        default:
            NanoAssert(0);
        }

        typeCheckArgs(op, nArgs, formals, args);

        return out->insGuardXov(op, a, b, gr);
    }

    LIns* ValidateWriter::insBranch(LOpcode op, LIns* cond, LIns* to)
    {
        int nArgs = -1;     // init to shut compilers up
        LTy formals[1];
        LIns* args[1];

        switch (op) {
        case LIR_j:
            checkLInsIsNull(op, 1, cond);
            nArgs = 0;
            break;

        case LIR_jt:
        case LIR_jf:
            checkLInsIsACondOrConst(op, 1, cond);
            nArgs = 1;
            formals[0] = LTy_I32;
            args[0] = cond;
            break;

        default:
            NanoAssert(0);
        }

        // We check that target is a label in ValidateReader because it may
        // not have been set here.

        typeCheckArgs(op, nArgs, formals, args);

        return out->insBranch(op, cond, to);
    }

    LIns* ValidateWriter::insAlloc(int32_t size)
    {
        return out->insAlloc(size);
    }

    LIns* ValidateWriter::insJtbl(LIns* index, uint32_t size)
    {
        int nArgs = 1;
        LTy formals[1] = { LTy_I32 };
        LIns* args[1] = { index };

        typeCheckArgs(LIR_jtbl, nArgs, formals, args);

        // We check that all jump table entries are labels in ValidateReader
        // because they won't have been set here.

        return out->insJtbl(index, size);
    }

    ValidateReader::ValidateReader(LirFilter* in) : LirFilter(in)
        {}

    LIns* ValidateReader::read()
    {
        LIns *ins = in->read();
        switch (ins->opcode()) {
        case LIR_jt:
        case LIR_jf:
        case LIR_j:
            NanoAssert(ins->getTarget() && ins->oprnd2()->isop(LIR_label));
            break;
        case LIR_jtbl: {
            uint32_t tableSize = ins->getTableSize();
            NanoAssert(tableSize > 0);
            for (uint32_t i = 0; i < tableSize; i++) {
                LIns* target = ins->getTarget(i);
                NanoAssert(target);
                NanoAssert(target->isop(LIR_label));
            }
            break;
        }
        default:
            ;
        }
        return ins;
    }

#endif
#endif

}
