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
#include <stdio.h>
#include <ctype.h>

#ifdef PERFM
#include "../vprof/vprof.h"
#endif /* PERFM */


namespace nanojit
{
    using namespace avmplus;
    #ifdef FEATURE_NANOJIT

    const uint8_t operandCount[] = {
#define OPDEF(op, number, operands, repkind) \
        operands,
#define OPDEF64(op, number, operands, repkind) \
        operands,
#include "LIRopcode.tbl"
#undef OPDEF
#undef OPDEF64
        0
    };

    const uint8_t repKinds[] = {
#define OPDEF(op, number, operands, repkind) \
        LRK_##repkind,
#define OPDEF64(op, number, operands, repkind) \
        OPDEF(op, number, operands, repkind)
#include "LIRopcode.tbl"
#undef OPDEF
#undef OPDEF64
        0
    };

        const uint8_t insSizes[] = {
#define OPDEF(op, number, operands, repkind) \
            sizeof(LIns##repkind),
#define OPDEF64(op, number, operands, repkind) \
            OPDEF(op, number, operands, repkind)
#include "LIRopcode.tbl"
#undef OPDEF
#undef OPDEF64
            0
        };

    // LIR verbose specific
    #ifdef NJ_VERBOSE

    const char* lirNames[] = {
#define OPDEF(op, number, operands, repkind) \
        #op,
#define OPDEF64(op, number, operands, repkind) \
        #op,
#include "LIRopcode.tbl"
#undef OPDEF
#undef OPDEF64
        NULL
    };

    #endif /* NANOJIT_VEBROSE */

    // implementation

#ifdef NJ_PROFILE
    // @todo fixup move to nanojit.h
    #undef counter_value
    #define counter_value(x)        x
#endif /* NJ_PROFILE */

    // LCompressedBuffer
    LirBuffer::LirBuffer(Fragmento* frago)
        : _frago(frago),
#ifdef NJ_VERBOSE
          names(NULL),
#endif
          abi(ABI_FASTCALL),
          state(NULL), param1(NULL), sp(NULL), rp(NULL),
          _pages(frago->core()->GetGC())
    {
        rewind();
    }

    LirBuffer::~LirBuffer()
    {
        clear();
        verbose_only(if (names) NJ_DELETE(names);)
        _frago = 0;
    }

    void LirBuffer::clear()
    {
        // free all the memory and clear the stats
        _frago->pagesRelease(_pages);
        NanoAssert(!_pages.size());
        _unused = 0;
        _stats.lir = 0;
        _noMem = 0;
        _nextPage = 0;
        for (int i = 0; i < NumSavedRegs; ++i)
            savedRegs[i] = NULL;
        explicitSavedRegs = false;
    }

    void LirBuffer::rewind()
    {
        clear();
        // pre-allocate the current and the next page we will be using
        Page* start = pageAlloc();
        _unused = start ? uintptr_t(&start->lir[0]) : 0;
        _nextPage = pageAlloc();
        NanoAssert((_unused && _nextPage) || _noMem);
    }

    int32_t LirBuffer::insCount()
    {
        // A LIR_skip payload is considered part of the LIR_skip, and LIR_call
        // arg slots are considered part of the LIR_call.
        return _stats.lir;
    }

    size_t LirBuffer::byteCount()
    {
        return ((_pages.size() ? _pages.size()-1 : 0) * sizeof(Page)) +
            (_unused - pageTop(_unused));
    }

    Page* LirBuffer::pageAlloc()
    {
        Page* page = _frago->pageAlloc();
        if (page)
            _pages.add(page);
        else
            _noMem = 1;
        return page;
    }

    // Allocate a new page, and write the first instruction to it -- a skip
    // linking to last instruction of the previous page.
    void LirBuffer::moveToNewPage(uintptr_t addrOfLastLInsOnCurrentPage)
    {
        // We don't want this to fail, so we always have a page in reserve.
        NanoAssert(_nextPage);
        _unused = uintptr_t(&_nextPage->lir[0]);
        _nextPage = pageAlloc();
        NanoAssert(_nextPage || _noMem);

        // Link LIR stream back to prior instruction.
        // Unlike all the ins*() functions, we don't call makeRoom() here
        // because we know we have enough space, having just started a new
        // page.
        LInsSk* insSk = (LInsSk*)_unused;
        LIns*   ins   = insSk->getLIns();
        ins->initLInsSk((LInsp)addrOfLastLInsOnCurrentPage);
        _unused += sizeof(LInsSk);
        _stats.lir++;
    }

    // Make room for a single instruction.
    uintptr_t LirBuffer::makeRoom(size_t szB)
    {
        // Make sure the size is ok, and that we're not pointing to the
        // PageHeader.
        NanoAssert(0 == szB % sizeof(void*));
        NanoAssert(sizeof(LIns) <= szB && szB <= NJ_MAX_LINS_SZB);
        NanoAssert(_unused >= pageDataStart(_unused));

        // If the instruction won't fit on the current page, move to the next
        // page.
        if (_unused + szB - 1 > pageBottom(_unused)) {
            uintptr_t addrOfLastLInsOnPage = _unused - sizeof(LIns);
            moveToNewPage(addrOfLastLInsOnPage);
        }

        // We now know that we are on a page that has the requested amount of
        // room: record the starting address of the requested space and bump
        // the pointer.
        uintptr_t startOfRoom = _unused;
        _unused += szB;
        _stats.lir++;             // count the instruction

        // If there's no more space on this page, move to the next page.
        // (This will only occur if the asked-for size filled up exactly to
        // the end of the page.)  This ensures that next time we enter this
        // function, _unused won't be pointing one byte past the end of
        // the page, which would break everything.
        if (_unused > pageBottom(startOfRoom)) {
            // Check we only spilled over by one byte.
            NanoAssert(_unused == pageTop(_unused));
            NanoAssert(_unused == pageBottom(startOfRoom) + 1);
            uintptr_t addrOfLastLInsOnPage = _unused - sizeof(LIns);
            moveToNewPage(addrOfLastLInsOnPage);
        }

        // Make sure it's word-aligned.
        NanoAssert(0 == startOfRoom % sizeof(void*));
        return startOfRoom;
    }

    LInsp LirBufWriter::insStorei(LInsp val, LInsp base, int32_t d)
    {
        LOpcode op = val->isQuad() ? LIR_stqi : LIR_sti;
        LInsSti* insSti = (LInsSti*)_buf->makeRoom(sizeof(LInsSti));
        LIns*    ins    = insSti->getLIns();
        ins->initLInsSti(op, val, base, d);
        return ins;
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

    LInsp LirBufWriter::insLoad(LOpcode op, LInsp base, int32_t d)
    {
        LInsLd* insLd = (LInsLd*)_buf->makeRoom(sizeof(LInsLd));
        LIns*   ins   = insLd->getLIns();
        ins->initLInsLd(op, base, d);
        return ins;
    }

    LInsp LirBufWriter::insGuard(LOpcode op, LInsp c, LInsp data)
    {
        return ins2(op, c, data);
    }

    LInsp LirBufWriter::insBranch(LOpcode op, LInsp condition, LInsp toLabel)
    {
        NanoAssert(condition);
        return ins2(op, condition, toLabel);
    }

    LInsp LirBufWriter::insAlloc(int32_t size)
    {
        size = (size+3)>>2; // # of required 32bit words
        LInsI* insI = (LInsI*)_buf->makeRoom(sizeof(LInsI));
        LIns*  ins  = insI->getLIns();
        ins->initLInsI(LIR_ialloc, size);
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
            _buf->explicitSavedRegs = true;
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

    LInsp LirBufWriter::insImmq(uint64_t imm)
    {
        LInsI64* insI64 = (LInsI64*)_buf->makeRoom(sizeof(LInsI64));
        LIns*    ins    = insI64->getLIns();
        ins->initLInsI64(LIR_quad, imm);
        return ins;
    }

    LInsp LirBufWriter::insSkip(size_t payload_szB)
    {
        // First, round up payload_szB to a multiple of the word size.  To
        // ensure that the rounding up won't cause it to exceed
        // NJ_MAX_SKIP_PAYLOAD_SZB, NJ_MAX_SKIP_PAYLOAD_SZB must also be a
        // multiple of the word size, which we check.
        payload_szB = alignUp(payload_szB, sizeof(void*));
        NanoAssert(0 == NJ_MAX_SKIP_PAYLOAD_SZB % sizeof(void*));
        NanoAssert(sizeof(void*) <= payload_szB && payload_szB <= NJ_MAX_SKIP_PAYLOAD_SZB);

        uintptr_t payload = _buf->makeRoom(payload_szB + sizeof(LInsSk));
        uintptr_t prevLInsAddr = payload - sizeof(LIns);
        LInsSk* insSk = (LInsSk*)(payload + payload_szB);
        LIns*   ins   = insSk->getLIns();
        NanoAssert(prevLInsAddr >= pageDataStart(prevLInsAddr));
        NanoAssert(samepage(prevLInsAddr, insSk));
        ins->initLInsSk((LInsp)prevLInsAddr);
        return ins;
    }

    // Reads the next non-skip instruction.
    LInsp LirReader::read()
    {
        NanoAssert(_i);
        LInsp cur = _i;
        uintptr_t i = uintptr_t(cur);
        LOpcode iop = ((LInsp)i)->opcode();

        // We pass over skip instructions below.  Also, the last instruction
        // for a fragment shouldn't be a skip(*).  Therefore we shouldn't see
        // a skip here.
        //
        // (*) Actually, if the last *inserted* instruction exactly fills up a
        // page, a new page will be created, and thus the last *written*
        // instruction will be a skip -- the one needed for the cross-page
        // link.  But the last *inserted* instruction is what is recorded and
        // used to initialise each LirReader, and that is what is seen here,
        // and therefore this assertion holds.
        NanoAssert(iop != LIR_skip);

        do
        {
            // Nb: this switch is table-driven (because sizeof_LInsXYZ() is
            // table-driven) in most cases to avoid branch mispredictions --
            // if we do a vanilla switch on the iop or LInsRepKind the extra
            // branch mispredictions cause a small but noticeable slowdown.
            switch (iop)
            {
                default:
                    i -= insSizes[((LInsp)i)->opcode()];
                    break;

#if defined NANOJIT_64BIT
                case LIR_callh:
#endif
                case LIR_call:
                case LIR_fcall: {
                    int argc = ((LInsp)i)->argc();
                    i -= sizeof(LInsC);         // step over the instruction
                    i -= argc*sizeof(LInsp);    // step over the arguments
                    NanoAssert( samepage(i, _i) );
                    break;
                }

                case LIR_skip:
                    // Ignore the skip, move onto its predecessor.
                    NanoAssert(((LInsp)i)->prevLIns() != (LInsp)i);
                    i = uintptr_t(((LInsp)i)->prevLIns());
                    break;

                case LIR_start:
                    // Once we hit here, this method shouldn't be called again.
                    // The assertion at the top of this method checks this.
                    _i = 0;
                    return cur;
            }
            iop = ((LInsp)i)->opcode();
        }
        while (LIR_skip == iop);
        _i = (LInsp)i;
        return cur;
    }

    bool LIns::isFloat() const {
        switch (opcode()) {
            default:
                return false;
            case LIR_fadd:
            case LIR_fsub:
            case LIR_fmul:
            case LIR_fdiv:
            case LIR_fneg:
            case LIR_fcall:
            case LIR_i2f:
            case LIR_u2f:
                return true;
        }
    }

    bool LIns::isLInsOp0() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_Op0 == repKinds[opcode()];
    }

    bool LIns::isLInsOp1() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_Op1 == repKinds[opcode()];
    }

    bool LIns::isLInsOp2() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_Op2 == repKinds[opcode()];
    }

