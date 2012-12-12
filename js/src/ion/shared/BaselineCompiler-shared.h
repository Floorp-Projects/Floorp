/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_baselinecompiler_shared_h__
#define jsion_baselinecompiler_shared_h__

#include "jscntxt.h"
#include "ion/BaselineFrameInfo.h"
#include "ion/BaselineIC.h"
#include "ion/IonMacroAssembler.h"

namespace js {
namespace ion {

class BaselineCompilerShared
{
  protected:
    JSContext *cx;
    RootedScript script;
    jsbytecode *pc;
    MacroAssembler masm;
    bool ionCompileable_;

    FrameInfo frame;

    ICStubSpace stubSpace_;
    js::Vector<ICEntry, 16, SystemAllocPolicy> icEntries_;

    // Labels for the 'movWithPatch' for loading IC entry pointers in
    // the generated IC-calling code in the main jitcode.  These need
    // to be patched with the actual icEntry offsets after the BaselineScript
    // has been allocated.
    js::Vector<CodeOffsetLabel, 16, SystemAllocPolicy> icLoadLabels_;

    BaselineCompilerShared(JSContext *cx, JSScript *script);

    ICEntry *allocateICEntry(ICStub *stub) {
        if (!stub)
            return NULL;

        // Create the entry and add it to the vector.
        ICEntry entry((uint32_t) (pc - script->code));
        if (!icEntries_.append(entry))
            return NULL;
        ICEntry &vecEntry = icEntries_[icEntries_.length() - 1];

        // Set the first stub for the IC entry to the fallback stub
        vecEntry.setFirstStub(stub);

        // Return pointer to the IC entry
        return &vecEntry;
    }

    bool addICLoadLabel(CodeOffsetLabel offset) {
        return icLoadLabels_.append(offset);
    }

    JSFunction *function() const {
        return script->function();
    }
};

} // namespace ion
} // namespace js

#endif // jsion_baselinecompiler_shared_h__
