/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "Assembler-arm.h"
#include "MacroAssembler-arm.h"
#include "gc/Marking.h"
#include "jsutil.h"
#include "assembler/jit/ExecutableAllocator.h"
#include "jscompartment.h"
#include "ion/IonCompartment.h"

using namespace js;
using namespace js::ion;

// Encode a standard register when it is being used as src1, the dest, and
// an extra register. These should never be called with an InvalidReg.
uint32_t
js::ion::RT(Register r)
{
    JS_ASSERT((r.code() & ~0xf) == 0);
    return r.code() << 12;
}

uint32_t
js::ion::RN(Register r)
{
    JS_ASSERT((r.code() & ~0xf) == 0);
    return r.code() << 16;
}

uint32_t
js::ion::RD(Register r)
{
    JS_ASSERT((r.code() & ~0xf) == 0);
    return r.code() << 12;
}

uint32_t
js::ion::RM(Register r)
{
    JS_ASSERT((r.code() & ~0xf) == 0);
    return r.code() << 8;
}

// Encode a standard register when it is being used as src1, the dest, and
// an extra register.  For these, an InvalidReg is used to indicate a optional
// register that has been omitted.
uint32_t
js::ion::maybeRT(Register r)
{
    if (r == InvalidReg)
        return 0;

    JS_ASSERT((r.code() & ~0xf) == 0);
    return r.code() << 12;
}

uint32_t
js::ion::maybeRN(Register r)
{
    if (r == InvalidReg)
        return 0;

    JS_ASSERT((r.code() & ~0xf) == 0);
    return r.code() << 16;
}

uint32_t
js::ion::maybeRD(Register r)
{
    if (r == InvalidReg)
        return 0;

    JS_ASSERT((r.code() & ~0xf) == 0);
    return r.code() << 12;
}

Register
js::ion::toRD(Instruction &i)
{
    return Register::FromCode((i.encode()>>12) & 0xf);
}
Register
js::ion::toR(Instruction &i)
{
    return Register::FromCode(i.encode() & 0xf);
}

Register
js::ion::toRM(Instruction &i)
{
    return Register::FromCode((i.encode()>>8) & 0xf);
}

Register
js::ion::toRN(Instruction &i)
{
    return Register::FromCode((i.encode()>>16) & 0xf);
}

uint32_t
js::ion::VD(VFPRegister vr)
{
    if (vr.isMissing())
        return 0;

    //bits 15,14,13,12, 22
    VFPRegister::VFPRegIndexSplit s = vr.encode();
    return s.bit << 22 | s.block << 12;
}
uint32_t
js::ion::VN(VFPRegister vr)
{
    if (vr.isMissing())
        return 0;

    // bits 19,18,17,16, 7
    VFPRegister::VFPRegIndexSplit s = vr.encode();
    return s.bit << 7 | s.block << 16;
}
uint32_t
js::ion::VM(VFPRegister vr)
{
    if (vr.isMissing())
        return 0;

    // bits 5, 3,2,1,0
    VFPRegister::VFPRegIndexSplit s = vr.encode();
    return s.bit << 5 | s.block;
}

VFPRegister::VFPRegIndexSplit
ion::VFPRegister::encode()
{
    JS_ASSERT(!_isInvalid);

    switch (kind) {
      case Double:
        return VFPRegIndexSplit(_code &0xf , _code >> 4);
      case Single:
        return VFPRegIndexSplit(_code >> 1, _code & 1);
      default:
        // vfp register treated as an integer, NOT a gpr
        return VFPRegIndexSplit(_code >> 1, _code & 1);
    }
}

VFPRegister js::ion::NoVFPRegister(true);

bool
InstDTR::isTHIS(const Instruction &i)
{
    return (i.encode() & IsDTRMask) == (uint32_t)IsDTR;
}

InstDTR *
InstDTR::asTHIS(Instruction &i)
{
    if (isTHIS(i))
        return (InstDTR*)&i;
    return NULL;
}

bool
InstLDR::isTHIS(const Instruction &i)
{
    return (i.encode() & IsDTRMask) == (uint32_t)IsDTR;
}

InstLDR *
InstLDR::asTHIS(Instruction &i)
{
    if (isTHIS(i))
        return (InstLDR*)&i;
    return NULL;
}

bool
InstBranchReg::isTHIS(const Instruction &i)
{
    return InstBXReg::isTHIS(i) || InstBLXReg::isTHIS(i);
}

InstBranchReg *
InstBranchReg::asTHIS(const Instruction &i)
{
    if (isTHIS(i))
        return (InstBranchReg*)&i;
    return NULL;
}
void
InstBranchReg::extractDest(Register *dest)
{
    *dest = toR(*this);
}
bool
InstBranchReg::checkDest(Register dest)
{
    return dest == toR(*this);
}

bool
InstBranchImm::isTHIS(const Instruction &i)
{
    return InstBImm::isTHIS(i) || InstBLImm::isTHIS(i);
}

InstBranchImm *
InstBranchImm::asTHIS(const Instruction &i)
{
    if (isTHIS(i))
        return (InstBranchImm*)&i;
    return NULL;
}

void
InstBranchImm::extractImm(BOffImm *dest)
{
    *dest = BOffImm(*this);
}

bool
InstBXReg::isTHIS(const Instruction &i)
{
    return (i.encode() & IsBRegMask) == IsBX;
}

InstBXReg *
InstBXReg::asTHIS(const Instruction &i)
{
    if (isTHIS(i))
        return (InstBXReg*)&i;
    return NULL;
}

bool
InstBLXReg::isTHIS(const Instruction &i)
{
    return (i.encode() & IsBRegMask) == IsBLX;

}
InstBLXReg *
InstBLXReg::asTHIS(const Instruction &i)
{
    if (isTHIS(i))
        return (InstBLXReg*)&i;
    return NULL;
}

bool
InstBImm::isTHIS(const Instruction &i)
{
    return (i.encode () & IsBImmMask) == IsB;
}
InstBImm *
InstBImm::asTHIS(const Instruction &i)
{
    if (isTHIS(i))
        return (InstBImm*)&i;
    return NULL;
}

bool
InstBLImm::isTHIS(const Instruction &i)
{
    return (i.encode () & IsBImmMask) == IsBL;

}
InstBLImm *
InstBLImm::asTHIS(Instruction &i)
{
    if (isTHIS(i))
        return (InstBLImm*)&i;
    return NULL;
}

bool
InstMovWT::isTHIS(Instruction &i)
{
    return  InstMovW::isTHIS(i) || InstMovT::isTHIS(i);
}
InstMovWT *
InstMovWT::asTHIS(Instruction &i)
{
    if (isTHIS(i))
        return (InstMovWT*)&i;
    return NULL;
}

void
InstMovWT::extractImm(Imm16 *imm)
{
    *imm = Imm16(*this);
}
bool
InstMovWT::checkImm(Imm16 imm)
{
    return (imm.decode() == Imm16(*this).decode());
}

void
InstMovWT::extractDest(Register *dest)
{
    *dest = toRD(*this);
}
bool
InstMovWT::checkDest(Register dest)
{
    return (dest == toRD(*this));
}

bool
InstMovW::isTHIS(const Instruction &i)
{
    return (i.encode() & IsWTMask) == IsW;
}

InstMovW *
InstMovW::asTHIS(const Instruction &i)
{
    if (isTHIS(i))
        return (InstMovW*) (&i);
    return NULL;
}
InstMovT *
InstMovT::asTHIS(const Instruction &i)
{
    if (isTHIS(i))
        return (InstMovT*) (&i);
    return NULL;
}

bool
InstMovT::isTHIS(const Instruction &i)
{
    return (i.encode() & IsWTMask) == IsT;
}

InstALU *
InstALU::asTHIS(const Instruction &i)
{
    if (isTHIS(i))
        return (InstALU*) (&i);
    return NULL;
}
bool
InstALU::isTHIS(const Instruction &i)
{
    return (i.encode() & ALUMask) == 0;
}
void
InstALU::extractOp(ALUOp *ret)
{
    *ret = ALUOp(encode() & (0xf << 21));
}
bool
InstALU::checkOp(ALUOp op)
{
    ALUOp mine;
    extractOp(&mine);
    return mine == op;
}
void
InstALU::extractDest(Register *ret)
{
    *ret = toRD(*this);
}
bool
InstALU::checkDest(Register rd)
{
    return rd == toRD(*this);
}
void
InstALU::extractOp1(Register *ret)
{
    *ret = toRN(*this);
}
bool
InstALU::checkOp1(Register rn)
{
    return rn == toRN(*this);
}

InstCMP *
InstCMP::asTHIS(const Instruction &i)
{
    if (isTHIS(i))
        return (InstCMP*) (&i);
    return NULL;
}

bool
InstCMP::isTHIS(const Instruction &i)
{
    return InstALU::isTHIS(i) && InstALU::asTHIS(i)->checkDest(r0);
}

Imm16::Imm16(Instruction &inst)
  : lower(inst.encode() & 0xfff),
    upper(inst.encode() >> 16),
    invalid(0xfff)
{ }

Imm16::Imm16(uint32_t imm)
  : lower(imm & 0xfff), pad(0),
    upper((imm>>12) & 0xf),
    invalid(0)
{
    JS_ASSERT(decode() == imm);
}

Imm16::Imm16()
  : invalid(0xfff)
{ }

