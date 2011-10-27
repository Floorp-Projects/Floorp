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

#if !defined jsjaeger_mono_ic_h__ && defined JS_METHODJIT && defined JS_MONOIC
#define jsjaeger_mono_ic_h__

#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/CodeLocation.h"
#include "assembler/moco/MocoStubs.h"
#include "methodjit/MethodJIT.h"
#include "CodeGenIncludes.h"
#include "methodjit/ICRepatcher.h"

namespace js {
namespace mjit {

class FrameSize
{
    uint32 frameDepth_ : 16;
    uint32 argc_;
  public:
    void initStatic(uint32 frameDepth, uint32 argc) {
        JS_ASSERT(frameDepth > 0);
        frameDepth_ = frameDepth;
        argc_ = argc;
    }

    void initDynamic() {
        frameDepth_ = 0;
        argc_ = -1;  /* quiet gcc */
    }

    bool isStatic() const {
        return frameDepth_ > 0;
    }

    bool isDynamic() const {
        return frameDepth_ == 0;
    }

    uint32 staticLocalSlots() const {
        JS_ASSERT(isStatic());
        return frameDepth_;
    }

    uint32 staticArgc() const {
        JS_ASSERT(isStatic());
        return argc_;
    }

    uint32 getArgc(VMFrame &f) const {
        return isStatic() ? staticArgc() : f.u.call.dynamicArgc;
    }

    bool lowered(jsbytecode *pc) const {
        return isDynamic() || staticArgc() != GET_ARGC(pc);
    }

    RejoinState rejoinState(jsbytecode *pc, bool native) {
        if (isStatic()) {
            if (staticArgc() == GET_ARGC(pc))
                return native ? REJOIN_NATIVE : REJOIN_CALL_PROLOGUE;
            JS_ASSERT(staticArgc() == GET_ARGC(pc) - 1);
            return native ? REJOIN_NATIVE_LOWERED : REJOIN_CALL_PROLOGUE_LOWERED_CALL;
        }
        return native ? REJOIN_NATIVE_LOWERED : REJOIN_CALL_PROLOGUE_LOWERED_APPLY;
    }

    bool lowered(jsbytecode *pc) {
        return !isStatic() || staticArgc() != GET_ARGC(pc);
    }
};

namespace ic {

struct GlobalNameIC
{
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    JSC::CodeLocationLabel  fastPathStart;
    JSC::CodeLocationCall   slowPathCall;

    /*
     * - ARM and x64 always emit exactly one instruction which needs to be
     *   patched. On ARM, the label points to the patched instruction, whilst
     *   on x64 it points to the instruction after it.
     * - For x86, the label "load" points to the start of the load/store
     *   sequence, which may consist of one or two "mov" instructions. Because
     *   of this, x86 is the only platform which requires non-trivial patching
     *   code.
     */
    int32 loadStoreOffset   : 15;
    int32 shapeOffset       : 15;
    bool usePropertyCache   : 1;
};

struct GetGlobalNameIC : public GlobalNameIC
{
};

struct SetGlobalNameIC : public GlobalNameIC
{
    JSC::CodeLocationLabel  slowPathStart;

    /* Dynamically generted stub for method-write checks. */
    JSC::JITCode            extraStub;

    /* SET only, if we had to generate an out-of-line path. */
    int32 inlineShapeJump : 10;   /* Offset into inline path for shape jump. */
    int32 extraShapeGuard : 6;    /* Offset into stub for shape guard. */
    bool objConst : 1;          /* True if the object is constant. */
    RegisterID objReg   : 5;    /* Register for object, if objConst is false. */
    RegisterID shapeReg : 5;    /* Register for shape; volatile. */
    bool hasExtraStub : 1;      /* Extra stub is preset. */

    int32 fastRejoinOffset : 16;  /* Offset from fastPathStart to rejoin. */
    int32 extraStoreOffset : 16;  /* Offset into store code. */

    /* SET only. */
    ValueRemat vr;              /* RHS value. */

