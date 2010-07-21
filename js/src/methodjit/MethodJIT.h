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

#if !defined jsjaeger_h__ && defined JS_METHODJIT
#define jsjaeger_h__

#include "jscntxt.h"

#include "assembler/assembler/MacroAssemblerCodeRef.h"

#if !defined JS_CPU_X64 && \
    !defined JS_CPU_X86 && \
    !defined JS_CPU_ARM
# error "Oh no, you should define a platform so this compiles."
#endif

#if !defined(JS_32BIT) && !defined(JS_64BIT)
# error "This processor is UNKNOWN."
#endif

namespace js {

struct VMFrame
{
#if defined(JS_CPU_ARM)
    JSC::ReturnAddressPtr   veneerReturn;
#endif

    /* This must be the first entry on CPUs which push return addresses. */
    void *scriptedReturn;

#if defined(JS_CPU_X86)
    uintptr_t               padding;
#endif

    union Arguments {
        struct {
            void *ptr;
            void *ptr2;
        } x;
    } u;

    VMFrame      *previous;
    JSFrameRegs  *oldRegs;
    JSFrameRegs  regs;
    JSStackFrame *fp;
    JSContext    *cx;
    uintptr_t    inlineCallCount;

#if defined(JS_CPU_X86)
    void *savedEBX;
    void *savedEDI;
    void *savedESI;
    void *savedEBP;
    void *savedEIP;

#ifdef JS_NO_FASTCALL
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 3;
    }
#else
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
#endif
#elif defined(JS_CPU_X64)
    void *savedRBX;
#ifdef _MSC_VER
    void *savedRSI;
    void *savedRDI;
#endif
    void *savedR15;
    void *savedR14;
    void *savedR13;
    void *savedR12;
    void *savedRBP;
    void *savedRIP;

#ifdef _MSC_VER
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 5;
    }
#else
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
#endif

#elif defined(JS_CPU_ARM)
    void *savedR4;
    void *savedR5;
    void *savedR6;
    void *savedR7;
    void *savedR8;
    void *savedR9;
    void *savedR10;
    void *savedR11;
    void *savedLR;

    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(&this->veneerReturn);
    }
#else
# error "The VMFrame layout isn't defined for your processor architecture!"
#endif

    JSRuntime *runtime() { return cx->runtime; }
};

#ifdef JS_CPU_ARM
extern "C" void JaegerStubVeneer(void);
#endif

typedef void (JS_FASTCALL *VoidStub)(VMFrame &);
typedef void (JS_FASTCALL *VoidVpStub)(VMFrame &, Value *);
typedef void (JS_FASTCALL *VoidStubUInt32)(VMFrame &, uint32);
typedef void (JS_FASTCALL *VoidStubInt32)(VMFrame &, int32);
typedef JSBool (JS_FASTCALL *BoolStub)(VMFrame &);
typedef void * (JS_FASTCALL *VoidPtrStub)(VMFrame &);
typedef void * (JS_FASTCALL *VoidPtrStubPC)(VMFrame &, jsbytecode *);
typedef void * (JS_FASTCALL *VoidPtrStubUInt32)(VMFrame &, uint32);
typedef JSObject * (JS_FASTCALL *JSObjStub)(VMFrame &);
typedef JSObject * (JS_FASTCALL *JSObjStubUInt32)(VMFrame &, uint32);
typedef JSObject * (JS_FASTCALL *JSObjStubFun)(VMFrame &, JSFunction *);
typedef JSObject * (JS_FASTCALL *JSObjStubJSObj)(VMFrame &, JSObject *);
typedef void (JS_FASTCALL *VoidStubAtom)(VMFrame &, JSAtom *);
typedef JSString * (JS_FASTCALL *JSStrStub)(VMFrame &);
typedef JSString * (JS_FASTCALL *JSStrStubUInt32)(VMFrame &, uint32);
typedef void (JS_FASTCALL *VoidStubJSObj)(VMFrame &, JSObject *);
typedef void (JS_FASTCALL *VoidStubPC)(VMFrame &, jsbytecode *);

#define JS_UNJITTABLE_METHOD (reinterpret_cast<void*>(1))

namespace mjit {

JSBool
JaegerShot(JSContext *cx);

enum CompileStatus
{
    Compile_Okay,
    Compile_Abort,
    Compile_Error
};

void JS_FASTCALL
ProfileStubCall(VMFrame &f);

CompileStatus
TryCompile(JSContext *cx, JSScript *script, JSFunction *fun, JSObject *scopeChain);

void
ReleaseScriptCode(JSContext *cx, JSScript *script);

static inline CompileStatus
CanMethodJIT(JSContext *cx, JSScript *script, JSFunction *fun, JSObject *scopeChain)
{
    if (!(cx->options & JSOPTION_METHODJIT) || script->ncode == JS_UNJITTABLE_METHOD)
        return Compile_Abort;
    if (script->ncode == NULL)
        return TryCompile(cx, script, fun, scopeChain);
    return Compile_Okay;
}

void
PurgeShapeDependencies(JSContext *cx);

union CallSite
{
    struct {
        uint32 codeOffset;
        uint32 pcOffset;
        uint32 id;
    }          c;
    uint32     nCallSites;
};

} /* namespace mjit */

} /* namespace js */

#ifdef _MSC_VER
extern "C" void *JaegerThrowpoline(js::VMFrame *vmFrame);
#else
extern "C" void JaegerThrowpoline();
#endif
extern "C" void JaegerFromTracer();

#endif /* jsjaeger_h__ */