void
ion::PatchJump(CodeLocationJump &jump_, CodeLocationLabel label)
{
    // We need to determine if this jump can fit into the standard 24+2 bit address
    // or if we need a larger branch (or just need to use our pool entry)
    Instruction *jump = (Instruction*)jump_.raw();
    Assembler::Condition c;
    jump->extractCond(&c);
    JS_ASSERT(jump->is<InstBranchImm>() || jump->is<InstLDR>());

    int jumpOffset = label.raw() - jump_.raw();
    if (BOffImm::isInRange(jumpOffset)) {
        // This instruction started off as a branch, and will remain one
        Assembler::retargetNearBranch(jump, jumpOffset, c);
    } else {
        // This instruction started off as a branch, but now needs to be demoted to an ldr.
        uint8_t **slot = reinterpret_cast<uint8_t**>(jump_.jumpTableEntry());
        Assembler::retargetFarBranch(jump, slot, label.raw(), c);
    }
}

void
Assembler::finish()
{
    flush();
    JS_ASSERT(!isFinished);
    isFinished = true;

    for (size_t i = 0; i < jumps_.length(); i++)
        jumps_[i].fixOffset(m_buffer);

    for (unsigned int i = 0; i < tmpDataRelocations_.length(); i++) {
        int offset = tmpDataRelocations_[i].getOffset();
        int real_offset = offset + m_buffer.poolSizeBefore(offset);
        dataRelocations_.writeUnsigned(real_offset);
    }

    for (unsigned int i = 0; i < tmpJumpRelocations_.length(); i++) {
        int offset = tmpJumpRelocations_[i].getOffset();
        int real_offset = offset + m_buffer.poolSizeBefore(offset);
        jumpRelocations_.writeUnsigned(real_offset);
    }
}

void
Assembler::executableCopy(uint8_t *buffer)
{
    JS_ASSERT(isFinished);
    m_buffer.executableCopy(buffer);
    AutoFlushCache::updateTop((uintptr_t)buffer, m_buffer.size());
}

void
Assembler::resetCounter()
{
    m_buffer.resetCounter();
}

uint32_t
Assembler::actualOffset(uint32_t off_) const
{
    return off_ + m_buffer.poolSizeBefore(off_);
}

uint32_t
Assembler::actualIndex(uint32_t idx_) const
{
    ARMBuffer::PoolEntry pe(idx_);
    return m_buffer.poolEntryOffset(pe);
}

uint8_t *
Assembler::PatchableJumpAddress(IonCode *code, uint32_t pe_)
{
    return code->raw() + pe_;
}

BufferOffset
Assembler::actualOffset(BufferOffset off_) const
{
    return BufferOffset(off_.getOffset() + m_buffer.poolSizeBefore(off_.getOffset()));
}

class RelocationIterator
{
    CompactBufferReader reader_;
    // offset in bytes
    uint32_t offset_;

  public:
    RelocationIterator(CompactBufferReader &reader)
      : reader_(reader)
    { }

    bool read() {
        if (!reader_.more())
            return false;
        offset_ = reader_.readUnsigned();
        return true;
    }

    uint32_t offset() const {
        return offset_;
    }
};

template<class Iter>
const uint32_t *
Assembler::getCF32Target(Iter *iter)
{
    Instruction *inst1 = iter->cur();
    Instruction *inst2 = iter->next();
    Instruction *inst3 = iter->next();
    Instruction *inst4 = iter->next();

    if (inst1->is<InstBranchImm>()) {
        // see if we have a simple case, b #offset
        BOffImm imm;
        InstBranchImm *jumpB = inst1->as<InstBranchImm>();
        jumpB->extractImm(&imm);
        return imm.getDest(inst1)->raw();
    }

    if (inst1->is<InstMovW>() && inst2->is<InstMovT>() &&
        (inst3->is<InstBranchReg>() || inst4->is<InstBranchReg>()))
    {
        // see if we have the complex case,
        // movw r_temp, #imm1
        // movt r_temp, #imm2
        // bx r_temp
        // OR
        // movw r_temp, #imm1
        // movt r_temp, #imm2
        // str pc, [sp]
        // bx r_temp

        Imm16 targ_bot;
        Imm16 targ_top;
        Register temp;

        // Extract both the temp register and the bottom immediate.
        InstMovW *bottom = inst1->as<InstMovW>();
        bottom->extractImm(&targ_bot);
        bottom->extractDest(&temp);

        // Extract the top part of the immediate.
        InstMovT *top = inst2->as<InstMovT>();
        top->extractImm(&targ_top);

        // Make sure they are being loaded into the same register.
        JS_ASSERT(top->checkDest(temp));

        // Make sure we're branching to the same register.
        InstBranchReg *realBranch = inst3->is<InstBranchReg>() ? inst3->as<InstBranchReg>()
                                                               : inst4->as<InstBranchReg>();
        JS_ASSERT(realBranch->checkDest(temp));

        uint32_t *dest = (uint32_t*) (targ_bot.decode() | (targ_top.decode() << 16));
        return dest;
    }
    
    if (inst1->is<InstLDR>()) {
        JS_NOT_REACHED("ldr-based relocs NYI");
    }

    JS_NOT_REACHED("unsupported branch relocation");
    return NULL;
}

uintptr_t
Assembler::getPointer(uint8_t *instPtr)
{
    InstructionIterator iter((Instruction*)instPtr);
    uintptr_t ret = (uintptr_t)getPtr32Target(&iter, NULL, NULL);
    return ret;
}

template<class Iter>
const uint32_t *
Assembler::getPtr32Target(Iter *start, Register *dest, RelocStyle *style)
{
    Instruction *load1 = start->cur();
    Instruction *load2 = start->next();

    if (load1->is<InstMovW>() && load2->is<InstMovT>()) {
        // see if we have the complex case,
        // movw r_temp, #imm1
        // movt r_temp, #imm2

        Imm16 targ_bot;
        Imm16 targ_top;
        Register temp;

        // Extract both the temp register and the bottom immediate.
        InstMovW *bottom = load1->as<InstMovW>();
        bottom->extractImm(&targ_bot);
        bottom->extractDest(&temp);

        // Extract the top part of the immediate.
        InstMovT *top = load2->as<InstMovT>();
        top->extractImm(&targ_top);

        // Make sure they are being loaded intothe same register.
        JS_ASSERT(top->checkDest(temp));

        if (dest)
            *dest = temp;
        if (style)
            *style = L_MOVWT;

        uint32_t *value = (uint32_t*) (targ_bot.decode() | (targ_top.decode() << 16));
        return value;
    }

    JS_NOT_REACHED("unsupported relocation");
    return NULL;
}

static IonCode *
CodeFromJump(InstructionIterator *jump)
{
    uint8_t *target = (uint8_t *)Assembler::getCF32Target(jump);
    return IonCode::FromExecutable(target);
}

void
Assembler::TraceJumpRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader)
{
    RelocationIterator iter(reader);
    while (iter.read()) {
        InstructionIterator institer((Instruction *) (code->raw() + iter.offset()));
        IonCode *child = CodeFromJump(&institer);
        MarkIonCodeUnbarriered(trc, &child, "rel32");
    }
}

static void
TraceDataRelocations(JSTracer *trc, uint8_t *buffer, CompactBufferReader &reader)
{
    while (reader.more()) {
        size_t offset = reader.readUnsigned();
        InstructionIterator iter((Instruction*)(buffer+offset));
        void *ptr = const_cast<uint32_t *>(js::ion::Assembler::getPtr32Target(&iter));
        // No barrier needed since these are constants.
        gc::MarkGCThingUnbarriered(trc, reinterpret_cast<void **>(&ptr), "ion-masm-ptr");
    }

}
static void
TraceDataRelocations(JSTracer *trc, ARMBuffer *buffer,
                     js::Vector<BufferOffset, 0, SystemAllocPolicy> *locs)
{
    for (unsigned int idx = 0; idx < locs->length(); idx++) {
        BufferOffset bo = (*locs)[idx];
        ARMBuffer::AssemblerBufferInstIterator iter(bo, buffer);
        void *ptr = const_cast<uint32_t *>(ion::Assembler::getPtr32Target(&iter));

        // No barrier needed since these are constants.
        gc::MarkGCThingUnbarriered(trc, reinterpret_cast<void **>(&ptr), "ion-masm-ptr");
    }

}
void
Assembler::TraceDataRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader)
{
    ::TraceDataRelocations(trc, code->raw(), reader);
}

void
Assembler::copyJumpRelocationTable(uint8_t *dest)
{
    if (jumpRelocations_.length())
        memcpy(dest, jumpRelocations_.buffer(), jumpRelocations_.length());
}

void
Assembler::copyDataRelocationTable(uint8_t *dest)
{
    if (dataRelocations_.length())
        memcpy(dest, dataRelocations_.buffer(), dataRelocations_.length());
}

void
Assembler::trace(JSTracer *trc)
{
    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch &rp = jumps_[i];
        if (rp.kind == Relocation::IONCODE) {
            IonCode *code = IonCode::FromExecutable((uint8_t*)rp.target);
            MarkIonCodeUnbarriered(trc, &code, "masmrel32");
            JS_ASSERT(code == IonCode::FromExecutable((uint8_t*)rp.target));
        }
    }

    if (tmpDataRelocations_.length())
        ::TraceDataRelocations(trc, &m_buffer, &tmpDataRelocations_);
}

void
Assembler::processDeferredData(IonCode *code, uint8_t *data)
{
    // Deferred Data is something like Pools for X86.
    // Since ARM has competent pools, this isn't actually used.
    // Except of course, for SwitchTables.  Those are really shoehorned
    // in and don't take up any space in the instruction stream, so dataSize()
    // is still 0.
    // NOTE: this means arm will in fact break if the for loop is removed.
    JS_ASSERT(dataSize() == 0);

    for (size_t i = 0; i < data_.length(); i++) {
        DeferredData *deferred = data_[i];
        //Bind(code, deferred->label(), data + deferred->offset());
        deferred->copy(code, data + deferred->offset());
    }

}