    void patchInlineShapeGuard(Repatcher &repatcher, const Shape *shape);
    void patchExtraShapeGuard(Repatcher &repatcher, const Shape *shape);
};

void JS_FASTCALL GetGlobalName(VMFrame &f, ic::GetGlobalNameIC *ic);
void JS_FASTCALL SetGlobalName(VMFrame &f, ic::SetGlobalNameIC *ic);

struct EqualityICInfo {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    JSC::CodeLocationLabel stubEntry;
    JSC::CodeLocationCall stubCall;
    BoolStub stub;
    JSC::CodeLocationLabel target;
    JSC::CodeLocationLabel fallThrough;
    JSC::CodeLocationJump jumpToStub;

    ValueRemat lvr, rvr;

    bool generated : 1;
    JSC::MacroAssembler::RegisterID tempReg : 5;
    Assembler::Condition cond;
};

JSBool JS_FASTCALL Equality(VMFrame &f, ic::EqualityICInfo *ic);

/* See MonoIC.cpp, CallCompiler for more information on call ICs. */
struct CallICInfo {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    /* Linked list entry for all ICs guarding on the same JIT entry point in fastGuardedObject. */
    JSCList links;

    enum PoolIndex {
        Pool_ScriptStub,
        Pool_ClosureStub,
        Total_Pools
    };

    JSC::ExecutablePool *pools[Total_Pools];

    /* Used for rooting and reification. */
    JSObject *fastGuardedObject;
    JSObject *fastGuardedNative;

    /* Return site for scripted calls at this site, with PC and inlining state. */
    CallSite *call;

    FrameSize frameSize;

    /* Function object identity guard. */
    JSC::CodeLocationDataLabelPtr funGuard;

    /* Starting point for all slow call paths. */
    JSC::CodeLocationLabel slowPathStart;

    /* Inline to OOL jump, redirected by stubs. */
    JSC::CodeLocationJump funJump;

    /* Offset to inline scripted call, from funGuard. */
    uint32 hotJumpOffset   : 16;
    uint32 joinPointOffset : 16;

    /* Out of line slow call. */
    uint32 oolCallOffset   : 16;

    /* Jump to patch for out-of-line scripted calls. */
    uint32 oolJumpOffset   : 16;

    /* Label for out-of-line call to IC function. */
    uint32 icCallOffset    : 16;

    /* Offset for deep-fun check to rejoin at. */
    uint32 hotPathOffset   : 16;

    /* Join point for all slow call paths. */
    uint32 slowJoinOffset  : 16;

    RegisterID funObjReg : 5;
    bool hit : 1;
    bool hasJsFunCheck : 1;
    bool typeMonitored : 1;

    inline void reset() {
        fastGuardedObject = NULL;
        fastGuardedNative = NULL;
        hit = false;
        hasJsFunCheck = false;
        PodArrayZero(pools);
    }

    inline void releasePools() {
        releasePool(Pool_ScriptStub);
        releasePool(Pool_ClosureStub);
    }

    inline void releasePool(PoolIndex index) {
        if (pools[index]) {
            pools[index]->release();
            pools[index] = NULL;
        }
    }

    inline void purgeGuardedObject() {
        JS_ASSERT(fastGuardedObject);
        releasePool(CallICInfo::Pool_ClosureStub);
        hasJsFunCheck = false;
        fastGuardedObject = NULL;
        JS_REMOVE_LINK(&links);
    }
};

void * JS_FASTCALL New(VMFrame &f, ic::CallICInfo *ic);
void * JS_FASTCALL Call(VMFrame &f, ic::CallICInfo *ic);
void * JS_FASTCALL NativeNew(VMFrame &f, ic::CallICInfo *ic);
void * JS_FASTCALL NativeCall(VMFrame &f, ic::CallICInfo *ic);
JSBool JS_FASTCALL SplatApplyArgs(VMFrame &f);

void GenerateArgumentCheckStub(VMFrame &f);

} /* namespace ic */
} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_mono_ic_h__ */

