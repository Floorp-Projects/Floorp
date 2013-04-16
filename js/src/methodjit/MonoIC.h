/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    uint32_t frameDepth_ : 16;
    uint32_t argc_;
  public:
    void initStatic(uint32_t frameDepth, uint32_t argc) {
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

    uint32_t staticLocalSlots() const {
        JS_ASSERT(isStatic());
        return frameDepth_;
    }

    uint32_t staticArgc() const {
        JS_ASSERT(isStatic());
        return argc_;
    }

    uint32_t getArgc(VMFrame &f) const {
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
    int32_t loadStoreOffset   : 15;
    int32_t shapeOffset       : 15;
};

struct GetGlobalNameIC : public GlobalNameIC
{
};

struct SetGlobalNameIC : public GlobalNameIC
{
    JSC::CodeLocationLabel  slowPathStart;

    /* SET only, if we had to generate an out-of-line path. */
    int32_t inlineShapeJump : 10;   /* Offset into inline path for shape jump. */
    bool objConst : 1;          /* True if the object is constant. */
    RegisterID objReg   : 5;    /* Register for object, if objConst is false. */
    RegisterID shapeReg : 5;    /* Register for shape; volatile. */

    int32_t fastRejoinOffset : 16;  /* Offset from fastPathStart to rejoin. */

    /* SET only. */
    ValueRemat vr;              /* RHS value. */

    void patchInlineShapeGuard(Repatcher &repatcher, RawShape shape);
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

    /* Label to the function object identity guard. */
    JSC::CodeLocationLabel funGuardLabel;

    /* Function object identity guard. */
    JSC::CodeLocationDataLabelPtr funGuard;

    /* Starting point for all slow call paths. */
    JSC::CodeLocationLabel slowPathStart;

    /* Inline to OOL jump, redirected by stubs. */
    JSC::CodeLocationJump funJump;

    /*
     * Target of the above jump, remembered so that if we need to generate a
     * callsite clone stub we can redirect to the original funJump target.
     */
    JSC::CodeLocationLabel funJumpTarget;

    /*
     * If an Ion stub has been generated, its guard may be linked to another
     * stub. The guard location is stored in this label.
     */
    bool hasIonStub_;
    JSC::JITCode lastOolCode_;
    JSC::CodeLocationJump lastOolJump_;

    /* Offset to inline scripted call, from funGuard. */
    uint32_t hotJumpOffset   : 16;
    uint32_t joinPointOffset : 16;

    /* Out of line slow call. */
    uint32_t oolCallOffset   : 16;

    /* Jump/rejoin to patch for out-of-line scripted calls. */
    uint32_t oolJumpOffset   : 16;

    /* Label for out-of-line call to IC function. */
    uint32_t icCallOffset    : 16;

    /* Offset for deep-fun check to rejoin at. */
    uint32_t hotPathOffset   : 16;

    /* Join point for all slow call paths. */
    uint32_t slowJoinOffset  : 16;

    /* Join point for Ion calls. */
    uint32_t ionJoinOffset : 16;

    RegisterID funObjReg : 5;
    bool hit : 1;
    bool hasJsFunCheck : 1;
    bool typeMonitored : 1;

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

    bool hasJMStub() const {
        return !!pools[Pool_ScriptStub];
    }
    bool hasIonStub() const {
        return hasIonStub_;
    }
    bool hasStubOolJump() const {
        return hasIonStub();
    }
    JSC::CodeLocationLabel icCall() {
        return slowPathStart.labelAtOffset(icCallOffset);
    }
    JSC::CodeLocationJump oolJump() {
        return slowPathStart.jumpAtOffset(oolJumpOffset);
    }
    JSC::CodeLocationJump lastOolJump() {
        if (hasStubOolJump())
            return lastOolJump_;
        return oolJump();
    }
    JSC::JITCode lastOolCode() {
        JS_ASSERT(hasStubOolJump());
        return lastOolCode_;
    }
    void updateLastOolJump(JSC::CodeLocationJump jump, JSC::JITCode code) {
        lastOolJump_ = jump;
        lastOolCode_ = code;
    }
    JSC::CodeLocationLabel nativeRejoin() {
        return slowPathStart.labelAtOffset(slowJoinOffset);
    }
    JSC::CodeLocationLabel ionJoinPoint() {
        return funGuard.labelAtOffset(ionJoinOffset);
    }

    inline void reset(Repatcher &repatcher) {
        if (fastGuardedObject) {
            repatcher.repatch(funGuard, NULL);
            repatcher.relink(funJump, slowPathStart);
            purgeGuardedObject();
        }
        if (fastGuardedNative) {
            repatcher.relink(funJump, slowPathStart);
            fastGuardedNative = NULL;
        }
        if (pools[Pool_ScriptStub] || hasIonStub()) {
            repatcher.relink(oolJump(), icCall());
            releasePool(Pool_ScriptStub);
        }
        hit = false;
        hasIonStub_ = false;
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