// As far as I can tell, CodeLabels were supposed to be used in switch tables
// and they aren't used there, nor anywhere else.
void
Assembler::processCodeLabels(IonCode *code)
{
    for (size_t i = 0; i < codeLabels_.length(); i++) {
        //Bind(code, label->dest(), code->raw() + label->src()->offset());
        JS_NOT_REACHED("dead code?");
    }
}

Assembler::Condition
Assembler::InvertCondition(Condition cond)
{
    const uint32_t ConditionInversionBit = 0x10000000;
    return Condition(ConditionInversionBit ^ cond);
}

Imm8::TwoImm8mData
Imm8::encodeTwoImms(uint32_t imm)
{
    // In the ideal case, we are looking for a number that (in binary) looks like:
    // 0b((00)*)n_1((00)*)n_2((00)*)
    //    left  n1   mid  n2
    // where both n_1 and n_2 fit into 8 bits.
    // since this is being done with rotates, we also need to handle the case
    // that one of these numbers is in fact split between the left and right
    // sides, in which case the constant will look like:
    // 0bn_1a((00)*)n_2((00)*)n_1b
    //   n1a  mid  n2   rgh    n1b
    // also remember, values are rotated by multiples of two, and left,
    // mid or right can have length zero
    uint32_t imm1, imm2;
    int left = (js_bitscan_clz32(imm)) & 0x1E;
    uint32_t no_n1 = imm & ~(0xff << (24 - left));

    // not technically needed: this case only happens if we can encode
    // as a single imm8m.  There is a perfectly reasonable encoding in this
    // case, but we shouldn't encourage people to do things like this.
    if (no_n1 == 0)
        return TwoImm8mData();

    int mid = ((js_bitscan_clz32(no_n1)) & 0x1E);
    uint32_t no_n2 = no_n1 & ~((0xff << ((24 - mid) & 0x1f)) | 0xff >> ((8 + mid) & 0x1f));

    if (no_n2 == 0) {
        // we hit the easy case, no wraparound.
        // note: a single constant *may* look like this.
        int imm1shift = left + 8;
        int imm2shift = mid + 8;
        imm1 = (imm >> (32 - imm1shift)) & 0xff;
        if (imm2shift >= 32) {
            imm2shift = 0;
            // this assert does not always hold
            //assert((imm & 0xff) == no_n1);
            // in fact, this would lead to some incredibly subtle bugs.
            imm2 = no_n1;
        } else {
            imm2 = ((imm >> (32 - imm2shift)) | (imm << imm2shift)) & 0xff;
            JS_ASSERT( ((no_n1 >> (32 - imm2shift)) | (no_n1 << imm2shift)) ==
                       imm2);
        }
        JS_ASSERT((imm1shift & 0x1) == 0);
        JS_ASSERT((imm2shift & 0x1) == 0);
        return TwoImm8mData(datastore::Imm8mData(imm1, imm1shift >> 1),
                            datastore::Imm8mData(imm2, imm2shift >> 1));
    }

    // either it wraps, or it does not fit.
    // if we initially chopped off more than 8 bits, then it won't fit.
    if (left >= 8)
        return TwoImm8mData();

    int right = 32 - (js_bitscan_clz32(no_n2) & 30);
    // all remaining set bits *must* fit into the lower 8 bits
    // the right == 8 case should be handled by the previous case.
    if (right > 8)
        return TwoImm8mData();

    // make sure the initial bits that we removed for no_n1
    // fit into the 8-(32-right) leftmost bits
    if (((imm & (0xff << (24 - left))) << (8-right)) != 0) {
        // BUT we may have removed more bits than we needed to for no_n1
        // 0x04104001 e.g. we can encode 0x104 with a single op, then
        // 0x04000001 with a second, but we try to encode 0x0410000
        // and find that we need a second op for 0x4000, and 0x1 cannot
        // be included in the encoding of 0x04100000
        no_n1 = imm & ~((0xff >> (8-right)) | (0xff << (24 + right)));
        mid = (js_bitscan_clz32(no_n1)) & 30;
        no_n2 =
            no_n1  & ~((0xff << ((24 - mid)&31)) | 0xff >> ((8 + mid)&31));
        if (no_n2 != 0)
            return TwoImm8mData();
    }

    // now assemble all of this information into a two coherent constants
    // it is a rotate right from the lower 8 bits.
    int imm1shift = 8 - right;
    imm1 = 0xff & ((imm << imm1shift) | (imm >> (32 - imm1shift)));
    JS_ASSERT ((imm1shift&~0x1e) == 0);
    // left + 8 + mid is the position of the leftmost bit of n_2.
    // we needed to rotate 0x000000ab right by 8 in order to get
    // 0xab000000, then shift again by the leftmost bit in order to
    // get the constant that we care about.
    int imm2shift =  mid + 8;
    imm2 = ((imm >> (32 - imm2shift)) | (imm << imm2shift)) & 0xff;
    JS_ASSERT((imm1shift & 0x1) == 0);
    JS_ASSERT((imm2shift & 0x1) == 0);
    return TwoImm8mData(datastore::Imm8mData(imm1, imm1shift >> 1),
                        datastore::Imm8mData(imm2, imm2shift >> 1));
}

ALUOp
ion::ALUNeg(ALUOp op, Register dest, Imm32 *imm, Register *negDest)
{
    // find an alternate ALUOp to get the job done, and use a different imm.
    *negDest = dest;
    switch (op) {
      case op_mov:
        *imm = Imm32(~imm->value);
        return op_mvn;
      case op_mvn:
        *imm = Imm32(~imm->value);
        return op_mov;
      case op_and:
        *imm = Imm32(~imm->value);
        return op_bic;
      case op_bic:
        *imm = Imm32(~imm->value);
        return op_and;
      case op_add:
        *imm = Imm32(-imm->value);
        return op_sub;
      case op_sub:
        *imm = Imm32(-imm->value);
        return op_add;
      case op_cmp:
        *imm = Imm32(-imm->value);
        return op_cmn;
      case op_cmn:
        *imm = Imm32(-imm->value);
        return op_cmp;
      case op_tst:
        JS_ASSERT(dest == InvalidReg);
        *imm = Imm32(~imm->value);
        *negDest = ScratchRegister;
        return op_bic;
        // orr has orn on thumb2 only.
      default:
        return op_invalid;
    }
}

bool
ion::can_dbl(ALUOp op)
{
    // some instructions can't be processed as two separate instructions
    // such as and, and possibly add (when we're setting ccodes).
    // there is also some hilarity with *reading* condition codes.
    // for example, adc dest, src1, 0xfff; (add with carry) can be split up
    // into adc dest, src1, 0xf00; add dest, dest, 0xff, since "reading" the
    // condition code increments the result by one conditionally, that only needs
    // to be done on one of the two instructions.
    switch (op) {
      case op_bic:
      case op_add:
      case op_sub:
      case op_eor:
      case op_orr:
        return true;
      default:
        return false;
    }
}

bool
ion::condsAreSafe(ALUOp op) {
    // Even when we are setting condition codes, sometimes we can
    // get away with splitting an operation into two.
    // for example, if our immediate is 0x00ff00ff, and the operation is eors
    // we can split this in half, since x ^ 0x00ff0000 ^ 0x000000ff should
    // set all of its condition codes exactly the same as x ^ 0x00ff00ff.
    // However, if the operation were adds,
    // we cannot split this in half.  If the source on the add is
    // 0xfff00ff0, the result sholud be 0xef10ef, but do we set the overflow bit
    // or not?  Depending on which half is performed first (0x00ff0000
    // or 0x000000ff) the V bit will be set differently, and *not* updating
    // the V bit would be wrong.  Theoretically, the following should work
    // adds r0, r1, 0x00ff0000;
    // addsvs r0, r1, 0x000000ff;
    // addvc r0, r1, 0x000000ff;
    // but this is 3 instructions, and at that point, we might as well use
    // something else.
    switch(op) {
      case op_bic:
      case op_orr:
      case op_eor:
        return true;
      default:
        return false;
    }
}

ALUOp
ion::getDestVariant(ALUOp op)
{
    // all of the compare operations are dest-less variants of a standard
    // operation.  Given the dest-less variant, return the dest-ful variant.
    switch (op) {
      case op_cmp:
        return op_sub;
      case op_cmn:
        return op_add;
      case op_tst:
        return op_and;
      case op_teq:
        return op_eor;
      default:
        return op;
    }
}

O2RegImmShift
ion::O2Reg(Register r) {
    return O2RegImmShift(r, LSL, 0);
}

O2RegImmShift
ion::lsl(Register r, int amt)
{
    return O2RegImmShift(r, LSL, amt);
}

O2RegImmShift
ion::lsr(Register r, int amt)
{
    return O2RegImmShift(r, LSR, amt);
}

O2RegImmShift
ion::ror(Register r, int amt)
{
    return O2RegImmShift(r, ROR, amt);
}
O2RegImmShift
ion::rol(Register r, int amt)
{
    return O2RegImmShift(r, ROR, 32 - amt);
}

O2RegImmShift
ion::asr (Register r, int amt)
{
    return O2RegImmShift(r, ASR, amt);
}


O2RegRegShift
ion::lsl(Register r, Register amt)
{

    return O2RegRegShift(r, LSL, amt);
}

