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

#include "jscntxt.h"
#include "jstl.h"
#include "jsvector.h"
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/CodeLocation.h"
#include "methodjit/CodeGenIncludes.h"
#include "methodjit/MethodJIT.h"
#include "BaseAssembler.h"
#include "RematInfo.h"
#include "BaseCompiler.h"

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

# define JS_POLYIC_OFFSET_BITS 8

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
#if defined JS_CPU_X86 || defined JS_CPU_X64
        return fastPathStart.jumpAtOffset(getInlineTypeJumpOffset());
#elif defined JS_CPU_ARM
        /* Type check is after the testObject, so offset by one instruction to get the jump. */
        return fastPathStart.jumpAtOffset(getInlineTypeJumpOffset() - sizeof(ARMWord));
#endif
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
    
    void setStubShapeJump(int offset) {
#ifdef JS_HAS_IC_LABELS
        stubShapeJumpOffset = offset;
#endif
        JS_ASSERT(offset == stubShapeJumpOffset);
    }

    int getInlineShapeJumpOffset() {
#if defined JS_CPU_X86
        return INLINE_SHAPE_JUMP;
#elif defined JS_CPU_X64
        return getInlineShapeOffset() + INLINE_SHAPE_JUMP;
#elif defined JS_CPU_ARM
        return INLINE_SHAPE_JUMP - sizeof(ARMWord);
#endif
    }

    void setInlineShapeJumpOffset(int offset) {
        JS_ASSERT(INLINE_SHAPE_JUMP == offset);
    }

    int getInlineTypeJumpOffset() {
#if defined JS_CPU_X86 || defined JS_CPU_X64
        return INLINE_TYPE_JUMP;
#elif defined JS_CPU_ARM
        return inlineTypeJumpOffset;
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
#if defined JS_CPU_X86 || defined JS_CPU_X64
        return stubShapeJumpOffset;
#elif defined JS_CPU_ARM
        return stubShapeJumpOffset - sizeof(ARMWord);
#endif
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
struct SetPropLabels {
    friend class ::ICOffsetInitializer;

#ifdef JS_PUNBOX64
    void setDslotsLoadOffset(int offset) {
# ifdef JS_HAS_IC_LABELS
        dslotsLoadOffset = offset;
# endif
        JS_ASSERT(offset == dslotsLoadOffset);
    }
#endif

    int getDslotsLoadOffset() {
#ifdef JS_PUNBOX64
        return dslotsLoadOffset;
#else
        JS_NOT_REACHED("this is inlined");
        return 0;
#endif
    }

    void setInlineShapeOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        inlineShapeOffset = offset;
#endif
        JS_ASSERT(offset == inlineShapeOffset);
    }

    void setStubShapeJump(int offset) {
#ifdef JS_HAS_IC_LABELS
        stubShapeJump = offset;
#endif
        JS_ASSERT(offset == stubShapeJump);
    }

    int getInlineShapeOffset() {
        return inlineShapeOffset;
    }
    int getStubShapeJump() {
        return stubShapeJump;
    }

  private:
#ifdef JS_PUNBOX64
    /* Offset from storeBack to beginning of 'mov dslots, addr'. */
    int32 dslotsLoadOffset : 8;
#endif

    /* Offset from shapeGuard to end of shape comparison. */
    int32 inlineShapeOffset : 8;

    /* 
     * Offset from lastStubStart to end of shape jump.
     * TODO: We can redefine the location of lastStubStart to be
     * after the jump -- at which point this is always 0.
     */
    int32 stubShapeJump : 8;
};

/* BindNameCompiler */
struct BindNameLabels {
    friend class ::ICOffsetInitializer;

    void setInlineJumpOffset(int offset) {
#ifdef JS_HAS_IC_LABELS
        inlineJumpOffset = offset;
#endif
        JS_ASSERT(offset == inlineJumpOffset);
    }

    int getInlineJumpOffset() {
        return inlineJumpOffset;
    }

  private:
    /* Offset from shapeGuard to end of shape jump. */
    int32 inlineJumpOffset : 8;
};

} /* namespace ic */
} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_ic_labels_h__ */

