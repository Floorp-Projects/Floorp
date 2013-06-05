/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsanalyzeinlines_h___
#define jsanalyzeinlines_h___

#include "jsanalyze.h"

#include "jsopcodeinlines.h"

namespace js {
namespace analyze {

inline const SSAValue &
ScriptAnalysis::poppedValue(uint32_t offset, uint32_t which)
{
    JS_ASSERT(offset < script_->length);
    JS_ASSERT(which < GetUseCount(script_, offset) +
              (ExtendedUse(script_->code + offset) ? 1 : 0));
    return getCode(offset).poppedValues[which];
}

inline const SSAValue &
ScriptAnalysis::poppedValue(const jsbytecode *pc, uint32_t which)
{
    return poppedValue(pc - script_->code, which);
}

inline types::StackTypeSet *
ScriptAnalysis::pushedTypes(uint32_t offset, uint32_t which)
{
    JS_ASSERT(offset < script_->length);
    JS_ASSERT(which < GetDefCount(script_, offset) +
              (ExtendedDef(script_->code + offset) ? 1 : 0));
    types::StackTypeSet *array = getCode(offset).pushedTypes;
    JS_ASSERT(array);
    return array + which;
}

inline types::StackTypeSet *
ScriptAnalysis::pushedTypes(const jsbytecode *pc, uint32_t which)
{
    return pushedTypes(pc - script_->code, which);
}

inline types::StackTypeSet *
ScriptAnalysis::getValueTypes(const SSAValue &v)
{
    switch (v.kind()) {
      case SSAValue::PUSHED:
        return pushedTypes(v.pushedOffset(), v.pushedIndex());
      case SSAValue::VAR:
        JS_ASSERT(!slotEscapes(v.varSlot()));
        if (v.varInitial()) {
            if (v.varSlot() < LocalSlot(script_, 0))
                return types::TypeScript::SlotTypes(script_, v.varSlot());
            return undefinedTypeSet;
        } else {
            /*
             * Results of intermediate assignments have the same type as
             * the first type pushed by the assignment op. Note that this
             * may not be the exact same value as was pushed, due to
             * post-inc/dec ops.
             */
            return pushedTypes(v.varOffset(), 0);
        }
      case SSAValue::PHI:
        return &v.phiNode()->types;
      default:
        /* Cannot compute types for empty SSA values. */
        JS_NOT_REACHED("Bad SSA value");
        return NULL;
    }
}

inline types::StackTypeSet *
ScriptAnalysis::poppedTypes(uint32_t offset, uint32_t which)
{
    return getValueTypes(poppedValue(offset, which));
}

inline types::StackTypeSet *
ScriptAnalysis::poppedTypes(const jsbytecode *pc, uint32_t which)
{
    return getValueTypes(poppedValue(pc, which));
}

inline SSAUseChain *&
ScriptAnalysis::useChain(const SSAValue &v)
{
    JS_ASSERT(trackUseChain(v));
    if (v.kind() == SSAValue::PUSHED)
        return getCode(v.pushedOffset()).pushedUses[v.pushedIndex()];
    if (v.kind() == SSAValue::VAR)
        return getCode(v.varOffset()).pushedUses[GetDefCount(script_, v.varOffset())];
    return v.phiNode()->uses;
}

inline jsbytecode *
ScriptAnalysis::getCallPC(jsbytecode *pc)
{
    SSAUseChain *uses = useChain(SSAValue::PushedValue(pc - script_->code, 0));
    JS_ASSERT(uses && uses->popped);
    JS_ASSERT(js_CodeSpec[script_->code[uses->offset]].format & JOF_INVOKE);
    return script_->code + uses->offset;
}

inline types::StackTypeSet *
CrossScriptSSA::getValueTypes(const CrossSSAValue &cv)
{
    return getFrame(cv.frame).script->analysis()->getValueTypes(cv.v);
}

} /* namespace analyze */
} /* namespace js */

#endif // jsanalyzeinlines_h___