O2RegRegShift
ion::lsr(Register r, Register amt)
{
    return O2RegRegShift(r, LSR, amt);
}

O2RegRegShift
ion::ror(Register r, Register amt)
{
    return O2RegRegShift(r, ROR, amt);
}

O2RegRegShift
ion::asr (Register r, Register amt)
{
    return O2RegRegShift(r, ASR, amt);
}


js::ion::VFPImm::VFPImm(uint32_t top)
{
    data = -1;
    datastore::Imm8VFPImmData tmp;
    if (DoubleEncoder::lookup(top, &tmp))
        data = tmp.encode();
}

BOffImm::BOffImm(Instruction &inst)
  : data(inst.encode() & 0x00ffffff)
{
}

Instruction *
BOffImm::getDest(Instruction *src)
{
    // TODO: It is probably worthwhile to verify that src is actually a branch
    // NOTE: This does not explicitly shift the offset of the destination left by 2,
    // since it is indexing into an array of instruction sized objects.
    return &src[(((int32_t)data<<8)>>8) + 2];
}

js::ion::DoubleEncoder js::ion::DoubleEncoder::_this;

//VFPRegister implementation
VFPRegister
VFPRegister::doubleOverlay()
{
    JS_ASSERT(!_isInvalid);
    if (kind != Double)
        return VFPRegister(_code >> 1, Double);
    return *this;
}
VFPRegister
VFPRegister::singleOverlay()
{
    JS_ASSERT(!_isInvalid);
    if (kind == Double) {
        // There are no corresponding float registers for d16-d31
        ASSERT(_code < 16);
        return VFPRegister(_code << 1, Single);
    }

    return VFPRegister(_code, Single);
}

VFPRegister
VFPRegister::sintOverlay()
{
    JS_ASSERT(!_isInvalid);
    if (kind == Double) {
        // There are no corresponding float registers for d16-d31
        ASSERT(_code < 16);
        return VFPRegister(_code << 1, Int);
    }

    return VFPRegister(_code, Int);
}
VFPRegister
VFPRegister::uintOverlay()
{
    JS_ASSERT(!_isInvalid);
    if (kind == Double) {
        // There are no corresponding float registers for d16-d31
        ASSERT(_code < 16);
        return VFPRegister(_code << 1, UInt);
    }

    return VFPRegister(_code, UInt);
}

bool
VFPRegister::isInvalid()
{
    return _isInvalid;
}

bool
VFPRegister::isMissing()
{
    JS_ASSERT(!_isInvalid);
    return _isMissing;
}


bool
Assembler::oom() const
{
    return m_buffer.oom() || !enoughMemory_ || jumpRelocations_.oom();
}

bool
Assembler::addDeferredData(DeferredData *data, size_t bytes)
{
    data->setOffset(dataBytesNeeded_);
    dataBytesNeeded_ += bytes;
    if (dataBytesNeeded_ >= MAX_BUFFER_SIZE)
        return false;
    return data_.append(data);
}

bool
Assembler::addCodeLabel(CodeLabel *label)
{
    return codeLabels_.append(label);
}

// Size of the instruction stream, in bytes.  Including pools. This function expects
// all pools that need to be placed have been placed.  If they haven't then we
// need to go an flush the pools :(
size_t
Assembler::size() const
{
    return m_buffer.size();
}
// Size of the relocation table, in bytes.
size_t
Assembler::jumpRelocationTableBytes() const
{
    return jumpRelocations_.length();
}
size_t
Assembler::dataRelocationTableBytes() const
{
    return dataRelocations_.length();
}

// Size of the data table, in bytes.
size_t
Assembler::dataSize() const
{
    return dataBytesNeeded_;
}
size_t
Assembler::bytesNeeded() const
{
    return size() +
        dataSize() +
        jumpRelocationTableBytes() +
        dataRelocationTableBytes();
}
// write a blob of binary into the instruction stream
BufferOffset
Assembler::writeInst(uint32_t x, uint32_t *dest)
{
    if (dest == NULL)
        return m_buffer.putInt(x);

    writeInstStatic(x, dest);
    return BufferOffset();
}
void
Assembler::writeInstStatic(uint32_t x, uint32_t *dest)
{
    JS_ASSERT(dest != NULL);
    *dest = x;
}

BufferOffset
Assembler::align(int alignment)
{
    BufferOffset ret;
    while (!m_buffer.isAligned(alignment)) {
        BufferOffset tmp = as_nop();
        if (!ret.assigned())
            ret = tmp;
    }
    return ret;

}
BufferOffset
Assembler::as_nop()
{
    return writeInst(0xe320f000);
}
BufferOffset
Assembler::as_alu(Register dest, Register src1, Operand2 op2,
                ALUOp op, SetCond_ sc, Condition c)
{
    return writeInst((int)op | (int)sc | (int) c | op2.encode() |
                     ((dest == InvalidReg) ? 0 : RD(dest)) |
                     ((src1 == InvalidReg) ? 0 : RN(src1)));
}
BufferOffset
Assembler::as_mov(Register dest, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, InvalidReg, op2, op_mov, sc, c);
}
BufferOffset
Assembler::as_mvn(Register dest, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, InvalidReg, op2, op_mvn, sc, c);
}

// Logical operations.
BufferOffset
Assembler::as_and(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_and, sc, c);
}
BufferOffset
Assembler::as_bic(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_bic, sc, c);
}
BufferOffset
Assembler::as_eor(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_eor, sc, c);
}
BufferOffset
Assembler::as_orr(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_orr, sc, c);
}

// Mathematical operations.
BufferOffset
Assembler::as_adc(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_adc, sc, c);
}
BufferOffset
Assembler::as_add(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_add, sc, c);
}
BufferOffset
Assembler::as_sbc(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_sbc, sc, c);
}
BufferOffset
Assembler::as_sub(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_sub, sc, c);
}
BufferOffset
Assembler::as_rsb(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_rsb, sc, c);
}
BufferOffset
Assembler::as_rsc(Register dest, Register src1, Operand2 op2, SetCond_ sc, Condition c)
{
    return as_alu(dest, src1, op2, op_rsc, sc, c);
}

// Test operations.
BufferOffset
Assembler::as_cmn(Register src1, Operand2 op2, Condition c)
{
    return as_alu(InvalidReg, src1, op2, op_cmn, SetCond, c);
}
BufferOffset
Assembler::as_cmp(Register src1, Operand2 op2, Condition c)
{
    return as_alu(InvalidReg, src1, op2, op_cmp, SetCond, c);
}
BufferOffset
Assembler::as_teq(Register src1, Operand2 op2, Condition c)
{
    return as_alu(InvalidReg, src1, op2, op_teq, SetCond, c);
}
BufferOffset
Assembler::as_tst(Register src1, Operand2 op2, Condition c)
{
    return as_alu(InvalidReg, src1, op2, op_tst, SetCond, c);
}

// Not quite ALU worthy, but useful none the less:
// These also have the isue of these being formatted
// completly differently from the standard ALU operations.
BufferOffset
Assembler::as_movw(Register dest, Imm16 imm, Condition c, Instruction *pos)
{
    JS_ASSERT(hasMOVWT());
    return writeInst(0x03000000 | c | imm.encode() | RD(dest), (uint32_t*)pos);
}
BufferOffset
Assembler::as_movt(Register dest, Imm16 imm, Condition c, Instruction *pos)
{
    JS_ASSERT(hasMOVWT());
    return writeInst(0x03400000 | c | imm.encode() | RD(dest), (uint32_t*)pos);
}

const int mull_tag = 0x90;

BufferOffset
Assembler::as_genmul(Register dhi, Register dlo, Register rm, Register rn,
                     MULOp op, SetCond_ sc, Condition c)
{

    return writeInst(RN(dhi) | maybeRD(dlo) | RM(rm) | rn.code() | op | sc | c | mull_tag);
}
BufferOffset
Assembler::as_mul(Register dest, Register src1, Register src2, SetCond_ sc, Condition c)
{
    return as_genmul(dest, InvalidReg, src1, src2, opm_mul, sc, c);
}
BufferOffset
Assembler::as_mla(Register dest, Register acc, Register src1, Register src2,
                  SetCond_ sc, Condition c)
{
    return as_genmul(dest, acc, src1, src2, opm_mla, sc, c);
}
BufferOffset
Assembler::as_umaal(Register destHI, Register destLO, Register src1, Register src2, Condition c)
{
    return as_genmul(destHI, destLO, src1, src2, opm_umaal, NoSetCond, c);
}
BufferOffset
Assembler::as_mls(Register dest, Register acc, Register src1, Register src2, Condition c)
{
    return as_genmul(dest, acc, src1, src2, opm_mls, NoSetCond, c);
}

BufferOffset
Assembler::as_umull(Register destHI, Register destLO, Register src1, Register src2,
                    SetCond_ sc, Condition c)
{
    return as_genmul(destHI, destLO, src1, src2, opm_umull, sc, c);
}

BufferOffset
Assembler::as_umlal(Register destHI, Register destLO, Register src1, Register src2,
                    SetCond_ sc, Condition c)
{
    return as_genmul(destHI, destLO, src1, src2, opm_umlal, sc, c);
}

BufferOffset
Assembler::as_smull(Register destHI, Register destLO, Register src1, Register src2,
                    SetCond_ sc, Condition c)
{
    return as_genmul(destHI, destLO, src1, src2, opm_smull, sc, c);
}

