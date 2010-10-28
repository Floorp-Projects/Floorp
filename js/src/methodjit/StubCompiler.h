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
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
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

#if !defined(jsstub_compiler_h__) && defined(JS_METHODJIT)
#define jsstub_compiler_h__

#include "jscntxt.h"
#include "jstl.h"
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
        CrossJumpInScript(Jump from, jsbytecode *pc)
          : from(from), pc(pc)
        { }

        Jump from;
        jsbytecode *pc;
    };

    JSContext *cx;
    Compiler &cc;
    FrameState &frame;
    JSScript *script;

  public:
    Assembler masm;

  private:
    uint32 generation;
    uint32 lastGeneration;

    /* :TODO: oom check */
    Vector<CrossPatch, 64, SystemAllocPolicy> exits;
    Vector<CrossPatch, 64, SystemAllocPolicy> joins;
    Vector<CrossJumpInScript, 64, SystemAllocPolicy> scriptJoins;
    Vector<Jump, 8, SystemAllocPolicy> jumpList;

  public:
    StubCompiler(JSContext *cx, mjit::Compiler &cc, FrameState &frame, JSScript *script);

    bool init(uint32 nargs);

    size_t size() {
        return masm.size();
    }

    uint8 *buffer() {
        return masm.buffer();
    }

    Call vpInc(JSOp op, uint32 depth);

#define STUB_CALL_TYPE(type)                                        \
    Call call(type stub) {                                          \
        return stubCall(JS_FUNC_TO_DATA_PTR(void *, stub));         \
    }                                                               \
    Call call(type stub, uint32 slots) {                            \
        return stubCall(JS_FUNC_TO_DATA_PTR(void *, stub), slots);  \
    }

    STUB_CALL_TYPE(JSObjStub);
    STUB_CALL_TYPE(VoidStub);
    STUB_CALL_TYPE(VoidStubUInt32);
    STUB_CALL_TYPE(VoidPtrStubUInt32);
    STUB_CALL_TYPE(VoidPtrStub);
    STUB_CALL_TYPE(BoolStub);
    STUB_CALL_TYPE(VoidStubAtom);
    STUB_CALL_TYPE(VoidStubPC);
#ifdef JS_POLYIC
    STUB_CALL_TYPE(VoidStubPIC);
#endif
#ifdef JS_MONOIC
    STUB_CALL_TYPE(VoidStubMIC);
    STUB_CALL_TYPE(VoidPtrStubMIC);
    STUB_CALL_TYPE(VoidStubCallIC);
    STUB_CALL_TYPE(VoidPtrStubCallIC);
    STUB_CALL_TYPE(BoolStubEqualityIC);
    STUB_CALL_TYPE(VoidPtrStubTraceIC);
#endif

#undef STUB_CALL_TYPE

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
    void leaveWithDepth(uint32 depth);

    /*
     * Rejoins slow-path code back to the fast-path. The invalidation param
     * specifies how many stack slots below sp must not be reloaded from
     * registers.
     */
    void rejoin(Changes changes);
    void linkRejoin(Jump j);

    /* Finish all native code patching. */
    void fixCrossJumps(uint8 *ncode, size_t offset, size_t total);
    void jumpInScript(Jump j, jsbytecode *target);
    void crossJump(Jump j, Label l);

  private:
    Call stubCall(void *ptr);
    Call stubCall(void *ptr, uint32 slots);
};

} /* namepsace mjit */
} /* namespace js */

#endif /* jsstub_compiler_h__ */

