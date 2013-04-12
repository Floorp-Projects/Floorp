/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Retroactive continuity ("retcon") refers to the retroactive modification
 * or reinterpretation of established facts.
 */

#if !defined jsjaeger_retcon_h__ && defined JS_METHODJIT
#define jsjaeger_retcon_h__

#include "jscntxt.h"
#include "jsscript.h"
#include "MethodJIT.h"
#include "Compiler.h"

namespace js {
namespace mjit {

/*
 * This class is responsible for sanely destroying a JITed script while frames
 * for it are still on the stack, removing all references in the world to it
 * and patching up those existing frames to go into the interpreter. If you
 * ever change the code associated with a JSScript, or otherwise would cause
 * existing JITed code to be incorrect, you /must/ use this to invalidate the
 * JITed code, fixing up the stack in the process.
 */
class Recompiler {
public:

    // Clear all uses of compiled code for script on the stack. This must be
    // followed by destroying all JIT code for the script.
    static void
    clearStackReferences(FreeOp *fop, JSScript *script);

    static void
    expandInlineFrames(JS::Zone *zone, StackFrame *fp, mjit::CallSite *inlined,
                       StackFrame *next, VMFrame *f);

    static void patchFrame(JSRuntime *rt, VMFrame *f, JSScript *script);

private:

    static void patchCall(JITChunk *chunk, StackFrame *fp, void **location);
    static void patchNative(JSRuntime *rt, JITChunk *chunk, StackFrame *fp,
                            jsbytecode *pc, RejoinState rejoin);

    static StackFrame *
    expandInlineFrameChain(StackFrame *outer, InlineFrame *inner);
};

} /* namespace mjit */
} /* namespace js */

#endif

