/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Mandelin <dmandelin@mozilla.com>
 *   David Anderson <danderson@mozilla.com>
 *   Chris Leary <cdleary@mozilla.com>
 *   Jacob Bramley <Jacob.Bramely@arm.com>
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

#if !defined jsjaeger_ic_labels_h__ && defined JS_METHODJIT
#define jsjaeger_ic_labels_h__

#include "methodjit/BaseCompiler.h"

class ICOffsetInitializer {
  public:
    ICOffsetInitializer();
};

namespace js {
namespace mjit {
namespace ic {

/*
 * On x64 and ARM, we record offsets into the labels data structures at runtime
 * instead of using hardcoded offsets into the instruction stream, as we do on
 * x86.
 *
 * This is done on x64 because of variable-width instruction encoding when
 * using the extended register set. It is done on ARM for ease of
 * implementation.
 */

#if defined JS_CPU_X64 || defined JS_CPU_ARM
# define JS_HAS_IC_LABELS
#endif

/* GetPropCompiler */
struct GetPropLabels : MacroAssemblerTypedefs {
    friend class ::ICOffsetInitializer;

    void setValueLoad(MacroAssembler &masm, Label fastPathRejoin, Label fastValueLoad) {
        int offset = masm.differenceBetween(fastPathRejoin, fastValueLoad);
#ifdef JS_HAS_IC_LABELS
        inlineValueLoadOffset = offset;
#endif
        /* 
         * Note: the offset between the type and data loads for x86 is asserted
         * in NunboxAssembler::loadValueWithAddressOffsetPatch.
         */
        JS_ASSERT(offset == inlineValueLoadOffset);
        (void) offset;
    }

    CodeLocationLabel getValueLoad(CodeLocationLabel fastPathRejoin) {
        return fastPathRejoin.labelAtOffset(inlineValueLoadOffset);
    }

    void setDslotsLoad(MacroAssembler &masm, Label fastPathRejoin, Label dslotsLoad) {
        int offset = masm.differenceBetween(fastPathRejoin, dslotsLoad);
        setDslotsLoadOffset(offset);
    }

    CodeLocationInstruction getDslotsLoad(CodeLocationLabel fastPathRejoin) {
        return fastPathRejoin.instructionAtOffset(getDslotsLoadOffset());
    }

    void setInlineShapeData(MacroAssembler &masm, Label shapeGuard, DataLabel32 inlineShape) {
        int offset = masm.differenceBetween(shapeGuard, inlineShape);
        setInlineShapeOffset(offset);
    }

    CodeLocationDataLabel32 getInlineShapeData(CodeLocationLabel fastShapeGuard) {
        return fastShapeGuard.dataLabel32AtOffset(getInlineShapeOffset());
    }

    /*
     * Note: on x64, the base is the inlineShapeLabel DataLabel32, whereas on other
     * platforms the base is the shapeGuard.
     */
    template <typename T>
    void setInlineShapeJump(MacroAssembler &masm, T base, Label afterJump) {
        setInlineShapeJumpOffset(masm.differenceBetween(base, afterJump));
    }

    CodeLocationJump getInlineShapeJump(CodeLocationLabel fastShapeGuard) {
        return fastShapeGuard.jumpAtOffset(getInlineShapeJumpOffset());
    }

    void setInlineTypeJump(MacroAssembler &masm, Label fastPathStart, Label afterTypeJump) {
        int offset = masm.differenceBetween(fastPathStart, afterTypeJump);
        setInlineTypeJumpOffset(offset);
    }

    CodeLocationJump getInlineTypeJump(CodeLocationLabel fastPathStart) {
        return fastPathStart.jumpAtOffset(getInlineTypeJumpOffset());
    }

    void setStubShapeJump(MacroAssembler &masm, Label stubStart, Label shapeJump) {
        int offset = masm.differenceBetween(stubStart, shapeJump);
        setStubShapeJumpOffset(offset);
    }

    /* Offset-based interface */

    void setDslotsLoadOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        dslotsLoadOffset = offset;
#endif
        JS_ASSERT(offset == dslotsLoadOffset);
    }

    void setInlineShapeOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        inlineShapeOffset = offset;
#endif
        JS_ASSERT(offset == inlineShapeOffset);
    }
    