BufferOffset
Assembler::as_smlal(Register destHI, Register destLO, Register src1, Register src2,
                    SetCond_ sc, Condition c)
{
    return as_genmul(destHI, destLO, src1, src2, opm_smlal, sc, c);
}

// Data transfer instructions: ldr, str, ldrb, strb.
// Using an int to differentiate between 8 bits and 32 bits is
// overkill, but meh
BufferOffset
Assembler::as_dtr(LoadStore ls, int size, Index mode,
                  Register rt, DTRAddr addr, Condition c, uint32_t *dest)
{
    JS_ASSERT (mode == Offset ||  (rt != addr.getBase() && pc != addr.getBase()));
    JS_ASSERT(size == 32 || size == 8);
    return writeInst( 0x04000000 | ls | (size == 8 ? 0x00400000 : 0) | mode | c |
                      RT(rt) | addr.encode(), dest);

}
class PoolHintData {
  public:
    enum LoadType {
        // set 0 to bogus, since that is the value most likely to be
        // accidentally left somewhere.
        poolBOGUS  = 0,
        poolDTR    = 1,
        poolBranch = 2,
        poolVDTR   = 3
    };

  private:
    uint32_t   index    : 17;
    uint32_t   cond     : 4;
    LoadType loadType : 2;
    uint32_t   destReg  : 5;
    uint32_t   ONES     : 4;

  public:
    void init(uint32_t index_, Assembler::Condition cond_, LoadType lt, const Register &destReg_) {
        index = index_;
        JS_ASSERT(index == index_);
        cond = cond_ >> 28;
        JS_ASSERT(cond == cond_ >> 28);
        loadType = lt;
        ONES = 0xfu;
        destReg = destReg_.code();
    }
    void init(uint32_t index_, Assembler::Condition cond_, LoadType lt, const VFPRegister &destReg_) {
        index = index_;
        JS_ASSERT(index == index_);
        cond = cond_ >> 28;
        JS_ASSERT(cond == cond_ >> 28);
        loadType = lt;
        ONES = 0xfu;
        destReg = destReg_.code();
    }
    Assembler::Condition getCond() {
        return Assembler::Condition(cond << 28);
    }

    Register getReg() {
        return Register::FromCode(destReg);
    }
    VFPRegister getVFPReg() {
        return VFPRegister(FloatRegister::FromCode(destReg));
    }

    int32_t getIndex() {
        return index;
    }
    void setIndex(uint32_t index_) {
        JS_ASSERT(ONES == 0xf && loadType != poolBOGUS);
        index = index_;
        JS_ASSERT(index == index_);
    }

    LoadType getLoadType() {
        // If this *was* a poolBranch, but the branch has already been bound
        // then this isn't going to look like a real poolhintdata, but we still
        // want to lie about it so everyone knows it *used* to be a branch.
        if (ONES != 0xf)
            return PoolHintData::poolBranch;
        return loadType;
    }

    bool isValidPoolHint() {
        // Most instructions cannot have a condition that is 0xf. Notable exceptions are
        // blx and the entire NEON instruction set. For the purposes of pool loads, and
        // possibly patched branches, the possible instructions are ldr and b, neither of
        // which can have a condition code of 0xf.
        return ONES == 0xf;
    }
};

union PoolHintPun {
    PoolHintData phd;
    uint32_t raw;
};

// Handles all of the other integral data transferring functions:
// ldrsb, ldrsh, ldrd, etc.
// size is given in bits.
BufferOffset
Assembler::as_extdtr(LoadStore ls, int size, bool IsSigned, Index mode,
                     Register rt, EDtrAddr addr, Condition c, uint32_t *dest)
{
    int extra_bits2 = 0;
    int extra_bits1 = 0;
    switch(size) {
      case 8:
        JS_ASSERT(IsSigned);
        JS_ASSERT(ls!=IsStore);
        extra_bits1 = 0x1;
        extra_bits2 = 0x2;
        break;
      case 16:
        //case 32:
        // doesn't need to be handled-- it is handled by the default ldr/str
        extra_bits2 = 0x01;
        extra_bits1 = (ls == IsStore) ? 0 : 1;
        if (IsSigned) {
            JS_ASSERT(ls != IsStore);
            extra_bits2 |= 0x2;
        }
        break;
      case 64:
        extra_bits2 = (ls == IsStore) ? 0x3 : 0x2;
        extra_bits1 = 0;
        break;
      default:
        JS_NOT_REACHED("SAY WHAT?");
    }
    return writeInst(extra_bits2 << 5 | extra_bits1 << 20 | 0x90 |
                     addr.encode() | RT(rt) | mode | c, dest);
}

BufferOffset
Assembler::as_dtm(LoadStore ls, Register rn, uint32_t mask,
                DTMMode mode, DTMWriteBack wb, Condition c)
{
    return writeInst(0x08000000 | RN(rn) | ls |
                     mode | mask | c | wb);
}

BufferOffset
Assembler::as_Imm32Pool(Register dest, uint32_t value, ARMBuffer::PoolEntry *pe, Condition c)
{
    PoolHintPun php;
    php.phd.init(0, c, PoolHintData::poolDTR, dest);
    return m_buffer.insertEntry(4, (uint8_t*)&php.raw, int32Pool, (uint8_t*)&value, pe);
}

BufferOffset
Assembler::as_BranchPool(uint32_t value, RepatchLabel *label, ARMBuffer::PoolEntry *pe, Condition c)
{
    PoolHintPun php;
    BufferOffset next = nextOffset();
    php.phd.init(0, c, PoolHintData::poolBranch, pc);
    m_buffer.markNextAsBranch();
    BufferOffset ret = m_buffer.insertEntry(4, (uint8_t*)&php.raw, int32Pool, (uint8_t*)&value, pe);
    // If this label is already bound, then immediately replace the stub load with
    // a correct branch.
    if (label->bound()) {
        BufferOffset dest(label);
        as_b(dest.diffB<BOffImm>(next), c, next);
    } else {
        label->use(next.getOffset());
    }
    return ret;
}


BufferOffset
Assembler::as_FImm64Pool(VFPRegister dest, double value, ARMBuffer::PoolEntry *pe, Condition c)
{
    JS_ASSERT(dest.isDouble());
    PoolHintPun php;
    php.phd.init(0, c, PoolHintData::poolVDTR, dest);
    return m_buffer.insertEntry(4, (uint8_t*)&php.raw, doublePool, (uint8_t*)&value, pe);
}
// Pool callbacks stuff:
void
Assembler::insertTokenIntoTag(uint32_t instSize, uint8_t *load_, int32_t token)
{
    uint32_t *load = (uint32_t*) load_;
    PoolHintPun php;
    php.raw = *load;
    php.phd.setIndex(token);
    *load = php.raw;
}
// patchConstantPoolLoad takes the address of the instruction that wants to be patched, and
//the address of the start of the constant pool, and figures things out from there.
bool
Assembler::patchConstantPoolLoad(void* loadAddr, void* constPoolAddr)
{
    PoolHintData data = *(PoolHintData*)loadAddr;
    uint32_t *instAddr = (uint32_t*) loadAddr;
    int offset = (char *)constPoolAddr - (char *)loadAddr;
    switch(data.getLoadType()) {
      case PoolHintData::poolBOGUS:
        JS_NOT_REACHED("bogus load type!");
      case PoolHintData::poolDTR:
        dummy->as_dtr(IsLoad, 32, Offset, data.getReg(),
                      DTRAddr(pc, DtrOffImm(offset+4*data.getIndex() - 8)), data.getCond(), instAddr);
        break;
      case PoolHintData::poolBranch:
        // Either this used to be a poolBranch, and the label was already bound, so it was
        // replaced with a real branch, or this may happen in the future.
        // If this is going to happen in the future, then the actual bits that are written here
        // don't matter (except the condition code, since that is always preserved across
        // patchings) but if it does not get bound later,
        // then we want to make sure this is a load from the pool entry (and the pool entry
        // should be NULL so it will crash).
        if (data.isValidPoolHint()) {
            dummy->as_dtr(IsLoad, 32, Offset, pc,
                          DTRAddr(pc, DtrOffImm(offset+4*data.getIndex() - 8)),
                          data.getCond(), instAddr);
        }
        break;
      case PoolHintData::poolVDTR:
        if ((offset + (8 * data.getIndex()) - 8) < -1023 ||
            (offset + (8 * data.getIndex()) - 8) > 1023)
        {
            return false;
        }
        dummy->as_vdtr(IsLoad, data.getVFPReg(),
                       VFPAddr(pc, VFPOffImm(offset+8*data.getIndex() - 8)), data.getCond(), instAddr);
        break;
    }
    return true;
}

uint32_t
Assembler::placeConstantPoolBarrier(int offset)
{
    // BUG: 700526
    // this is still an active path, however, we do not hit it in the test
    // suite at all.
    JS_NOT_REACHED("ARMAssembler holdover");
#if 0
    offset = (offset - sizeof(ARMWord)) >> 2;
    ASSERT((offset <= BOFFSET_MAX && offset >= BOFFSET_MIN));
    return AL | B | (offset & BRANCH_MASK);
#endif
    return -1;
}

// Control flow stuff:

