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
#include "methodjit/MethodJIT.h"
#include "CodeGenIncludes.h"

namespace js {
namespace mjit {
namespace ic {

struct MICInfo {
#ifdef JS_CPU_X86
    static const uint32 GET_DATA_OFFSET = 6;
    static const uint32 GET_TYPE_OFFSET = 12;

    static const uint32 SET_TYPE_OFFSET = 6;
    static const uint32 SET_DATA_CONST_TYPE_OFFSET = 16;
    static const uint32 SET_DATA_TYPE_OFFSET = 12;
#elif JS_CPU_X64 || JS_CPU_ARM
    /* X64: No constants used, thanks to patchValueOffset. */
    /* ARM: No constants used as mic.load always points to an LDR that loads the offset. */
#endif

    enum Kind
#ifdef _MSC_VER
    : uint8_t
#endif
    {
        GET,
        SET
    };

    /* Used by multiple MICs. */
    JSC::CodeLocationLabel entry;
    JSC::CodeLocationLabel stubEntry;

    /* TODO: use a union-like structure for the below. */

    /* Used by GET/SET. */
    JSC::CodeLocationLabel load;
    JSC::CodeLocationDataLabel32 shape;
    JSC::CodeLocationCall stubCall;
#if defined JS_PUNBOX64
    uint32 patchValueOffset;
#endif

    /* Used by all MICs. */
    Kind kind : 3;
    union {
        /* Used by GET/SET. */
        struct {
            bool touched : 1;
            bool typeConst : 1;
            bool dataConst : 1;
        } name;
    } u;
};

struct TraceICInfo {
    TraceICInfo() {}

    JSC::CodeLocationLabel stubEntry;
    JSC::CodeLocationLabel jumpTarget;
    JSC::CodeLocationJump traceHint;
    JSC::CodeLocationJump slowTraceHint;
#ifdef DEBUG
    jsbytecode *jumpTargetPC;
#endif

    bool hasSlowTraceHint : 1;
};

static const uint16 BAD_TRACEIC_INDEX = (uint16_t)-1;

void JS_FASTCALL GetGlobalName(VMFrame &f, ic::MICInfo *ic);
void JS_FASTCALL SetGlobalName(VMFrame &f, ic::MICInfo *ic);

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
    Assembler::Condition cond : 6;
};

JSBool JS_FASTCALL Equality(VMFrame &f, ic::EqualityICInfo *ic);

/* See MonoIC.cpp, CallCompiler for more information on call ICs. */
struct CallICInfo {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    enum PoolIndex {
        Pool_ScriptStub,
        Pool_ClosureStub,
        Pool_NativeStub,
        Total_Pools
    };

    JSC::ExecutablePool *pools[Total_Pools];

    /* Used for rooting and reification. */
    JSObject *fastGuardedObject;
    JSObject *fastGuardedNative;

    /* PC at the call site. */
    jsbytecode *pc;

    uint32 argc : 16;
    uint32 frameDepth : 16;

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

    /* Offset for deep-fun check to rejoin at. */
    uint32 hotPathOffset   : 16;

    /* Join point for all slow call paths. */
    uint32 slowJoinOffset  : 16;

    RegisterID funObjReg : 5;
    RegisterID funPtrReg : 5;
    bool hit : 1;
    bool hasJsFunCheck : 1;

    inline void reset() {
        fastGuardedObject = NULL;
        fastGuardedNative = NULL;
        hit = false;
        hasJsFunCheck = false;
        pools[0] = pools[1] = pools[2] = NULL;
    }

    inline void releasePools() {
        releasePool(Pool_ScriptStub);
        releasePool(Pool_ClosureStub);
        releasePool(Pool_NativeStub);
    }

    inline void releasePool(PoolIndex index) {
        if (pools[index]) {
            pools[index]->release();
            pools[index] = NULL;
        }
    }
};

void * JS_FASTCALL New(VMFrame &f, ic::CallICInfo *ic);
void * JS_FASTCALL Call(VMFrame &f, ic::CallICInfo *ic);
void JS_FASTCALL NativeNew(VMFrame &f, ic::CallICInfo *ic);
void JS_FASTCALL NativeCall(VMFrame &f, ic::CallICInfo *ic);

void PurgeMICs(JSContext *cx, JSScript *script);
void SweepCallICs(JSScript *script);

} /* namespace ic */
} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_mono_ic_h__ */

