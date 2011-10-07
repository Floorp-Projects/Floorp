/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "Assembler-arm.h"
#include "jsgcmark.h"

using namespace js;
using namespace js::ion;
int
js::ion::RT(Register r)
{
    return r.code() << 12;
}
int
js::ion::RN(Register r)
{
    return r.code() << 16;
}
int
js::ion::RD(Register r)
{
    return r.code() << 12;
}
void
Assembler::executableCopy(uint8 *buffer)
{
    executableCopy((void*)buffer);
    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch &rp = jumps_[i];
        JS_NOT_REACHED("Feature NYI");
        //JSC::ARMAssembler::setRel32(buffer + rp.offset, rp.target);
    }
}

class RelocationIterator
{
    CompactBufferReader reader_;
    uint32 offset_;

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

    uint32 offset() const {
        return offset_;
    }
};

static inline IonCode *
CodeFromJump(uint8 *jump)
{
    JS_NOT_REACHED("Feature NYI");
#if 0
    uint8 *target = (uint8 *)JSC::ARMAssembler::getRel32Target(jump);
    return IonCode::FromExecutable(target);
#endif
    return NULL;
}

void
Assembler::TraceRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader)
{
    JS_NOT_REACHED("Feature NYI");
#if 0
    RelocationIterator iter(reader);
    while (iter.read()) {
        IonCode *child = CodeFromJump(code->raw() + iter.offset());
        MarkIonCode(trc, child, "rel32");
    };
#endif
}

void
Assembler::copyRelocationTable(uint8 *dest)
{
    if (relocations_.length())
        memcpy(dest, relocations_.buffer(), relocations_.length());
}

void
Assembler::trace(JSTracer *trc)
{
    JS_NOT_REACHED("Feature NYI");
#if 0
    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch &rp = jumps_[i];
        if (rp.kind == Relocation::CODE)
            MarkIonCode(trc, IonCode::FromExecutable((uint8 *)rp.target), "masmrel32");
    }
#endif
}

void
Assembler::executableCopy(void *buffer)
{
    ASSERT(m_buffer.sizeOfConstantPool() == 0);
    memcpy(buffer, m_buffer.data(), m_buffer.size());
    // whoops, can't call this; it is ARMAssembler-specific.
    // Presently, we don't generate anything that needs to be fixed up.
    // In the future, things that *do* need to be fixed up will
    // likely look radically different from things that needed to be fixed
    // up in JM.
    // TODO: implement this!
#if 0
    fixUpOffsets(buffer);
#endif
}

void
Assembler::processDeferredData(IonCode *code, uint8 *data)
{
    for (size_t i = 0; i < data_.length(); i++) {
        DeferredData *deferred = data_[i];
        Bind(code, deferred->label(), data + deferred->offset());
        deferred->copy(code, data + deferred->offset());
    }
}

void
Assembler::processCodeLabels(IonCode *code)
{
    for (size_t i = 0; i < codeLabels_.length(); i++) {
        CodeLabel *label = codeLabels_[i];
        Bind(code, label->dest(), code->raw() + label->src()->offset());
    }
}

Assembler::Condition
Assembler::inverseCondition(Condition cond)
{
    switch (cond) {
      case Equal:
        //case Zero:
        return NotEqual;
      case Above:
        return BelowOrEqual;
      case AboveOrEqual:
        return Below;
      case Below:
        return AboveOrEqual;
      case BelowOrEqual:
        return Above;
      case LessThan:
        return GreaterThanOrEqual;
        //    case LessThanOrEqual:
        //        return GreaterThan;
      case GreaterThan:
        return LessThanOrEqual;
      case GreaterThanOrEqual:
        return LessThan;
        // still missing overflow, signed.
      default:
        JS_NOT_REACHED("Comparisons other than LT, LE, GT, GE not yet supported");
        return Equal;
    }

}

Imm8::TwoImm8mData
Imm8::encodeTwoImms(uint32 imm)
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
    uint32 imm1, imm2;
    int left = (js_bitscan_clz32(imm)) & 30;
    uint32 no_n1 = imm & ~(0xff << (24 - left));
    // not technically needed: this case only happens if we can encode
    // as a single imm8m.  There is a perfectly reasonable encoding in this
    // case, but we shouldn't encourage people to do things like this.
    if (no_n1 == 0)
        return TwoImm8mData();
    int mid = ((js_bitscan_clz32(no_n1)) & 30);
    uint32 no_n2 =
        no_n1  & ~((0xff << ((24 - mid)&31)) | 0xff >> ((8 + mid)&31));
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
        return TwoImm8mData(datastore::Imm8mData(imm1,imm1shift),
                            datastore::Imm8mData(imm2, imm2shift));
    } else {
        // either it wraps, or it does not fit.
        // if we initially chopped off more than 8 bits, then it won't fit.
        if (left >= 8)
            return TwoImm8mData();
        int right = 32 - (js_bitscan_clz32(no_n2) & 30);
        // all remaining set bits *must* fit into the lower 8 bits
        // the right == 8 case should be handled by the previous case.
        if (right > 8) {
            return TwoImm8mData();
        }
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
            if (no_n2 != 0) {
                return TwoImm8mData();
            }
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
        return TwoImm8mData(datastore::Imm8mData(imm1,imm1shift),
                            datastore::Imm8mData(imm2, imm2shift));
    }
}

ALUOp
ion::ALUNeg(ALUOp op, Imm32 *imm)
{
    // find an alternate ALUOp to get the job done, and use a different imm.
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
        // orr has orn on thumb2 only.
      default:
        break;
    }
    return op;
}

bool
ion::can_dbl(ALUOp op)
{
    // some instructions can't be processed as two separate instructions
    // such as and, and possibly add (when we're setting ccodes).
    return false;
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
    return false;
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
    }
    return op;
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