// bx can *only* branch to a register
// never to an immediate.
BufferOffset
Assembler::as_bx(Register r, Condition c, bool isPatchable)
{
    BufferOffset ret = writeInst(((int) c) | op_bx | r.code());
    if (c == Always && !isPatchable)
        m_buffer.markGuard();
    return ret;
}
void
Assembler::writePoolGuard(BufferOffset branch, Instruction *dest, BufferOffset afterPool)
{
    BOffImm off = afterPool.diffB<BOffImm>(branch);
    *dest = InstBImm(off, Always);
}
// Branch can branch to an immediate *or* to a register.
// Branches to immediates are pc relative, branches to registers
// are absolute
BufferOffset
Assembler::as_b(BOffImm off, Condition c, bool isPatchable)
{
    m_buffer.markNextAsBranch();
    BufferOffset ret =writeInst(((int)c) | op_b | off.encode());
    if (c == Always && !isPatchable)
        m_buffer.markGuard();
    return ret;
}

BufferOffset
Assembler::as_b(Label *l, Condition c, bool isPatchable)
{
    if (m_buffer.oom()) {
        BufferOffset ret;
        return ret;
    }
    m_buffer.markNextAsBranch();
    if (l->bound()) {
        BufferOffset ret = as_nop();
        as_b(BufferOffset(l).diffB<BOffImm>(ret), c, ret);
        return ret;
    }

    int32_t old;
    BufferOffset ret;
    if (l->used()) {
        old = l->offset();
        // This will currently throw an assertion if we couldn't actually
        // encode the offset of the branch.
        ret = as_b(BOffImm(old), c, isPatchable);
    } else {
        old = LabelBase::INVALID_OFFSET;
        BOffImm inv;
        ret = as_b(inv, c, isPatchable);
    }
    int32_t check = l->use(ret.getOffset());
    JS_ASSERT(check == old);
    return ret;
}
BufferOffset
Assembler::as_b(BOffImm off, Condition c, BufferOffset inst)
{
    *editSrc(inst) = InstBImm(off, c);
    return inst;
}

// blx can go to either an immediate or a register.
// When blx'ing to a register, we change processor state
// depending on the low bit of the register
// when blx'ing to an immediate, we *always* change processor state.

BufferOffset
Assembler::as_blx(Register r, Condition c)
{
    return writeInst(((int) c) | op_blx | r.code());
}

// bl can only branch to an pc-relative immediate offset
// It cannot change the processor state.
BufferOffset
Assembler::as_bl(BOffImm off, Condition c)
{
    return writeInst(((int)c) | op_bl | off.encode());
}

BufferOffset
Assembler::as_bl(Label *l, Condition c)
{
    if (l->bound()) {
        BufferOffset ret = as_nop();
        as_bl(BufferOffset(l).diffB<BOffImm>(ret), c, ret);
        return ret;
    }

    int32_t old;
    BufferOffset ret;
    // See if the list was empty :(
    if (l->used()) {
        // This will currently throw an assertion if we couldn't actually
        // encode the offset of the branch.
        old = l->offset();
        ret = as_bl(BOffImm(old), c);
    } else {
        old = LabelBase::INVALID_OFFSET;
        BOffImm inv;
        ret = as_bl(inv, c);
    }
    int32_t check = l->use(ret.getOffset());
    JS_ASSERT(check == old);
    return ret;
}
BufferOffset
Assembler::as_bl(BOffImm off, Condition c, BufferOffset inst)
{
    *editSrc(inst) = InstBLImm(off, c);
    return inst;
}

// VFP instructions!
enum vfp_tags {
    vfp_tag   = 0x0C000A00,
    vfp_arith = 0x02000000
};
BufferOffset
Assembler::writeVFPInst(vfp_size sz, uint32_t blob, uint32_t *dest)
{
    JS_ASSERT((sz & blob) == 0);
    JS_ASSERT((vfp_tag & blob) == 0);
    return writeInst(vfp_tag | sz | blob, dest);
}

// Unityped variants: all registers hold the same (ieee754 single/double)
// notably not included are vcvt; vmov vd, #imm; vmov rt, vn.
BufferOffset
Assembler::as_vfp_float(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  VFPOp op, Condition c)
{
    // Make sure we believe that all of our operands are the same kind
    JS_ASSERT(vd.equiv(vn) && vd.equiv(vm));
    vfp_size sz = vd.isDouble() ? isDouble : isSingle;
    return writeVFPInst(sz, VD(vd) | VN(vn) | VM(vm) | op | vfp_arith | c);
}

BufferOffset
Assembler::as_vadd(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c)
{
    return as_vfp_float(vd, vn, vm, opv_add, c);
}

BufferOffset
Assembler::as_vdiv(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c)
{
    return as_vfp_float(vd, vn, vm, opv_div, c);
}

BufferOffset
Assembler::as_vmul(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c)
{
    return as_vfp_float(vd, vn, vm, opv_mul, c);
}

BufferOffset
Assembler::as_vnmul(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c)
{
    return as_vfp_float(vd, vn, vm, opv_mul, c);
    JS_NOT_REACHED("Feature NYI");
}

BufferOffset
Assembler::as_vnmla(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c)
{
    JS_NOT_REACHED("Feature NYI");
    return BufferOffset();
}

BufferOffset
Assembler::as_vnmls(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c)
{
    JS_NOT_REACHED("Feature NYI");
    return BufferOffset();
}

BufferOffset
Assembler::as_vneg(VFPRegister vd, VFPRegister vm, Condition c)
{
    return as_vfp_float(vd, NoVFPRegister, vm, opv_neg, c);
}

BufferOffset
Assembler::as_vsqrt(VFPRegister vd, VFPRegister vm, Condition c)
{
    return as_vfp_float(vd, NoVFPRegister, vm, opv_sqrt, c);
}

BufferOffset
Assembler::as_vabs(VFPRegister vd, VFPRegister vm, Condition c)
{
    return as_vfp_float(vd, NoVFPRegister, vm, opv_abs, c);
}

BufferOffset
Assembler::as_vsub(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c)
{
    return as_vfp_float(vd, vn, vm, opv_sub, c);
}

BufferOffset
Assembler::as_vcmp(VFPRegister vd, VFPRegister vm,
                 Condition c)
{
    return as_vfp_float(vd, NoVFPRegister, vm, opv_cmp, c);
}
BufferOffset
Assembler::as_vcmpz(VFPRegister vd, Condition c)
{
    return as_vfp_float(vd, NoVFPRegister, NoVFPRegister, opv_cmpz, c);
}

// Specifically, a move between two same sized-registers.
BufferOffset
Assembler::as_vmov(VFPRegister vd, VFPRegister vsrc, Condition c)
{
    return as_vfp_float(vd, NoVFPRegister, vsrc, opv_mov, c);
}
//xfer between Core and VFP

// Unlike the next function, moving between the core registers and vfp
// registers can't be *that* properly typed.  Namely, since I don't want to
// munge the type VFPRegister to also include core registers.  Thus, the core
// and vfp registers are passed in based on their type, and src/dest is
// determined by the float2core.

BufferOffset
Assembler::as_vxfer(Register vt1, Register vt2, VFPRegister vm, FloatToCore_ f2c,
                    Condition c, int idx)
{
    vfp_size sz = isSingle;
    if (vm.isDouble()) {
        // Technically, this can be done with a vmov à la ARM ARM under vmov
        // however, that requires at least an extra bit saying if the
        // operation should be performed on the lower or upper half of the
        // double.  Moving a single to/from 2N/2N+1 isn't equivalent,
        // since there are 32 single registers, and 32 double registers
        // so there is no way to encode the last 16 double registers.
        sz = isDouble;
        JS_ASSERT(idx == 0 || idx == 1);
        // If we are transferring a single half of the double
        // then it must be moving a VFP reg to a core reg.
        if (vt2 == InvalidReg)
            JS_ASSERT(f2c == FloatToCore);
        idx = idx << 21;
    } else {
        JS_ASSERT(idx == 0);
    }
    VFPXferSize xfersz = WordTransfer;
    uint32_t (*encodeVFP)(VFPRegister) = VN;
    if (vt2 != InvalidReg) {
        // We are doing a 64 bit transfer.
        xfersz = DoubleTransfer;
        encodeVFP = VM;
    }

    return writeVFPInst(sz, xfersz | f2c | c |
                        RT(vt1) | maybeRN(vt2) | encodeVFP(vm) | idx);
}
enum vcvt_destFloatness {
    toInteger = 1 << 18,
    toFloat  = 0 << 18
};
enum vcvt_toZero {
    toZero = 1 << 7, // use the default rounding mode, which rounds truncates
    toFPSCR = 0 << 7 // use whatever rounding mode the fpscr specifies
};
enum vcvt_Signedness {
    toSigned   = 1 << 16,
    toUnsigned = 0 << 16,
    fromSigned   = 1 << 7,
    fromUnsigned = 0 << 7
};

// our encoding actually allows just the src and the dest (and their types)
// to uniquely specify the encoding that we are going to use.
BufferOffset
Assembler::as_vcvt(VFPRegister vd, VFPRegister vm, bool useFPSCR,
                   Condition c)
{
    // Unlike other cases, the source and dest types cannot be the same
    JS_ASSERT(!vd.equiv(vm));
    vfp_size sz = isDouble;
    if (vd.isFloat() && vm.isFloat()) {
        // Doing a float -> float conversion
        if (vm.isSingle())
            sz = isSingle;
        return writeVFPInst(sz, c | 0x02B700C0 |
                            VM(vm) | VD(vd));
    }

    // At least one of the registers should be a float.
    vcvt_destFloatness destFloat;
    vcvt_Signedness opSign;
    vcvt_toZero doToZero = toFPSCR;
    JS_ASSERT(vd.isFloat() || vm.isFloat());
    if (vd.isSingle() || vm.isSingle()) {
        sz = isSingle;
    }
    if (vd.isFloat()) {
        destFloat = toFloat;
        opSign = (vm.isSInt()) ? fromSigned : fromUnsigned;
    } else {
        destFloat = toInteger;
        opSign = (vd.isSInt()) ? toSigned : toUnsigned;
        doToZero = useFPSCR ? toFPSCR : toZero;
    }
    return writeVFPInst(sz, c | 0x02B80040 | VD(vd) | VM(vm) | destFloat | opSign | doToZero);
}