    void setStubShapeJumpOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        stubShapeJumpOffset = offset;
#endif
        JS_ASSERT(offset == stubShapeJumpOffset);
    }

    int getInlineShapeJumpOffset() {
#if defined JS_CPU_X64
        return getInlineShapeOffset() + INLINE_SHAPE_JUMP;
#else
        return POST_INST_OFFSET(INLINE_SHAPE_JUMP);
#endif
    }

    void setInlineShapeJumpOffset(int offset) {
        JS_ASSERT(INLINE_SHAPE_JUMP == offset);
    }

    int getInlineTypeJumpOffset() {
#if defined JS_CPU_X86 || defined JS_CPU_X64
        return INLINE_TYPE_JUMP;
#elif defined JS_CPU_ARM
        return POST_INST_OFFSET(inlineTypeJumpOffset);
#endif
    }

    void setInlineTypeJumpOffset(int offset) {
#if defined JS_CPU_X86 || defined JS_CPU_X64
        JS_ASSERT(INLINE_TYPE_JUMP == offset);
#elif defined JS_CPU_ARM
        inlineTypeJumpOffset = offset;
        JS_ASSERT(offset == inlineTypeJumpOffset);
#endif
     }

    int getInlineShapeOffset() {
        return inlineShapeOffset;
    }
    int getDslotsLoadOffset() {
        return dslotsLoadOffset;
    }
    int getStubShapeJumpOffset() {
        return POST_INST_OFFSET(stubShapeJumpOffset);
    }

  private:
    /* Offset from storeBack to beginning of 'mov dslots, addr' */
    int32 dslotsLoadOffset : 8;

    /* Offset from shapeGuard to end of shape comparison. */
    int32 inlineShapeOffset : 8;

    /* Offset from storeBack to end of value load. */
    int32 inlineValueLoadOffset : 8;

    /* 
     * Offset from lastStubStart to end of shape jump.
     * TODO: We can redefine the location of lastStubStart to be
     * after the jump -- at which point this is always 0.
     */
    int32 stubShapeJumpOffset : 8;

#if defined JS_CPU_X86 
    static const int32 INLINE_SHAPE_JUMP = 12;
    static const int32 INLINE_TYPE_JUMP = 12;
#elif defined JS_CPU_X64
    static const int32 INLINE_SHAPE_JUMP = 6;
    static const int32 INLINE_TYPE_JUMP = 19;
#elif defined JS_CPU_ARM
    /* Offset from the shape guard start to the shape guard jump. */
    static const int32 INLINE_SHAPE_JUMP = 12;

    /* Offset from the fast path to the type guard jump. */
    int32 inlineTypeJumpOffset : 8;
#endif
};

/* SetPropCompiler */
struct SetPropLabels : MacroAssemblerTypedefs {
    friend class ::ICOffsetInitializer;

    void setInlineValueStore(MacroAssembler &masm, Label fastPathRejoin, DataLabel32 inlineValueStore,
                             const ValueRemat &vr) {
        int offset = masm.differenceBetween(fastPathRejoin, inlineValueStore);
        setInlineValueStoreOffset(offset, vr.isConstant(), vr.isTypeKnown());
    }

    CodeLocationLabel getInlineValueStore(CodeLocationLabel fastPathRejoin, const ValueRemat &vr) {
        return fastPathRejoin.labelAtOffset(getInlineValueStoreOffset(vr.isConstant(),
                                                                      vr.isTypeKnown()));
    }

    void setInlineShapeData(MacroAssembler &masm, Label shapeGuard, DataLabel32 inlineShapeData) {
        int offset = masm.differenceBetween(shapeGuard, inlineShapeData);
        setInlineShapeDataOffset(offset);
    }

    CodeLocationDataLabel32 getInlineShapeData(CodeLocationLabel fastPathStart, int shapeGuardOffset) {
        return fastPathStart.dataLabel32AtOffset(shapeGuardOffset + getInlineShapeDataOffset());
    }

    void setDslotsLoad(MacroAssembler &masm, Label fastPathRejoin, Label beforeLoad,
                       const ValueRemat &rhs) {
        int offset = masm.differenceBetween(fastPathRejoin, beforeLoad);
        setDslotsLoadOffset(offset, rhs.isConstant(), rhs.isTypeKnown());
    }

