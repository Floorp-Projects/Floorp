/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonInstrumentatjit_h
#define jit_IonInstrumentatjit_h

namespace js {

class SPSProfiler;

namespace jit {

class MacroAssembler;

typedef SPSInstrumentation<MacroAssembler, Register> BaseInstrumentation;

class IonInstrumentation : public BaseInstrumentation
{
    jsbytecode **trackedPc_;

  public:
    IonInstrumentation(SPSProfiler *profiler, jsbytecode **pc)
      : BaseInstrumentation(profiler),
        trackedPc_(pc)
    {
        JS_ASSERT(pc != nullptr);
    }

    void leave(MacroAssembler &masm, Register reg) {
        BaseInstrumentation::leave(*trackedPc_, masm, reg);
    }
};

} // namespace jit
} // namespace js

#endif /* jit_IonInstrumentatjit_h */