BufferOffset
Assembler::as_vcvtFixed(VFPRegister vd, bool isSigned, uint32_t fixedPoint, bool toFixed, Condition c)
{
    JS_ASSERT(vd.isFloat());
    uint32_t sx = 0x1;
    vfp_size sf = vd.isDouble() ? isDouble : isSingle;
    int32_t imm5 = fixedPoint;
    imm5 = (sx ? 32 : 16) - imm5;
    JS_ASSERT(imm5 >= 0);
    imm5 = imm5 >> 1 | (imm5 & 1) << 6;
    return writeVFPInst(sf, 0x02BA0040 | VD(vd) | toFixed << 18 | sx << 7 |
                        (!isSigned) << 16 | imm5 | c);
}

// xfer between VFP and memory
BufferOffset
Assembler::as_vdtr(LoadStore ls, VFPRegister vd, VFPAddr addr,
                   Condition c /* vfp doesn't have a wb option*/,
                   uint32_t *dest)
{
    vfp_size sz = vd.isDouble() ? isDouble : isSingle;
    return writeVFPInst(sz, ls | 0x01000000 | addr.encode() | VD(vd) | c, dest);
}

// VFP's ldm/stm work differently from the standard arm ones.
// You can only transfer a range

BufferOffset
Assembler::as_vdtm(LoadStore st, Register rn, VFPRegister vd, int length,
                 /*also has update conditions*/Condition c)
{
    JS_ASSERT(length <= 16 && length >= 0);
    vfp_size sz = vd.isDouble() ? isDouble : isSingle;

    if (vd.isDouble())
        length *= 2;

    return writeVFPInst(sz, dtmLoadStore | RN(rn) | VD(vd) |
                        length |
                        dtmMode | dtmUpdate | dtmCond);
}

BufferOffset
Assembler::as_vimm(VFPRegister vd, VFPImm imm, Condition c)
{
    vfp_size sz = vd.isDouble() ? isDouble : isSingle;

    // Don't know how to handle this right now.
    if (!vd.isDouble())
        JS_NOT_REACHED("non-double immediate");

    return writeVFPInst(sz,  c | imm.encode() | VD(vd) | 0x02B00000);

}
BufferOffset
Assembler::as_vmrs(Register r, Condition c)
{
    return writeInst(c | 0x0ef10a10 | RT(r));
}

bool
Assembler::nextLink(BufferOffset b, BufferOffset *next)
{
    Instruction branch = *editSrc(b);
    JS_ASSERT(branch.is<InstBranchImm>());

    BOffImm destOff;
    branch.as<InstBranchImm>()->extractImm(&destOff);
    if (destOff.isInvalid())
        return false;

    // Propagate the next link back to the caller, by
    // constructing a new BufferOffset into the space they
    // provided.
    new (next) BufferOffset(destOff.decode());
    return true;
}

void
Assembler::bind(Label *label, BufferOffset boff)
{
    if (label->used()) {
        bool more;
        // If our caller didn't give us an explicit target to bind to
        // then we want to bind to the location of the next instruction
        BufferOffset dest = boff.assigned() ? boff : nextOffset();
        BufferOffset b(label);
        do {
            BufferOffset next;
            more = nextLink(b, &next);
            Instruction branch = *editSrc(b);
            Condition c;
            branch.extractCond(&c);
            if (branch.is<InstBImm>())
                as_b(dest.diffB<BOffImm>(b), c, b);
            else if (branch.is<InstBLImm>())
                as_bl(dest.diffB<BOffImm>(b), c, b);
            else
                JS_NOT_REACHED("crazy fixup!");
            b = next;
        } while (more);
    }
    label->bind(nextOffset().getOffset());
}

void
Assembler::bind(RepatchLabel *label)
{
    BufferOffset dest = nextOffset();
    if (label->used()) {
        // If the label has a use, then change this use to refer to
        // the bound label;
        BufferOffset branchOff(label->offset());
        // Since this was created with a RepatchLabel, the value written in the
        // instruction stream is not branch shaped, it is PoolHintData shaped.
        Instruction *branch = editSrc(branchOff);
        PoolHintPun p;
        p.raw = branch->encode();
        Condition cond;
        if (p.phd.isValidPoolHint())
            cond = p.phd.getCond();
        else
            branch->extractCond(&cond);
        as_b(dest.diffB<BOffImm>(branchOff), cond, branchOff);
    }
    label->bind(dest.getOffset());
}

void
Assembler::retarget(Label *label, Label *target)
{
    if (label->used()) {
        if (target->bound()) {
            bind(label, BufferOffset(target));
        } else {
            // The target is unbound.  We can just take the head of the list
            // hanging off of label, and dump that into target.
            DebugOnly<uint32_t> prev = target->use(label->offset());
            JS_ASSERT((int32_t)prev == Label::INVALID_OFFSET);
        }
    }
    label->reset();

}


void dbg_break() {}
static int stopBKPT = -1;
void
Assembler::as_bkpt()
{
    // This is a count of how many times a breakpoint instruction has been generated.
    // It is embedded into the instruction for debugging purposes.  gdb will print "bkpt xxx"
    // when you attempt to dissassemble a breakpoint with the number xxx embedded into it.
    // If this breakpoint is being hit, then you can run (in gdb)
    // >b dbg_break
    // >b main
    // >commands
    // >set stopBKPT = xxx
    // >c
    // >end

    // which will set a breakpoint on the function dbg_break above
    // set a scripted breakpoint on main that will set the (otherwise unmodified)
    // value to the number of the breakpoint, so dbg_break will actuall be called
    // and finally, when you run the executable, execution will halt when that
    // breakpoint is generated
    static int hit = 0;
    if (stopBKPT == hit)
        dbg_break();
    writeInst(0xe1200070 | (hit & 0xf) | ((hit & 0xfff0)<<4));
    hit++;
}

void
Assembler::dumpPool()
{
    m_buffer.flushPool();
}

void
Assembler::flushBuffer()
{
    m_buffer.flushPool();
}

void
Assembler::enterNoPool()
{
    m_buffer.enterNoPool();
}

void
Assembler::leaveNoPool()
{
    m_buffer.leaveNoPool();
}

BufferOffset
Assembler::as_jumpPool(uint32_t numCases)
{
    if (numCases == 0)
        return BufferOffset();

    BufferOffset ret = writeInst(-1);
    for (uint32_t i = 1; i < numCases; i++)
        writeInst(-1);

    return ret;
}

ptrdiff_t
Assembler::getBranchOffset(const Instruction *i_)
{
    if (!i_->is<InstBranchImm>())
        return 0;

    InstBranchImm *i = i_->as<InstBranchImm>();
    BOffImm dest;
    i->extractImm(&dest);
    return dest.decode();
}
void
Assembler::retargetNearBranch(Instruction *i, int offset, bool final)
{
    Assembler::Condition c;
    i->extractCond(&c);
    retargetNearBranch(i, offset, c, final);
}

void
Assembler::retargetNearBranch(Instruction *i, int offset, Condition cond, bool final)
{
    // Retargeting calls is totally unsupported!
    JS_ASSERT_IF(i->is<InstBranchImm>(), i->is<InstBImm>());
    new (i) InstBImm(BOffImm(offset), cond);

    // Flush the cache, since an instruction was overwritten
    if (final)
        AutoFlushCache::updateTop(uintptr_t(i), 4);
}

void
Assembler::retargetFarBranch(Instruction *i, uint8_t **slot, uint8_t *dest, Condition cond)
{
    int32_t offset = reinterpret_cast<uint8_t*>(slot) - reinterpret_cast<uint8_t*>(i);
    if (!i->is<InstLDR>()) {
        new (i) InstLDR(Offset, pc, DTRAddr(pc, DtrOffImm(offset - 8)), cond);
        AutoFlushCache::updateTop(uintptr_t(i), 4);
    }
    *slot = dest;

}

struct PoolHeader : Instruction {
    struct Header
    {
        // size should take into account the pool header.
        // size is in units of Instruction (4bytes), not byte
        uint32_t size : 15;
        bool isNatural : 1;
        uint32_t ONES : 16;

        Header(int size_, bool isNatural_)
          : size(size_),
            isNatural(isNatural_),
            ONES(0xffff)
        { }

        Header(const Instruction *i) {
            JS_STATIC_ASSERT(sizeof(Header) == sizeof(uint32_t));
            memcpy(this, i, sizeof(Header));
            JS_ASSERT(ONES == 0xffff);
        }

        uint32_t raw() const {
            JS_STATIC_ASSERT(sizeof(Header) == sizeof(uint32_t));
            uint32_t dest;
            memcpy(&dest, this, sizeof(Header));
            return dest;
        }
    };

    PoolHeader(int size_, bool isNatural_)
      : Instruction(Header(size_, isNatural_).raw(), true)
    { }

    uint32_t size() const {
        Header tmp(this);
        return tmp.size;
    }
    uint32_t isNatural() const {
        Header tmp(this);
        return tmp.isNatural;
    }
    static bool isTHIS(const Instruction &i) {
        return (*i.raw() & 0xffff0000) == 0xffff0000;
    }
    static const PoolHeader *asTHIS(const Instruction &i) {
        if (!isTHIS(i))
            return NULL;
        return static_cast<const PoolHeader*>(&i);
    }
};