    CodeLocationInstruction getDslotsLoad(CodeLocationLabel fastPathRejoin, const ValueRemat &vr) {
        return fastPathRejoin.instructionAtOffset(getDslotsLoadOffset(vr));
    }

    void setInlineShapeJump(MacroAssembler &masm, Label shapeGuard, Label afterJump) {
        setInlineShapeJumpOffset(masm.differenceBetween(shapeGuard, afterJump));
    }

    CodeLocationJump getInlineShapeJump(CodeLocationLabel shapeGuard) {
        return shapeGuard.jumpAtOffset(getInlineShapeJumpOffset());
    }

    void setStubShapeJump(MacroAssembler &masm, Label stubStart, Label afterShapeJump) {
        int offset = masm.differenceBetween(stubStart, afterShapeJump);
        setStubShapeJumpOffset(offset);
    }

    CodeLocationJump getStubShapeJump(CodeLocationLabel stubStart) {
        return stubStart.jumpAtOffset(getStubShapeJumpOffset());
    }

  private:

    /* Offset-based interface. */

    void setDslotsLoadOffset(int offset, bool isConstant, bool isTypeKnown) {
#if defined JS_HAS_IC_LABELS
        dslotsLoadOffset = offset;
        JS_ASSERT(offset == dslotsLoadOffset);
#elif defined JS_CPU_X86
        JS_ASSERT_IF(isConstant, offset == INLINE_DSLOTS_BEFORE_CONSTANT);
        JS_ASSERT_IF(isTypeKnown && !isConstant, offset == INLINE_DSLOTS_BEFORE_KTYPE);
        JS_ASSERT_IF(!isTypeKnown, offset == INLINE_DSLOTS_BEFORE_DYNAMIC);
#else
# error
#endif
    }

    int getDslotsLoadOffset(const ValueRemat &vr) {
#if defined JS_CPU_X86
        if (vr.isConstant())
            return INLINE_DSLOTS_BEFORE_CONSTANT;
        if (vr.isTypeKnown())
            return INLINE_DSLOTS_BEFORE_KTYPE;
        return INLINE_DSLOTS_BEFORE_DYNAMIC;
#else
        (void) vr;
        return dslotsLoadOffset;
#endif
    }

    void setInlineShapeDataOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        inlineShapeDataOffset = offset;
#endif
        JS_ASSERT(offset == inlineShapeDataOffset);
    }

    void setStubShapeJumpOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        stubShapeJumpOffset = offset;
#endif
        JS_ASSERT(offset == stubShapeJumpOffset);
    }

    void setInlineValueStoreOffset(int offset, bool isConstant, bool isTypeKnown) {
#ifdef JS_HAS_IC_LABELS
        inlineValueStoreOffset = offset;
        JS_ASSERT(offset == inlineValueStoreOffset);
#elif defined JS_CPU_X86
        JS_ASSERT_IF(isConstant, offset == INLINE_VALUE_STORE_CONSTANT);
        JS_ASSERT_IF(isTypeKnown && !isConstant, offset == INLINE_VALUE_STORE_KTYPE);
        JS_ASSERT_IF(!isTypeKnown && !isConstant, offset == INLINE_VALUE_STORE_DYNAMIC);
#endif
    }

    void setInlineShapeJumpOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        inlineShapeJumpOffset = offset;
#endif
        JS_ASSERT(offset == inlineShapeJumpOffset);
    }

    int getInlineShapeJumpOffset() {
        return POST_INST_OFFSET(inlineShapeJumpOffset);
    }

    int getInlineShapeDataOffset() {
        return inlineShapeDataOffset;
    }

    int getStubShapeJumpOffset() {
        return POST_INST_OFFSET(stubShapeJumpOffset);
    }

    int getInlineValueStoreOffset(bool isConstant, bool isTypeKnown) {
#ifdef JS_HAS_IC_LABELS
        return inlineValueStoreOffset;
#elif defined JS_CPU_X86
        if (isConstant)
            return INLINE_VALUE_STORE_CONSTANT;
        else if (isTypeKnown)
            return INLINE_VALUE_STORE_KTYPE;
        else
            return INLINE_VALUE_STORE_DYNAMIC;
#endif
    }

    /* Offset from storeBack to beginning of 'mov dslots, addr'. */
