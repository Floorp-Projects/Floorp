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

#if defined JS_CPU_X64 || defined JS_CPU_ARM
/*
 * On x64 and ARM, we create offsets dynamically instead of using hardcoded
 * offsets into the instruction stream. This is done on x64 because of
 * variable-width instruction encoding when using the extended register set and
 * it is done on ARM for expediency of implementation.
 */
# define JS_HAS_IC_LABELS
#endif

/* GetPropCompiler */
struct GetPropLabels {
    friend class ::ICOffsetInitializer;

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
        stubShapeJump = offset;
#endif
        JS_ASSERT(offset == stubShapeJump);
    }

#ifdef JS_NUNBOX32
    void setTypeLoad(int offset) {
# ifdef JS_HAS_IC_LABELS
        inlineTypeLoad = offset;
# endif
        JS_ASSERT(offset == inlineTypeLoad);
    }
    void setDataLoad(int offset) {
# ifdef JS_HAS_IC_LABELS
        inlineDataLoad = offset;
# endif
        JS_ASSERT(offset == inlineDataLoad);
    }
    JSC::CodeLocationDataLabel32 getTypeLoad(JSC::CodeLocationLabel start) {
        return start.dataLabel32AtOffset(inlineTypeLoad);
    }
    JSC::CodeLocationDataLabel32 getDataLoad(JSC::CodeLocationLabel start) {
        return start.dataLabel32AtOffset(inlineDataLoad);
    }
#elif JS_PUNBOX64
    void setValueLoad(int offset) {
# ifdef JS_HAS_IC_LABELS
        inlineValueLoad = offset;
# endif
        JS_ASSERT(offset == inlineValueLoad);
    }
    JSC::CodeLocationDataLabel32 getValueLoad(JSC::CodeLocationLabel start) {
        return start.dataLabel32AtOffset(inlineValueLoad);
    }
#endif

    int getInlineShapeOffset() {
        return inlineShapeOffset;
    }
    int getDslotsLoadOffset() {
        return dslotsLoadOffset;
    }
    int getStubShapeJump() {
        return stubShapeJump;
    }

  private:
    /* Offset from storeBack to beginning of 'mov dslots, addr' */
    int32 dslotsLoadOffset : 8;

    /* Offset from shapeGuard to end of shape comparison. */
    int32 inlineShapeOffset : 8;

    /* Offset from storeBack to end of value load. */
#ifdef JS_NUNBOX32
    int32 inlineTypeLoad    : 8;
    int32 inlineDataLoad    : 8;
#elif JS_PUNBOX64
    int32 inlineValueLoad   : 8;
#endif

    /* 
     * Offset from lastStubStart to end of shape jump.
     * TODO: We can redefine the location of lastStubStart to be
     * after the jump -- at which point this is always 0.
     */
    int32 stubShapeJump : 8;
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
    /* Offset from storeBack to beginning of 'mov dslots, addr' */
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