void
Assembler::writePoolHeader(uint8_t *start, Pool *p, bool isNatural)
{
    STATIC_ASSERT(sizeof(PoolHeader) == 4);
    uint8_t *pool = start+4;
    // go through the usual rigaramarole to get the size of the pool.
    pool = p[0].addPoolSize(pool);
    pool = p[1].addPoolSize(pool);
    pool = p[1].other->addPoolSize(pool);
    pool = p[0].other->addPoolSize(pool);
    uint32_t size = pool - start;
    JS_ASSERT((size & 3) == 0);
    size = size >> 2;
    JS_ASSERT(size < (1 << 15));
    PoolHeader header(size, isNatural);
    *(PoolHeader*)start = header;
}


void
Assembler::writePoolFooter(uint8_t *start, Pool *p, bool isNatural)
{
    return;
}

// The size of an arbitrary 32-bit call in the instruction stream.
// On ARM this sequence is |pc = ldr pc - 4; imm32| given that we
// never reach the imm32.
uint32_t
Assembler::patchWrite_NearCallSize()
{
    return sizeof(uint32_t);
}
void
Assembler::patchWrite_NearCall(CodeLocationLabel start, CodeLocationLabel toCall)
{
    Instruction *inst = (Instruction *) start.raw();
    // Overwrite whatever instruction used to be here with a call.
    // Since the destination is in the same function, it will be within range of the 24<<2 byte
    // bl instruction.
    uint8_t *dest = toCall.raw();
    new (inst) InstBLImm(BOffImm(dest - (uint8_t*)inst) , Always);
    // Ensure everyone sees the code that was just written into memory.

    AutoFlushCache::updateTop(uintptr_t(inst), 4);

}
void
Assembler::patchDataWithValueCheck(CodeLocationLabel label, ImmWord newValue, ImmWord expectedValue)
{
    Instruction *ptr = (Instruction *) label.raw();
    InstructionIterator iter(ptr);
    Register dest;
    Assembler::RelocStyle rs;
    const uint32_t *val = getPtr32Target(&iter, &dest, &rs);
    JS_ASSERT((uint32_t)val == expectedValue.value);
    reinterpret_cast<MacroAssemblerARM*>(dummy)->ma_movPatchable(Imm32(newValue.value), dest, Always, rs, ptr);

    AutoFlushCache::updateTop(uintptr_t(ptr), 4);
    AutoFlushCache::updateTop(uintptr_t(ptr->next()), 4);
}

// This just stomps over memory with 32 bits of raw data. Its purpose is to
// overwrite the call of JITed code with 32 bits worth of an offset. This will
// is only meant to function on code that has been invalidated, so it should
// be totally safe. Since that instruction will never be executed again, a
// ICache flush should not be necessary
void
Assembler::patchWrite_Imm32(CodeLocationLabel label, Imm32 imm) {
    // Raw is going to be the return address.
    uint32_t *raw = (uint32_t*)label.raw();
    // Overwrite the 4 bytes before the return address, which will
    // end up being the call instruction.
    *(raw-1) = imm.value;
}


uint8_t *
Assembler::nextInstruction(uint8_t *inst_, uint32_t *count)
{
    Instruction *inst = reinterpret_cast<Instruction*>(inst_);
    if (count != NULL)
        *count += sizeof(Instruction);
    return reinterpret_cast<uint8_t*>(inst->next());
}

static bool
InstIsGuard(Instruction *inst, const PoolHeader **ph)
{
    Assembler::Condition c;
    inst->extractCond(&c);
    if (c != Assembler::Always)
        return false;
    if (!(inst->is<InstBXReg>() || inst->is<InstBImm>()))
        return false;
    // See if the next instruction is a pool header.
    *ph = (inst+1)->as<const PoolHeader>();
    return *ph != NULL;
}

static bool
InstIsBNop(Instruction *inst) {
    // In some special situations, it is necessary to insert a NOP
    // into the instruction stream that nobody knows about, since nobody should know about
    // it, make sure it gets skipped when Instruction::next() is called.
    // this generates a very specific nop, namely a branch to the next instruction.
    Assembler::Condition c;
    inst->extractCond(&c);
    if (c != Assembler::Always)
        return false;
    if (!inst->is<InstBImm>())
        return false;
    InstBImm *b = inst->as<InstBImm>();
    BOffImm offset;
    b->extractImm(&offset);
    return offset.decode() == 4;
}

static bool
InstIsArtificialGuard(Instruction *inst, const PoolHeader **ph)
{
    if (!InstIsGuard(inst, ph))
        return false;
    return !(*ph)->isNatural();
}

// Cases to be handled:
// 1) no pools or branches in sight => return this+1
// 2) branch to next instruction => return this+2, because a nop needed to be inserted into the stream.
// 3) this+1 is an artificial guard for a pool => return first instruction after the pool
// 4) this+1 is a natural guard => return the branch
// 5) this is a branch, right before a pool => return first instruction after the pool
// in assembly form:
// 1) add r0, r0, r0 <= this
//    add r1, r1, r1 <= returned value
//    add r2, r2, r2
//
// 2) add r0, r0, r0 <= this
//    b foo
//    foo:
//    add r2, r2, r2 <= returned value
//
// 3) add r0, r0, r0 <= this
//    b after_pool;
//    .word 0xffff0002  # bit 15 being 0 indicates that the branch was not requested by the assembler
//    0xdeadbeef        # the 2 indicates that there is 1 pool entry, and the pool header
//    add r4, r4, r4 <= returned value
// 4) add r0, r0, r0 <= this
//    b after_pool  <= returned value
//    .word 0xffff8002  # bit 15 being 1 indicates that the branch was requested by the assembler
//    0xdeadbeef
//    add r4, r4, r4
// 5) b after_pool  <= this
//    .word 0xffff8002  # bit 15 has no bearing on the returned value
//    0xdeadbeef
//    add r4, r4, r4  <= returned value

Instruction *
Instruction::next()
{
    Instruction *ret = this+1;
    const PoolHeader *ph;
    // If this is a guard, and the next instruction is a header, always work around the pool
    // If it isn't a guard, then start looking ahead.
    if (InstIsGuard(this, &ph))
        return ret + ph->size();
    if (InstIsArtificialGuard(ret, &ph))
        return ret + 1 + ph->size();
    if (InstIsBNop(ret))
        return ret + 1;
    return ret;
}

void
Assembler::ToggleToJmp(CodeLocationLabel inst_)
{
    uint32_t *ptr = (uint32_t *)inst_.raw();

    DebugOnly<Instruction *> inst = (Instruction *)inst_.raw();
    JS_ASSERT(inst->is<InstCMP>());

    // Zero bits 20-27, then set 24-27 to be correct for a branch.
    // 20-23 will be party of the B's immediate, and should be 0.
    *ptr = (*ptr & ~(0xff << 20)) | (0xa0 << 20);
    AutoFlushCache::updateTop((uintptr_t)ptr, 4);
}

void
Assembler::ToggleToCmp(CodeLocationLabel inst_)
{
    uint32_t *ptr = (uint32_t *)inst_.raw();

    DebugOnly<Instruction *> inst = (Instruction *)inst_.raw();
    JS_ASSERT(inst->is<InstBImm>());

    // Ensure that this masking operation doesn't affect the offset of the
    // branch instruction when it gets toggled back.
    JS_ASSERT((*ptr & (0xf << 20)) == 0);

    // Also make sure that the CMP is valid. Part of having a valid CMP is that
    // all of the bits describing the destination in most ALU instructions are
    // all unset (looks like it is encoding r0).
    JS_ASSERT(toRD(*inst) == r0);

    // Zero out bits 20-27, then set them to be correct for a compare.
    *ptr = (*ptr & ~(0xff << 20)) | (0x35 << 20);

    AutoFlushCache::updateTop((uintptr_t)ptr, 4);
}

void
AutoFlushCache::update(uintptr_t newStart, size_t len)
{
    uintptr_t newStop = newStart + len;
    used_ = true;
    if (!start_) {
        IonSpewCont(IonSpew_CacheFlush,  ".");
        start_ = newStart;
        stop_ = newStop;
        return;
    }

    if (newStop < start_ - 4096 || newStart > stop_ + 4096) {
        // If this would add too many pages to the range, bail and just do the flush now.
        IonSpewCont(IonSpew_CacheFlush, "*");
        JSC::ExecutableAllocator::cacheFlush((void*)newStart, len);
        return;
    }
    start_ = Min(start_, newStart);
    stop_ = Max(stop_, newStop);
    IonSpewCont(IonSpew_CacheFlush, ".");
}

AutoFlushCache::~AutoFlushCache()
{
   if (!myCompartment_)
        return;

    flushAnyway();
    IonSpewCont(IonSpew_CacheFlush, ">", name_);
    if (myCompartment_->flusher() == this) {
        IonSpewFin(IonSpew_CacheFlush);
        myCompartment_->setFlusher(NULL);
    }

}
void
AutoFlushCache::flushAnyway()
{
    if (!myCompartment_)
        return;

    IonSpewCont(IonSpew_CacheFlush, "|", name_);

    if (!used_)
        return;

    if (start_) {
        JSC::ExecutableAllocator::cacheFlush((void*)start_, (size_t)(stop_ - start_ + sizeof(Instruction)));
    } else {
        JSC::ExecutableAllocator::cacheFlush(NULL, 0xff000000);
    }
    used_ = false;

}
Assembler *Assembler::dummy = NULL;
