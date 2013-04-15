/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsstub_compiler_h__) && defined(JS_METHODJIT)
#define jsstub_compiler_h__

#include "jscntxt.h"
#include "MethodJIT.h"
#include "methodjit/FrameState.h"
#include "CodeGenIncludes.h"

namespace js {
namespace mjit {

class Compiler;

class StubCompiler
{
    typedef JSC::MacroAssembler::Call Call;
    typedef JSC::MacroAssembler::Jump Jump;
    typedef JSC::MacroAssembler::Label Label;

    struct CrossPatch {
        CrossPatch(Jump from, Label to)
          : from(from), to(to)
        { }

        Jump from;
        Label to;
    };

    struct CrossJumpInScript {
        CrossJumpInScript(Jump from, jsbytecode *pc, uint32_t inlineIndex)
          : from(from), pc(pc), inlineIndex(inlineIndex)
        { }

        Jump from;
        jsbytecode *pc;
        uint32_t inlineIndex;
    };

    JSContext *cx;
    Compiler &cc;
    FrameState &frame;

  public:
    Assembler masm;

  private:
    uint32_t generation;
    uint32_t lastGeneration;

    Vector<CrossPatch, 64, mjit::CompilerAllocPolicy> exits;
    Vector<CrossPatch, 64, mjit::CompilerAllocPolicy> joins;
    Vector<CrossJumpInScript, 64, mjit::CompilerAllocPolicy> scriptJoins;
    Vector<Jump, 8, SystemAllocPolicy> jumpList;

  public:
    StubCompiler(JSContext *cx, mjit::Compiler &cc, FrameState &frame);

    size_t size() {
        return masm.size();
    }

    uint8_t *buffer() {
        return masm.buffer();
    }

    /*
     * Force a frame sync and return a label before the syncing code.
     * A Jump may bind to the label with leaveExitDirect().
     */
    JSC::MacroAssembler::Label syncExit(Uses uses);

    /*
     * Sync the exit, and state that code will be immediately outputted
     * to the out-of-line buffer.
     */
    JSC::MacroAssembler::Label syncExitAndJump(Uses uses);

    /* Exits from the fast path into the slow path. */
    JSC::MacroAssembler::Label linkExit(Jump j, Uses uses);
    void linkExitForBranch(Jump j);
    void linkExitDirect(Jump j, Label L);

    void leave();
    void leaveWithDepth(uint32_t depth);

    /*
     * Rejoins slow-path code back to the fast-path. The invalidation param
     * specifies how many stack slots below sp must not be reloaded from
     * registers.
     */
    void rejoin(Changes changes);
    void linkRejoin(Jump j);

    /* Finish all native code patching. */
    void fixCrossJumps(uint8_t *ncode, size_t offset, size_t total);
    bool jumpInScript(Jump j, jsbytecode *target);
    unsigned crossJump(Jump j, Label l);

    Call emitStubCall(void *ptr, RejoinState rejoin, Uses uses);
    Call emitStubCall(void *ptr, RejoinState rejoin, Uses uses, int32_t slots);

    void patchJoin(unsigned i, bool script, Assembler::Address address, AnyRegisterID reg);
};

} /* namepsace mjit */
} /* namespace js */

#endif /* jsstub_compiler_h__ */