    bool LIns::isLInsOp3() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_Op3 == repKinds[opcode()];
    }

    bool LIns::isLInsLd() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_Ld == repKinds[opcode()];
    }

    bool LIns::isLInsSti() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_Sti == repKinds[opcode()];
    }

    bool LIns::isLInsSk() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_Sk == repKinds[opcode()];
    }

    bool LIns::isLInsC() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_C == repKinds[opcode()];
    }

    bool LIns::isLInsP() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_P == repKinds[opcode()];
    }

    bool LIns::isLInsI() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_I == repKinds[opcode()];
    }

    bool LIns::isLInsI64() const {
        NanoAssert(LRK_None != repKinds[opcode()]);
        return LRK_I64 == repKinds[opcode()];
    }

    bool LIns::isCmp() const {
        LOpcode op = opcode();
        return (op >= LIR_eq && op <= LIR_uge) || (op >= LIR_feq && op <= LIR_fge);
    }

    bool LIns::isCond() const {
        LOpcode op = opcode();
        return (op == LIR_ov) || isCmp();
    }

    bool LIns::isQuad() const {
#ifdef AVMPLUS_64BIT
        // callh in 64bit cpu's means a call that returns an int64 in a single register
        return (opcode() & LIR64) != 0 || opcode() == LIR_callh;
#else
        // callh in 32bit cpu's means the 32bit MSW of an int64 result in 2 registers
        return (opcode() & LIR64) != 0;
#endif
    }

    bool LIns::isconstval(int32_t val) const
    {
        return isconst() && imm32()==val;
    }

    bool LIns::isconstq() const
    {
        return opcode() == LIR_quad;
    }

    bool LIns::isconstp() const
    {
#ifdef AVMPLUS_64BIT
        return isconstq();
#else
        return isconst();
#endif
    }

    bool LIns::isCse() const
    {
        return nanojit::isCseOpcode(opcode()) || (isCall() && callInfo()->_cse);
    }

    void LIns::setTarget(LInsp label)
    {
        NanoAssert(label && label->isop(LIR_label));
        NanoAssert(isBranch());
        toLInsOp2()->oprnd_2 = label;
    }

    LInsp LIns::getTarget()
    {
        NanoAssert(isBranch());
        return oprnd2();
    }

    void *LIns::payload() const
    {
        NanoAssert(isop(LIR_skip));
        // Operand 1 points to the previous LIns;  we move past it to get to
        // the payload.
        return (void*) (uintptr_t(prevLIns()) + sizeof(LIns));
    }

    uint64_t LIns::imm64() const
    {
        NanoAssert(isconstq());
        return (uint64_t(toLInsI64()->imm64_1) << 32) | uint32_t(toLInsI64()->imm64_0);
    }

    double LIns::imm64f() const
    {
        union {
            double f;
            uint64_t q;
        } u;
        u.q = imm64();
        return u.f;
    }

    const CallInfo* LIns::callInfo() const
    {
        NanoAssert(isCall());
        return toLInsC()->ci;
    }

    // Index args in r-l order.  arg(0) is rightmost arg.
    // Nb: this must be kept in sync with insCall().
    LInsp LIns::arg(uint32_t i)
    {
        NanoAssert(isCall());
        NanoAssert(i < argc());
        // Move to the start of the LInsC, then move back one word per argument.
        LInsp* argSlot = (LInsp*)(uintptr_t(toLInsC()) - (i+1)*sizeof(void*));
        return *argSlot;
    }

    LIns* LirWriter::ins2i(LOpcode v, LIns* oprnd1, int32_t imm)
    {
        return ins2(v, oprnd1, insImm(imm));
    }

    bool insIsS16(LInsp i)
    {
        if (i->isconst()) {
            int c = i->imm32();
            return isS16(c);
        }
        if (i->isop(LIR_cmov) || i->isop(LIR_qcmov)) {
            return insIsS16(i->oprnd2()) && insIsS16(i->oprnd3());
        }
        if (i->isCmp())
            return true;
        // many other possibilities too.
        return false;
    }

    LIns* ExprFilter::ins1(LOpcode v, LIns* i)
    {
        switch (v) {
        case LIR_qlo:
            if (i->isconstq())
                return insImm(i->imm64_0());
            if (i->isop(LIR_qjoin))
                return i->oprnd1();
            break;
        case LIR_qhi:
            if (i->isconstq())
                return insImm(i->imm64_1());
            if (i->isop(LIR_qjoin))
                return i->oprnd2();
            break;
        case LIR_not:
            if (i->isconst())
                return insImm(~i->imm32());
        involution:
            if (v == i->opcode())
                return i->oprnd1();
            break;
        case LIR_neg:
            if (i->isconst())
                return insImm(-i->imm32());
            if (i->isop(LIR_sub)) // -(a-b) = b-a
                return out->ins2(LIR_sub, i->oprnd2(), i->oprnd1());
            goto involution;
        case LIR_fneg:
            if (i->isconstq())
                return insImmf(-i->imm64f());
            if (i->isop(LIR_fsub))
                return out->ins2(LIR_fsub, i->oprnd2(), i->oprnd1());
            goto involution;
        case LIR_i2f:
            if (i->isconst())
                return insImmf(i->imm32());
            break;
        case LIR_u2f:
            if (i->isconst())
                return insImmf(uint32_t(i->imm32()));
            break;
        default:
            ;
        }

        return out->ins1(v, i);
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
            uint64_t q;

            switch (v) {
            case LIR_qjoin:
                q = c1 | uint64_t(c2)<<32;
                return insImmq(q);
            case LIR_eq:
                return insImm(c1 == c2);
            case LIR_ov:
                return insImm((c2 != 0) && ((c1 + c2) <= c1));
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
            case LIR_div:
            case LIR_mod:
                // We can't easily fold div and mod, since folding div makes it
                // impossible to calculate the mod that refers to it. The
                // frontend shouldn't emit div and mod with constant operands.
                NanoAssert(0);
            default:
                ;
            }
        }
        else if (oprnd1->isconstq() && oprnd2->isconstq())
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
            case LIR_iaddp:
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
                if (v >= LIR_lt && v <= LIR_uge) {
                    NanoStaticAssert((LIR_lt ^ 1) == LIR_gt);
                    NanoStaticAssert((LIR_le ^ 1) == LIR_ge);
                    NanoStaticAssert((LIR_ult ^ 1) == LIR_ugt);
                    NanoStaticAssert((LIR_ule ^ 1) == LIR_uge);

                    // move const to rhs, swap the operator
                    LIns *t = oprnd2;
                    oprnd2 = oprnd1;
                    oprnd1 = t;
                    v = LOpcode(v^1);
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
                case LIR_iaddp:
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
            }
        }

        LInsp i;
        if (v == LIR_qjoin && oprnd1->isop(LIR_qlo) && oprnd2->isop(LIR_qhi) &&
            (i = oprnd1->oprnd1()) == oprnd2->oprnd1()) {
            // qjoin(qlo(x),qhi(x)) == x
            return i;
        }

        return out->ins2(v, oprnd1, oprnd2);
    }

    LIns* ExprFilter::ins3(LOpcode v, LIns* oprnd1, LIns* oprnd2, LIns* oprnd3)
    {
        NanoAssert(oprnd1 && oprnd2 && oprnd3);
        NanoAssert(v == LIR_cmov || v == LIR_qcmov);
        if (oprnd2 == oprnd3) {
            // c ? a : a => a
            return oprnd2;
        }
        if (oprnd1->isconst()) {
            // const ? x : y => return x or y depending on const
            return oprnd1->imm32() ? oprnd2 : oprnd3;
        }

        return out->ins3(v, oprnd1, oprnd2, oprnd3);
    }

    LIns* ExprFilter::insGuard(LOpcode v, LInsp c, LInsp x)
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
                    return out->insGuard(LIR_x, NULL, x);
                }
            }
            else {
                NanoStaticAssert((LIR_xt ^ 1) == LIR_xf);
                while (c->isop(LIR_eq) && c->oprnd1()->isCmp() &&
                    c->oprnd2()->isconstval(0)) {
                    // xt(eq(cmp,0)) => xf(cmp)   or   xf(eq(cmp,0)) => xt(cmp)
                    v = LOpcode(v^1);
                    c = c->oprnd1();
                }
            }
        }
        return out->insGuard(v, c, x);
    }

    LIns* ExprFilter::insBranch(LOpcode v, LIns *c, LIns *t)
    {
        switch (v) {
        case LIR_jt:
        case LIR_jf:
            while (c->isop(LIR_eq) && c->oprnd1()->isCmp() && c->oprnd2()->isconstval(0)) {
                // jt(eq(cmp,0)) => jf(cmp)   or   jf(eq(cmp,0)) => jt(cmp)
                v = LOpcode(v ^ 1);
                c = c->oprnd1();
            }
            break;
        default:
            ;
        }
        return out->insBranch(v, c, t);
    }

    LIns* LirWriter::ins_eq0(LIns* oprnd1)
    {
        return ins2i(LIR_eq, oprnd1, 0);
    }

    LIns* LirWriter::insImmf(double f)
    {
        union {
            double f;
            uint64_t q;
        } u;
        u.f = f;
        return insImmq(u.q);
    }

    LIns* LirWriter::qjoin(LInsp lo, LInsp hi)
    {
        return ins2(LIR_qjoin, lo, hi);
    }

    LIns* LirWriter::insImmPtr(const void *ptr)
    {
        return sizeof(ptr) == 8 ? insImmq((uintptr_t)ptr) : insImm((intptr_t)ptr);
    }

    LIns* LirWriter::ins_choose(LIns* cond, LIns* iftrue, LIns* iffalse)
    {
        // if not a conditional, make it implicitly an ==0 test (then flop results)
        if (!cond->isCmp())
        {
            cond = ins_eq0(cond);
            LInsp tmp = iftrue;
            iftrue = iffalse;
            iffalse = tmp;
        }

        if (avmplus::AvmCore::use_cmov())
            return ins3((iftrue->isQuad() || iffalse->isQuad()) ? LIR_qcmov : LIR_cmov, cond, iftrue, iffalse);

        LInsp ncond = ins1(LIR_neg, cond); // cond ? -1 : 0
        return ins2(LIR_or,
                    ins2(LIR_and, iftrue, ncond),
                    ins2(LIR_and, iffalse, ins1(LIR_not, ncond)));
    }

    LIns* LirBufWriter::insCall(const CallInfo *ci, LInsp args[])
    {
        static const LOpcode k_callmap[] = {
        //  ARGSIZE_NONE  ARGSIZE_F  ARGSIZE_LO  ARGSIZE_Q  (4)        (5)        ARGSIZE_U  (7)
            LIR_call,     LIR_fcall, LIR_call,   LIR_callh, LIR_skip,  LIR_skip,  LIR_call,  LIR_skip
        };

        uint32_t argt = ci->_argtypes;
        LOpcode op = k_callmap[argt & ARGSIZE_MASK_ANY];
        NanoAssert(op != LIR_skip); // LIR_skip here is just an error condition

        ArgSize sizes[MAXARGS];
        int32_t argc = ci->get_sizes(sizes);

        if (AvmCore::config.soft_float) {
            if (op == LIR_fcall)
                op = LIR_callh;
        }

        NanoAssert(argc <= (int)MAXARGS);

        // Lay the call parameters out (in reverse order).
        // Nb: this must be kept in sync with arg().
        LInsp* newargs = (LInsp*)_buf->makeRoom(argc*sizeof(LInsp) + sizeof(LInsC)); // args + call
        for (int32_t i = 0; i < argc; i++)
            newargs[argc - i - 1] = args[i];

        // Write the call instruction itself.
        LInsC* insC = (LInsC*)(uintptr_t(newargs) + argc*sizeof(LInsp));
        LIns*  ins  = insC->getLIns();
#ifndef NANOJIT_64BIT
        ins->initLInsC(op==LIR_callh ? LIR_call : op, argc, ci);
#else
        ins->initLInsC(op, argc, ci);
#endif
        return ins;
    }

    using namespace avmplus;

    StackFilter::StackFilter(LirFilter *in, GC *gc, LirBuffer *lirbuf, LInsp sp)
        : LirFilter(in), gc(gc), lirbuf(lirbuf), sp(sp), top(0)
    {}

    LInsp StackFilter::read()
    {
        for (;;)
        {
            LInsp i = in->read();
            if (i->isStore())
            {
                LInsp base = i->oprnd2();
                if (base == sp)
                {
                    LInsp v = i->oprnd1();
                    int d = i->disp() >> 2;
                    if (d >= top) {
                        continue;
                    } else {
                        d = top - d;
                        if (v->isQuad()) {
                            // storing 8 bytes
                            if (stk.get(d) && stk.get(d-1)) {
                                continue;
                            } else {
                                stk.set(gc, d);
                                stk.set(gc, d-1);
                            }
                        }
                        else {
                            // storing 4 bytes
                            if (stk.get(d))
                                continue;
                            else
                                stk.set(gc, d);
                        }
                    }
                }
            }
            /*
             * NB: If there is a backward branch other than the loop-restart branch, this is
             * going to be wrong. Unfortunately there doesn't seem to be an easy way to detect
             * such branches. Just do not create any.
             */
            else if (i->isGuard())
            {
                stk.reset();
                top = getTop(i) >> 2;
            }
            return i;
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

    LInsHashSet::LInsHashSet(GC* gc) :
            m_used(0), m_cap(kInitialCap), m_gc(gc)
    {
#ifdef MEMORY_INFO
//        m_list.set_meminfo_name("LInsHashSet.list");
#endif
        LInsp *list = (LInsp*) gc->Alloc(sizeof(LInsp)*m_cap, GC::kZero);
        WB(gc, this, &m_list, list);
    }

    LInsHashSet::~LInsHashSet()
    {
        m_gc->Free(m_list);
    }

    void LInsHashSet::clear() {
        memset(m_list, 0, sizeof(LInsp)*m_cap);
        m_used = 0;
    }

    /*static*/ uint32_t FASTCALL LInsHashSet::hashcode(LInsp i)
    {
        const LOpcode op = i->opcode();
        switch (op)
        {
            case LIR_int:
                return hashimm(i->imm32());

            case LIR_quad:
                return hashimmq(i->imm64());

            case LIR_call:
            case LIR_fcall:
#if defined NANOJIT_64BIT
            case LIR_callh:
#endif
            {
                LInsp args[10];
                int32_t argc = i->argc();
                NanoAssert(argc < 10);
                for (int32_t j=0; j < argc; j++)
                    args[j] = i->arg(j);
                return hashcall(i->callInfo(), argc, args);
            }

            case LIR_ld:
            case LIR_ldcb:
            case LIR_ldcs:
            case LIR_ldc:
            case LIR_ldq:
            case LIR_ldqc:
                return hashLoad(op, i->oprnd1(), i->disp());

            default:
                if (operandCount[op] == 3)
                    return hash3(op, i->oprnd1(), i->oprnd2(), i->oprnd3());
                else if (operandCount[op] == 2)
                    return hash2(op, i->oprnd1(), i->oprnd2());
                else
                    return hash1(op, i->oprnd1());
        }
    }

    /*static*/ bool FASTCALL LInsHashSet::equals(LInsp a, LInsp b)
    {
        if (a==b)
            return true;
        if (a->opcode() != b->opcode())
            return false;
        AvmAssert(a->opcode() == b->opcode());
        const LOpcode op = a->opcode();
        switch (op)
        {
            case LIR_int:
            {
                return a->imm32() == b->imm32();
            }
            case LIR_quad:
            {
                return a->imm64() == b->imm64();
            }
            case LIR_call:
            case LIR_fcall:
#if defined NANOJIT_64BIT
            case LIR_callh:
#endif
            {
                if (a->callInfo() != b->callInfo()) return false;
                uint32_t argc=a->argc();
                NanoAssert(argc == b->argc());
                for (uint32_t i=0; i < argc; i++)
                    if (a->arg(i) != b->arg(i))
                        return false;
                return true;
            }

            case LIR_ld:
            case LIR_ldcb:
            case LIR_ldcs:
            case LIR_ldc:
            case LIR_ldq:
            case LIR_ldqc:
                return
                    (a->oprnd1() == b->oprnd1() && a->disp() == b->disp() ? true : false );

            default:
            {
                const uint32_t count = operandCount[op];
                if ((count >= 1 && a->oprnd1() != b->oprnd1()) ||
                    (count >= 2 && a->oprnd2() != b->oprnd2()) ||
                    (count >= 3 && a->oprnd3() != b->oprnd3()))
                    return false;
                return true;
            }
        }
    }

    void FASTCALL LInsHashSet::grow()
    {
        const uint32_t newcap = m_cap << 1;
        LInsp *newlist = (LInsp*) m_gc->Alloc(newcap * sizeof(LInsp), GC::kZero);
        LInsp *list = m_list;
#ifdef MEMORY_INFO
//        newlist.set_meminfo_name("LInsHashSet.list");
#endif
        for (uint32_t i=0, n=m_cap; i < n; i++) {
            LInsp name = list[i];
            if (!name) continue;
            uint32_t j = find(name, hashcode(name), newlist, newcap);
            newlist[j] = name;
        }
        m_cap = newcap;
        m_gc->Free(list);
        WB(m_gc, this, &m_list, newlist);
    }

    uint32_t FASTCALL LInsHashSet::find(LInsp name, uint32_t hash, const LInsp *list, uint32_t cap)
    {
        const uint32_t bitmask = (cap - 1) & ~0x1;

        uint32_t n = 7 << 1;
        hash &= bitmask;
        LInsp k;
        while ((k = list[hash]) != NULL && !equals(k, name))
        {
            hash = (hash + (n += 2)) & bitmask;        // quadratic probe
        }
        return hash;
    }

    LInsp LInsHashSet::add(LInsp name, uint32_t k)
    {
        // this is relatively short-lived so let's try a more aggressive load factor
        // in the interest of improving performance
        if (((m_used+1)<<1) >= m_cap) // 0.50
        {
            grow();
            k = find(name, hashcode(name), m_list, m_cap);
        }
        NanoAssert(!m_list[k]);
        m_used++;
        return m_list[k] = name;
    }

    void LInsHashSet::replace(LInsp i)
    {
        LInsp *list = m_list;
        uint32_t k = find(i, hashcode(i), list, m_cap);
        if (list[k]) {
            // already there, so replace it
            list[k] = i;
        } else {
            add(i, k);
        }
    }

    uint32_t LInsHashSet::hashimm(int32_t a) {
        return _hashfinish(_hash32(0,a));
    }

    uint32_t LInsHashSet::hashimmq(uint64_t a) {
        uint32_t hash = _hash32(0, uint32_t(a >> 32));
        return _hashfinish(_hash32(hash, uint32_t(a)));
    }

    uint32_t LInsHashSet::hash1(LOpcode op, LInsp a) {
        uint32_t hash = _hash8(0,uint8_t(op));
        return _hashfinish(_hashptr(hash, a));
    }

    uint32_t LInsHashSet::hash2(LOpcode op, LInsp a, LInsp b) {
        uint32_t hash = _hash8(0,uint8_t(op));
        hash = _hashptr(hash, a);
        return _hashfinish(_hashptr(hash, b));
    }

    uint32_t LInsHashSet::hash3(LOpcode op, LInsp a, LInsp b, LInsp c) {
        uint32_t hash = _hash8(0,uint8_t(op));
        hash = _hashptr(hash, a);
        hash = _hashptr(hash, b);
        return _hashfinish(_hashptr(hash, c));
    }

    uint32_t LInsHashSet::hashLoad(LOpcode op, LInsp a, int32_t d) {
        uint32_t hash = _hash8(0,uint8_t(op));
        hash = _hashptr(hash, a);
        return _hashfinish(_hash32(hash, d));
    }

    uint32_t LInsHashSet::hashcall(const CallInfo *ci, uint32_t argc, LInsp args[]) {
        uint32_t hash = _hashptr(0, ci);
        for (int32_t j=argc-1; j >= 0; j--)
            hash = _hashptr(hash,args[j]);
        return _hashfinish(hash);
    }

    LInsp LInsHashSet::find32(int32_t a, uint32_t &i)
    {
        uint32_t cap = m_cap;
        const LInsp *list = m_list;
        const uint32_t bitmask = (cap - 1) & ~0x1;
        uint32_t hash = hashimm(a) & bitmask;
        uint32_t n = 7 << 1;
        LInsp k;
        while ((k = list[hash]) != NULL &&
            (!k->isconst() || k->imm32() != a))
        {
            hash = (hash + (n += 2)) & bitmask;        // quadratic probe
        }
        i = hash;
        return k;
    }

    LInsp LInsHashSet::find64(uint64_t a, uint32_t &i)
    {
        uint32_t cap = m_cap;
        const LInsp *list = m_list;
        const uint32_t bitmask = (cap - 1) & ~0x1;
        uint32_t hash = hashimmq(a) & bitmask;
        uint32_t n = 7 << 1;
        LInsp k;
        while ((k = list[hash]) != NULL &&
            (!k->isconstq() || k->imm64() != a))
        {
            hash = (hash + (n += 2)) & bitmask;        // quadratic probe
        }
        i = hash;
        return k;
    }

    LInsp LInsHashSet::find1(LOpcode op, LInsp a, uint32_t &i)
    {
        uint32_t cap = m_cap;
        const LInsp *list = m_list;
        const uint32_t bitmask = (cap - 1) & ~0x1;
        uint32_t hash = hash1(op,a) & bitmask;
        uint32_t n = 7 << 1;
        LInsp k;
        while ((k = list[hash]) != NULL &&
            (k->opcode() != op || k->oprnd1() != a))
        {
            hash = (hash + (n += 2)) & bitmask;        // quadratic probe
        }
        i = hash;
        return k;
    }

    LInsp LInsHashSet::find2(LOpcode op, LInsp a, LInsp b, uint32_t &i)
    {
        uint32_t cap = m_cap;
        const LInsp *list = m_list;
        const uint32_t bitmask = (cap - 1) & ~0x1;
        uint32_t hash = hash2(op,a,b) & bitmask;
        uint32_t n = 7 << 1;
        LInsp k;
        while ((k = list[hash]) != NULL &&
            (k->opcode() != op || k->oprnd1() != a || k->oprnd2() != b))
        {
            hash = (hash + (n += 2)) & bitmask;        // quadratic probe
        }
        i = hash;
        return k;
    }

    LInsp LInsHashSet::find3(LOpcode op, LInsp a, LInsp b, LInsp c, uint32_t &i)
    {
        uint32_t cap = m_cap;
        const LInsp *list = m_list;
        const uint32_t bitmask = (cap - 1) & ~0x1;
        uint32_t hash = hash3(op,a,b,c) & bitmask;  
        uint32_t n = 7 << 1;
        LInsp k;
        while ((k = list[hash]) != NULL && 
            (k->opcode() != op || k->oprnd1() != a || k->oprnd2() != b || k->oprnd3() != c))
        {
            hash = (hash + (n += 2)) & bitmask;     // quadratic probe
        }
        i = hash;
        return k;
    }

    LInsp LInsHashSet::findLoad(LOpcode op, LInsp a, int32_t d, uint32_t &i)
    {
        uint32_t cap = m_cap;
        const LInsp *list = m_list;
        const uint32_t bitmask = (cap - 1) & ~0x1;
        uint32_t hash = hashLoad(op,a,d) & bitmask;
        uint32_t n = 7 << 1;
        LInsp k;
        while ((k = list[hash]) != NULL &&
            (k->opcode() != op || k->oprnd1() != a || k->disp() != d))
        {
            hash = (hash + (n += 2)) & bitmask;        // quadratic probe
        }
        i = hash;
        return k;
    }

    bool argsmatch(LInsp i, uint32_t argc, LInsp args[])
    {
        for (uint32_t j=0; j < argc; j++)
            if (i->arg(j) != args[j])
                return false;
        return true;
    }

    LInsp LInsHashSet::findcall(const CallInfo *ci, uint32_t argc, LInsp args[], uint32_t &i)
    {
        uint32_t cap = m_cap;
        const LInsp *list = m_list;
        const uint32_t bitmask = (cap - 1) & ~0x1;
        uint32_t hash = hashcall(ci, argc, args) & bitmask;
        uint32_t n = 7 << 1;
        LInsp k;
        while ((k = list[hash]) != NULL &&
            (!k->isCall() || k->callInfo() != ci || !argsmatch(k, argc, args)))
        {
            hash = (hash + (n += 2)) & bitmask;        // quadratic probe
        }
        i = hash;
        return k;
    }

    GuardRecord *LIns::record()
    {
        NanoAssert(isGuard());
        return (GuardRecord*)oprnd2()->payload();
    }

#ifdef NJ_VERBOSE
    class RetiredEntry: public GCObject
    {
    public:
        List<LInsp, LIST_NonGCObjects> live;
        LInsp i;
        RetiredEntry(GC *gc): live(gc) {}
    };
    class LiveTable
    {
    public:
        SortedMap<LInsp,LInsp,LIST_NonGCObjects> live;
        List<RetiredEntry*, LIST_GCObjects> retired;
        int maxlive;
        LiveTable(GC *gc) : live(gc), retired(gc), maxlive(0) {}
        ~LiveTable()
        {
            for (size_t i = 0; i < retired.size(); i++) {
                NJ_DELETE(retired.get(i));
            }

        }
        void add(LInsp i, LInsp use) {
            if (!i->isconst() && !i->isconstq() && !live.containsKey(i)) {
                NanoAssert(size_t(i->opcode()) < sizeof(lirNames) / sizeof(lirNames[0]));
                live.put(i,use);
            }
        }
        void retire(LInsp i, GC *gc) {
            RetiredEntry *e = NJ_NEW(gc, RetiredEntry)(gc);
            e->i = i;
            for (int j=0, n=live.size(); j < n; j++) {
                LInsp ins = live.keyAt(j);
                if (!ins->isStore() && !ins->isGuard())
                    e->live.add(ins);
            }
            int size=0;
            if ((size = e->live.size()) > maxlive)
                maxlive = size;

            live.remove(i);
            retired.add(e);
        }
        bool contains(LInsp i) {
            return live.containsKey(i);
        }
    };

    void live(GC *gc, Fragment *frag, LogControl *logc)
    {
        // traverse backwards to find live exprs and a few other stats.

        LiveTable live(gc);
        uint32_t exits = 0;
        LirReader br(frag->lastIns);
        StackFilter sf(&br, gc, frag->lirbuf, frag->lirbuf->sp);
        StackFilter r(&sf, gc, frag->lirbuf, frag->lirbuf->rp);
        int total = 0;
        if (frag->lirbuf->state)
            live.add(frag->lirbuf->state, r.pos());
        for (LInsp i = r.read(); !i->isop(LIR_start); i = r.read())
        {
            total++;

            // first handle side-effect instructions
            if (!i->isCse())
            {
                live.add(i,0);
                if (i->isGuard())
                    exits++;
            }

            // now propagate liveness
            if (live.contains(i))
            {
                live.retire(i,gc);
                NanoAssert(size_t(i->opcode()) < sizeof(operandCount) / sizeof(operandCount[0]));
                if (i->isStore()) {
                    live.add(i->oprnd2(),i); // base
                    live.add(i->oprnd1(),i); // val
                }
                else if (i->isop(LIR_cmov) || i->isop(LIR_qcmov)) {
                    live.add(i->oprnd1(),i);
                    live.add(i->oprnd2(),i);
                    live.add(i->oprnd3(),i);
                }
                else if (operandCount[i->opcode()] == 1) {
                    live.add(i->oprnd1(),i);
                }
                else if (operandCount[i->opcode()] == 2) {
                    live.add(i->oprnd1(),i);
                    live.add(i->oprnd2(),i);
                }
                else if (i->isCall()) {
                    for (int j=0, c=i->argc(); j < c; j++)
                        live.add(i->arg(j),i);
                }
            }
        }

        logc->printf("  Live instruction count %d, total %u, max pressure %d\n",
                     live.retired.size(), total, live.maxlive);
        logc->printf("  Side exits %u\n", exits);
        logc->printf("  Showing LIR instructions with live-after variables\n");
        logc->printf("\n");

        // print live exprs, going forwards
        LirNameMap *names = frag->lirbuf->names;
        bool newblock = true;
        for (int j=live.retired.size()-1; j >= 0; j--)
        {
            RetiredEntry *e = live.retired[j];
            char livebuf[4000], *s=livebuf;
            *s = 0;
            if (!newblock && e->i->isop(LIR_label)) {
                logc->printf("\n");
            }
            newblock = false;
            for (int k=0,n=e->live.size(); k < n; k++) {
                strcpy(s, names->formatRef(e->live[k]));
                s += strlen(s);
                *s++ = ' '; *s = 0;
                NanoAssert(s < livebuf+sizeof(livebuf));
            }
            /* If the LIR insn is pretty short, print it and its
               live-after set on the same line.  If not, put
               live-after set on a new line, suitably indented. */
            const char* insn_text = names->formatIns(e->i);
            if (strlen(insn_text) >= 30-2) {
                logc->printf("  %-30s\n  %-30s %s\n", names->formatIns(e->i), "", livebuf);
            } else {
                logc->printf("  %-30s %s\n", names->formatIns(e->i), livebuf);
            }

            if (e->i->isGuard() || e->i->isBranch() || e->i->isRet()) {
                logc->printf("\n");
                newblock = true;
            }
        }
    }

    LabelMap::Entry::~Entry()
    {
    }

    LirNameMap::Entry::~Entry()
    {
    }

    LirNameMap::~LirNameMap()
    {
        Entry *e;

        while ((e = names.removeLast()) != NULL) {
            labels->core->freeString(e->name);
            NJ_DELETE(e);
        }
    }

    bool LirNameMap::addName(LInsp i, Stringp name) {
        if (!names.containsKey(i)) {
            Entry *e = NJ_NEW(labels->core->gc, Entry)(name);
            names.put(i, e);
            return true;
        }
        return false;
    }
    void LirNameMap::addName(LInsp i, const char *name) {
        Stringp new_name = labels->core->newString(name);
        if (!addName(i, new_name)) {
            labels->core->freeString(new_name);
        }
    }

    void LirNameMap::copyName(LInsp i, const char *s, int suffix) {
        char s2[200];
        if (isdigit(s[strlen(s)-1])) {
            // if s ends with a digit, add '_' to clarify the suffix
            sprintf(s2,"%s_%d", s, suffix);
        } else {
            sprintf(s2,"%s%d", s, suffix);
        }
        addName(i, labels->core->newString(s2));
    }

    void LirNameMap::formatImm(int32_t c, char *buf) {
        if (c >= 10000 || c <= -10000)
            sprintf(buf,"#%s",labels->format((void*)c));
        else
            sprintf(buf,"%d", c);
    }

    const char* LirNameMap::formatRef(LIns *ref)
    {
        char buffer[200], *buf=buffer;
        buf[0]=0;
        GC *gc = labels->core->gc;
        if (names.containsKey(ref)) {
            StringNullTerminatedUTF8 cname(gc, names.get(ref)->name);
            strcat(buf, cname.c_str());
        }
        else if (ref->isconstq()) {
#if defined NANOJIT_64BIT
            sprintf(buf, "#0x%lx", (nj_printf_ld)ref->imm64());
#else
            formatImm(ref->imm64_1(), buf);
            buf += strlen(buf);
            *buf++ = ':';
            formatImm(ref->imm64_0(), buf);
#endif
        }
        else if (ref->isconst()) {
            formatImm(ref->imm32(), buf);
        }
        else {
            if (ref->isCall()) {
#if !defined NANOJIT_64BIT
                if (ref->isop(LIR_callh)) {
                    // we've presumably seen the other half already
                    ref = ref->oprnd1();
                } else {
#endif
                    copyName(ref, ref->callInfo()->_name, funccounts.add(ref->callInfo()));
#if !defined NANOJIT_64BIT
                }
#endif
            } else {
                NanoAssert(size_t(ref->opcode()) < sizeof(lirNames) / sizeof(lirNames[0]));
                copyName(ref, lirNames[ref->opcode()], lircounts.add(ref->opcode()));
            }
            StringNullTerminatedUTF8 cname(gc, names.get(ref)->name);
            strcat(buf, cname.c_str());
        }
        return labels->dup(buffer);
    }

    const char* LirNameMap::formatIns(LIns* i)
    {
        char sbuf[200];
        char *s = sbuf;
        LOpcode op = i->opcode();
        switch(op)
        {
            case LIR_int:
            {
                sprintf(s, "%s", formatRef(i));
                break;
            }

            case LIR_ialloc: {
                sprintf(s, "%s = %s %d", formatRef(i), lirNames[op], i->size());
                break;
            }

            case LIR_quad:
            {
                sprintf(s, "#%X:%X /* %g */", i->imm64_1(), i->imm64_0(), i->imm64f());
                break;
            }

            case LIR_loop:
            case LIR_start:
                sprintf(s, "%s", lirNames[op]);
                break;

#if defined NANOJIT_64BIT
            case LIR_callh:
#endif
            case LIR_fcall:
            case LIR_call: {
                const CallInfo* call = i->callInfo();
                int32_t argc = i->argc();
                if (call->isIndirect())
                    sprintf(s, "%s = %s [%s] ( ", formatRef(i), lirNames[op], formatRef(i->arg(--argc)));
                else
                    sprintf(s, "%s = %s #%s ( ", formatRef(i), lirNames[op], call->_name);
                for (int32_t j = argc - 1; j >= 0; j--) {
                    s += strlen(s);
                    sprintf(s, "%s ",formatRef(i->arg(j)));
                }
                s += strlen(s);
                sprintf(s, ")");
                break;
            }

            case LIR_iparam: {
                uint32_t arg = i->paramArg();
                if (!i->paramKind()) {
                    if (arg < sizeof(Assembler::argRegs)/sizeof(Assembler::argRegs[0])) {
                        sprintf(s, "%s = %s %d %s", formatRef(i), lirNames[op],
                            arg, gpn(Assembler::argRegs[arg]));
                    } else {
                        sprintf(s, "%s = %s %d", formatRef(i), lirNames[op], arg);
                    }
                } else {
                    sprintf(s, "%s = %s %d %s", formatRef(i), lirNames[op],
                        arg, gpn(Assembler::savedRegs[arg]));
                }
                break;
            }

            case LIR_label:
                sprintf(s, "%s:", formatRef(i));
                break;

            case LIR_jt:
            case LIR_jf:
                sprintf(s, "%s %s -> %s", lirNames[op], formatRef(i->oprnd1()),
                    i->oprnd2() ? formatRef(i->oprnd2()) : "unpatched");
                break;

            case LIR_j:
                sprintf(s, "%s -> %s", lirNames[op],
                    i->oprnd2() ? formatRef(i->oprnd2()) : "unpatched");
                break;

            case LIR_live:
            case LIR_ret:
            case LIR_fret:
                sprintf(s, "%s %s", lirNames[op], formatRef(i->oprnd1()));
                break;

            case LIR_callh:
            case LIR_neg:
            case LIR_fneg:
            case LIR_i2f:
            case LIR_u2f:
            case LIR_qlo:
            case LIR_qhi:
            case LIR_ov:
            case LIR_not:
            case LIR_mod:
                sprintf(s, "%s = %s %s", formatRef(i), lirNames[op], formatRef(i->oprnd1()));
                break;

            case LIR_x:
            case LIR_xt:
            case LIR_xf:
            case LIR_xbarrier:
            case LIR_xtbl:
                formatGuard(i, s);
                break;

            case LIR_add:
            case LIR_iaddp:
            case LIR_sub:
             case LIR_mul:
            case LIR_div:
            case LIR_fadd:
            case LIR_fsub:
             case LIR_fmul:
            case LIR_fdiv:
            case LIR_and:
            case LIR_or:
            case LIR_xor:
            case LIR_lsh:
            case LIR_rsh:
            case LIR_ush:
            case LIR_eq:
            case LIR_lt:
            case LIR_le:
            case LIR_gt:
            case LIR_ge:
            case LIR_ult:
            case LIR_ule:
            case LIR_ugt:
            case LIR_uge:
            case LIR_feq:
            case LIR_flt:
            case LIR_fle:
            case LIR_fgt:
            case LIR_fge:
            case LIR_qiadd:
            case LIR_qiand:
            case LIR_qilsh:
            case LIR_qior:
                sprintf(s, "%s = %s %s, %s", formatRef(i), lirNames[op],
                    formatRef(i->oprnd1()),
                    formatRef(i->oprnd2()));
                break;

            case LIR_qjoin:
                sprintf(s, "%s (%s), %s", lirNames[op],
                    formatIns(i->oprnd1()),
                     formatRef(i->oprnd2()));
                 break;

            case LIR_qcmov:
            case LIR_cmov:
                sprintf(s, "%s = %s %s ? %s : %s", formatRef(i), lirNames[op],
                    formatRef(i->oprnd1()),
                    formatRef(i->oprnd2()),
                    formatRef(i->oprnd3()));
                break;

            case LIR_ld:
            case LIR_ldc:
            case LIR_ldq:
            case LIR_ldqc:
            case LIR_ldcb:
            case LIR_ldcs:
                sprintf(s, "%s = %s %s[%d]", formatRef(i), lirNames[op],
                    formatRef(i->oprnd1()),
                    i->disp());
                break;

            case LIR_sti:
            case LIR_stqi:
                sprintf(s, "%s %s[%d] = %s", lirNames[op],
                    formatRef(i->oprnd2()),
                    i->disp(),
                    formatRef(i->oprnd1()));
                break;

            default:
                sprintf(s, "?");
                break;
        }
        return labels->dup(sbuf);
    }


#endif
    CseFilter::CseFilter(LirWriter *out, GC *gc)
        : LirWriter(out), exprs(gc) {}

    LIns* CseFilter::insImm(int32_t imm)
    {
        uint32_t k;
        LInsp found = exprs.find32(imm, k);
        if (found)
            return found;
        return exprs.add(out->insImm(imm), k);
    }

    LIns* CseFilter::insImmq(uint64_t q)
    {
        uint32_t k;
        LInsp found = exprs.find64(q, k);
        if (found)
            return found;
        return exprs.add(out->insImmq(q), k);
    }

    LIns* CseFilter::ins0(LOpcode v)
    {
        if (v == LIR_label)
            exprs.clear();
        return out->ins0(v);
    }

    LIns* CseFilter::ins1(LOpcode v, LInsp a)
    {
        if (isCseOpcode(v)) {
            NanoAssert(operandCount[v]==1);
            uint32_t k;
            LInsp found = exprs.find1(v, a, k);
            if (found)
                return found;
            return exprs.add(out->ins1(v,a), k);
        }
        return out->ins1(v,a);
    }

    LIns* CseFilter::ins2(LOpcode v, LInsp a, LInsp b)
    {
        if (isCseOpcode(v)) {
            NanoAssert(operandCount[v]==2);
            uint32_t k;
            LInsp found = exprs.find2(v, a, b, k);
            if (found)
                return found;
            return exprs.add(out->ins2(v,a,b), k);
        }
        return out->ins2(v,a,b);
    }

    LIns* CseFilter::ins3(LOpcode v, LInsp a, LInsp b, LInsp c)
    {
        NanoAssert(isCseOpcode(v));
        NanoAssert(operandCount[v]==3);
        uint32_t k;
        LInsp found = exprs.find3(v, a, b, c, k);
        if (found)
            return found;
        return exprs.add(out->ins3(v,a,b,c), k);
    }

    LIns* CseFilter::insLoad(LOpcode v, LInsp base, int32_t disp)
    {
        if (isCseOpcode(v)) {
            NanoAssert(operandCount[v]==1);
            uint32_t k;
            LInsp found = exprs.findLoad(v, base, disp, k);
            if (found)
                return found;
            return exprs.add(out->insLoad(v,base,disp), k);
        }
        return out->insLoad(v,base,disp);
    }

    LInsp CseFilter::insGuard(LOpcode v, LInsp c, LInsp x)
    {
        // LIR_xt and LIR_xf guards are CSEable.  Note that we compare the
        // opcode and condition when determining if two guards are equivalent
        // -- in find1(), hash1(), equals() and hashcode() -- but we do *not*
        // compare the GuardRecord.  This works because:
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
        if (isCseOpcode(v)) {
            // conditional guard
            NanoAssert(operandCount[v]==1);
            uint32_t k;
            LInsp found = exprs.find1(v, c, k);
            if (found)
                return 0;
            return exprs.add(out->insGuard(v,c,x), k);
        }
        return out->insGuard(v, c, x);
    }

    LInsp CseFilter::insCall(const CallInfo *ci, LInsp args[])
    {
        if (ci->_cse) {
            uint32_t k;
            uint32_t argc = ci->count_args();
            LInsp found = exprs.findcall(ci, argc, args, k);
            if (found)
                return found;
            return exprs.add(out->insCall(ci, args), k);
        }
        return out->insCall(ci, args);
    }

    CseReader::CseReader(LirFilter *in, LInsHashSet *exprs)
        : LirFilter(in), exprs(exprs)
    {}

    LInsp CseReader::read()
    {
        LInsp i = in->read();
        if (i->isCse())
            exprs->replace(i);
        return i;
    }

    LIns* FASTCALL callArgN(LIns* i, uint32_t n)
    {
        return i->arg(i->argc()-n-1);
    }

    void compile(Assembler* assm, Fragment* triggerFrag)
    {
        Fragmento *frago = triggerFrag->lirbuf->_frago;
        AvmCore *core = frago->core();
        GC *gc = core->gc;

        verbose_only(
        LogControl *logc = assm->_logc;
        bool anyVerb = (logc->lcbits & 0xFFFF) > 0;
        bool asmVerb = (logc->lcbits & 0xFFFF & LC_Assembly) > 0;
        bool liveVerb = (logc->lcbits & 0xFFFF & LC_Liveness) > 0;
        )

        /* BEGIN decorative preamble */
        verbose_only(
        if (anyVerb) {
            logc->printf("========================================"
                         "========================================\n");
            logc->printf("=== BEGIN LIR::compile(%p, %p)\n",
                         (void*)assm, (void*)triggerFrag);
            logc->printf("===\n");
        })
        /* END decorative preamble */

        verbose_only( if (liveVerb) {
            logc->printf("\n");
            logc->printf("=== Results of liveness analysis:\n");
            logc->printf("===\n");
            live(gc, triggerFrag, logc);
        })

        /* Set up the generic text output cache for the assembler */
        verbose_only( StringList asmOutput(gc); )
        verbose_only( assm->_outputCache = &asmOutput; )

        bool treeCompile = core->config.tree_opt && (triggerFrag->kind == BranchTrace);
        RegAllocMap regMap(gc);
        NInsList loopJumps(gc);
#ifdef MEMORY_INFO
//        loopJumps.set_meminfo_name("LIR loopjumps");
#endif
        assm->beginAssembly(triggerFrag, &regMap);
        if (assm->error())
            return;

        //logc->printf("recompile trigger %X kind %d\n", (int)triggerFrag, triggerFrag->kind);

        verbose_only( if (anyVerb) {
            logc->printf("=== Translating LIR fragments into assembly:\n");
        })

        Fragment* root = triggerFrag;
        if (treeCompile)
        {
            // recompile the entire tree
            verbose_only( if (anyVerb) {
               logc->printf("=== -- Compile the entire tree: begin\n");
            })
            root = triggerFrag->root;
            root->fragEntry = 0;
            root->loopEntry = 0;
            root->releaseCode(frago);

            // do the tree branches
            verbose_only( if (anyVerb) {
               logc->printf("=== -- Do the tree branches\n");
            })
            Fragment* frag = root->treeBranches;
            while (frag)
            {
                // compile til no more frags
                if (frag->lastIns)
                {
                    verbose_only( if (anyVerb) {
                        logc->printf("=== -- Compiling branch %s ip %s\n",
                                     frago->labels->format(frag),
                                     frago->labels->format(frag->ip));
                    })
                    assm->assemble(frag, loopJumps);
                    verbose_only(if (asmVerb)
                        assm->outputf("## compiling branch %s ip %s",
                                      frago->labels->format(frag),
                                      frago->labels->format(frag->ip)); )

                    NanoAssert(frag->kind == BranchTrace);
                    RegAlloc* regs = NJ_NEW(gc, RegAlloc)();
                    assm->copyRegisters(regs);
                    assm->releaseRegisters();
                    SideExit* exit = frag->spawnedFrom;
                    regMap.put(exit, regs);
                }
                frag = frag->treeBranches;
            }
            verbose_only( if (anyVerb) {
               logc->printf("=== -- Compile the entire tree: end\n");
            })
        }

        // now the the main trunk
        verbose_only( if (anyVerb) {
            logc->printf("=== -- Compile trunk %s: begin\n",
                         frago->labels->format(root));
        })
        assm->assemble(root, loopJumps);
        verbose_only( if (anyVerb) {
            logc->printf("=== -- Compile trunk %s: end\n",
                         frago->labels->format(root));
        })

        verbose_only(
            if (asmVerb)
                assm->outputf("## compiling trunk %s",
                              frago->labels->format(root));
        )
        NanoAssert(!frago->core()->config.tree_opt
                   || root == root->anchor || root->kind == MergeTrace);
        assm->endAssembly(root, loopJumps);

        // reverse output so that assembly is displayed low-to-high
        // Up to this point, assm->_outputCache has been non-NULL, and so
        // has been accumulating output.  Now we set it to NULL, traverse
        // the entire list of stored strings, and hand them a second time
        // to assm->output.  Since _outputCache is now NULL, outputf just
        // hands these strings directly onwards to logc->printf.
        verbose_only( if (anyVerb) {
               logc->printf("\n");
            logc->printf("=== Aggregated assembly output: BEGIN\n");
               logc->printf("===\n");
            assm->_outputCache = 0;
            for (int i = asmOutput.size() - 1; i >= 0; --i) {
                char* str = asmOutput.get(i);
                assm->outputf("  %s", str);
                gc->Free(str);
            }
               logc->printf("===\n");
            logc->printf("=== Aggregated assembly output: END\n");
        });

        if (assm->error()) {
            root->fragEntry = 0;
            root->loopEntry = 0;
        }

        /* BEGIN decorative postamble */
        verbose_only( if (anyVerb) {
            logc->printf("\n");
            logc->printf("===\n");
            logc->printf("=== END LIR::compile(%p, %p)\n",
                         (void*)assm, (void*)triggerFrag);
            logc->printf("========================================"
                         "========================================\n");
            logc->printf("\n");
        });
        /* END decorative postamble */
    }

    LInsp LoadFilter::insLoad(LOpcode v, LInsp base, int32_t disp)
    {
        if (base != sp && base != rp && (v == LIR_ld || v == LIR_ldq)) {
            uint32_t k;
            LInsp found = exprs.findLoad(v, base, disp, k);
            if (found)
                return found;
            return exprs.add(out->insLoad(v,base,disp), k);
        }
        return out->insLoad(v, base, disp);
    }

    void LoadFilter::clear(LInsp p)
    {
        if (p != sp && p != rp)
            exprs.clear();
    }

    LInsp LoadFilter::insStorei(LInsp v, LInsp b, int32_t d)
    {
        clear(b);
        return out->insStorei(v, b, d);
    }

    LInsp LoadFilter::insCall(const CallInfo *ci, LInsp args[])
    {
        if (!ci->_cse)
            exprs.clear();
        return out->insCall(ci, args);
    }

    LInsp LoadFilter::ins0(LOpcode op)
    {
        if (op == LIR_label)
            exprs.clear();
        return out->ins0(op);
    }

    #endif /* FEATURE_NANOJIT */

#if defined(NJ_VERBOSE)
    LabelMap::LabelMap(AvmCore *core)
        : names(core->gc), addrs(core->config.verbose_addrs), end(buf), core(core)
    {}

    LabelMap::~LabelMap()
    {
        clear();
    }

    void LabelMap::clear()
    {
        Entry *e;
        while ((e = names.removeLast()) != NULL) {
            core->freeString(e->name);
            NJ_DELETE(e);
        }
    }

    void LabelMap::add(const void *p, size_t size, size_t align, const char *name)
    {
        if (!this || names.containsKey(p))
            return;
        add(p, size, align, core->newString(name));
    }

    void LabelMap::add(const void *p, size_t size, size_t align, Stringp name)
    {
        if (!this || names.containsKey(p))
            return;
        Entry *e = NJ_NEW(core->gc, Entry)(name, size<<align, align);
        names.put(p, e);
    }

    const char *LabelMap::format(const void *p)
    {
        char b[200];
        int i = names.findNear(p);
        if (i >= 0) {
            const void *start = names.keyAt(i);
            Entry *e = names.at(i);
            const void *end = (const char*)start + e->size;
            avmplus::StringNullTerminatedUTF8 cname(core->gc, e->name);
            const char *name = cname.c_str();
            if (p == start) {
                if (addrs)
                    sprintf(b,"%p %s",p,name);
                else
                    strcpy(b, name);
                return dup(b);
            }
            else if (p > start && p < end) {
                int32_t d = int32_t(intptr_t(p)-intptr_t(start)) >> e->align;
                if (addrs)
                    sprintf(b, "%p %s+%d", p, name, d);
                else
                    sprintf(b,"%s+%d", name, d);
                return dup(b);
            }
            else {
                sprintf(b, "%p", p);
                return dup(b);
            }
        }
        sprintf(b, "%p", p);
        return dup(b);
    }

    const char *LabelMap::dup(const char *b)
    {
        size_t need = strlen(b)+1;
        char *s = end;
        end += need;
        if (end > buf+sizeof(buf)) {
            s = buf;
            end = s+need;
        }
        strcpy(s, b);
        return s;
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
    }

#endif // NJ_VERBOSE
}