#if defined JS_CPU_X86
    static const int INLINE_DSLOTS_BEFORE_CONSTANT = -23;
    static const int INLINE_DSLOTS_BEFORE_KTYPE = -19;
    static const int INLINE_DSLOTS_BEFORE_DYNAMIC = -15;
#else
    int32 dslotsLoadOffset : 8;
#endif

    /* Offset from shapeGuard to end of shape comparison. */
    int32 inlineShapeDataOffset : 8;

    /* 
     * Offset from lastStubStart to end of shape jump.
     * TODO: We can redefine the location of lastStubStart to be
     * after the jump -- at which point this is always 0.
     */
    int32 stubShapeJumpOffset : 8;

#if defined JS_CPU_X86
    static const int INLINE_VALUE_STORE_CONSTANT = -20;
    static const int INLINE_VALUE_STORE_KTYPE = -16;
    static const int INLINE_VALUE_STORE_DYNAMIC = -12;
#else
    int32 inlineValueStoreOffset : 8;
#endif

    /* Offset from shapeGuard to the end of the shape jump. */
    int32 inlineShapeJumpOffset : 8;
};

/* BindNameCompiler */
struct BindNameLabels : MacroAssemblerTypedefs {
    friend class ::ICOffsetInitializer;

    void setInlineJumpOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        inlineJumpOffset = offset;
#endif
        JS_ASSERT(offset == inlineJumpOffset);
    }

    void setInlineJump(MacroAssembler &masm, Label shapeGuard, Jump inlineJump) {
        int offset = masm.differenceBetween(shapeGuard, inlineJump);
        setInlineJumpOffset(offset);
    }

    CodeLocationJump getInlineJump(CodeLocationLabel fastPathStart) {
        return fastPathStart.jumpAtOffset(getInlineJumpOffset());
    }

    int getInlineJumpOffset() {
        return inlineJumpOffset;
    }

    void setStubJumpOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        stubJumpOffset = offset;
#endif
        JS_ASSERT(offset == stubJumpOffset);
    }

    void setStubJump(MacroAssembler &masm, Label stubStart, Jump stubJump) {
        int offset = masm.differenceBetween(stubStart, stubJump);
        setStubJumpOffset(offset);
    }

    CodeLocationJump getStubJump(CodeLocationLabel lastStubStart) {
        return lastStubStart.jumpAtOffset(getStubJumpOffset());
    }

    int getStubJumpOffset() {
        return stubJumpOffset;
    }

  private:
    /* Offset from shapeGuard to end of shape jump. */
    int32 inlineJumpOffset : 8;

    /* Offset from lastStubStart to end of the shape jump. */
    int32 stubJumpOffset : 8;
};

/* ScopeNameCompiler */
struct ScopeNameLabels : MacroAssemblerTypedefs {
    friend class ::ICOffsetInitializer;

    void setInlineJumpOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        inlineJumpOffset = offset;
#endif
        JS_ASSERT(offset == inlineJumpOffset);
    }

    void setInlineJump(MacroAssembler &masm, Label fastPathStart, Jump inlineJump) {
        int offset = masm.differenceBetween(fastPathStart, inlineJump);
        setInlineJumpOffset(offset);
    }

    CodeLocationJump getInlineJump(CodeLocationLabel fastPathStart) {
        return fastPathStart.jumpAtOffset(getInlineJumpOffset());
    }

    int getInlineJumpOffset() {
        return inlineJumpOffset;
    }

    void setStubJumpOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        stubJumpOffset = offset;
#endif
        JS_ASSERT(offset == stubJumpOffset);
    }

    void setStubJump(MacroAssembler &masm, Label stubStart, Jump stubJump) {
        int offset = masm.differenceBetween(stubStart, stubJump);
        setStubJumpOffset(offset);
    }

    CodeLocationJump getStubJump(CodeLocationLabel lastStubStart) {
        return lastStubStart.jumpAtOffset(getStubJumpOffset());
    }

    int getStubJumpOffset() {
        return stubJumpOffset;
    }

  private:
    /* Offset from fastPathStart to end of shape jump. */
    int32 inlineJumpOffset : 8;

    /* Offset from lastStubStart to end of the shape jump. */
    int32 stubJumpOffset : 8;
};

} /* namespace ic */
} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_ic_labels_h__ */
