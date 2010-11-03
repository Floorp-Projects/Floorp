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

#if !defined(JS_NUNBOX32) && !defined(JS_PUNBOX64)
# error "No boxing format selected."
#endif

namespace js {

struct VMFrame
{
    union Arguments {
        struct {
            void *ptr;
            void *ptr2;
            void *ptr3;
        } x;
    } u;

    VMFrame      *previous;
    void         *unused;
    JSFrameRegs  regs;
    JSContext    *cx;
    Value        *stackLimit;
    JSStackFrame *entryFp;

#if defined(JS_CPU_X86)
    void *savedEBX;
    void *savedEDI;
    void *savedESI;
    void *savedEBP;
    void *savedEIP;

# ifdef JS_NO_FASTCALL
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 3;
    }
# else
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
# endif
#elif defined(JS_CPU_X64)
    void *savedRBX;
# ifdef _WIN64
    void *savedRSI;
    void *savedRDI;
# endif
    void *savedR15;
    void *savedR14;
    void *savedR13;
    void *savedR12;
    void *savedRBP;
    void *savedRIP;

# ifdef _WIN64
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 5;
    }
# else
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
# endif

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
        return reinterpret_cast<void**>(this) - 1;
    }
#else
# error "The VMFrame layout isn't defined for your processor architecture!"
#endif

    JSRuntime *runtime() { return cx->runtime; }

    JSStackFrame *&fp() { return regs.fp; }
};

#ifdef JS_CPU_ARM
// WARNING: Do not call this function directly from C(++) code because it is not ABI-compliant.
extern "C" void JaegerStubVeneer(void);
#endif

namespace mjit {

/*
 * Trampolines to force returns from jit code.
 * See also TrampolineCompiler::generateForceReturn(Fast).
 */
struct Trampolines {
    typedef void (*TrampolinePtr)();

    TrampolinePtr       forceReturn;
    JSC::ExecutablePool *forceReturnPool;

#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
    TrampolinePtr       forceReturnFast;
    JSC::ExecutablePool *forceReturnFastPool;
#endif
};

/*
 * Method JIT compartment data. Currently, there is exactly one per
 * JS compartment. It would be safe for multiple JS compartments to
 * share a JaegerCompartment as long as only one thread can enter
 * the JaegerCompartment at a time.
 */
class JaegerCompartment {
    JSC::ExecutableAllocator *execAlloc;     // allocator for jit code
    Trampolines              trampolines;    // force-return trampolines
    VMFrame                  *activeFrame_;  // current active VMFrame

    void Finish();

  public:
    bool Initialize();

    ~JaegerCompartment() { Finish(); }

    JSC::ExecutablePool *poolForSize(size_t size) {
        return execAlloc->poolForSize(size);
    }

    VMFrame *activeFrame() {
        return activeFrame_;
    }

    void pushActiveFrame(VMFrame *f) {
        f->previous = activeFrame_;
        activeFrame_ = f;
    }

    void popActiveFrame() {
        JS_ASSERT(activeFrame_);
        activeFrame_ = activeFrame_->previous;
    }

    Trampolines::TrampolinePtr forceReturnTrampoline() const {
        return trampolines.forceReturn;
    }

#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
    Trampolines::TrampolinePtr forceReturnFastTrampoline() const {
        return trampolines.forceReturnFast;
    }
#endif
};

/*
 * Allocation policy for compiler jstl objects. The goal is to free the
 * compiler from having to check and propagate OOM after every time we
 * append to a vector. We do this by reporting OOM to the engine and
 * setting a flag on the compiler when OOM occurs. The compiler is required
 * to check for OOM only before trying to use the contents of the list.
 */
class CompilerAllocPolicy : public ContextAllocPolicy
{
    bool *oomFlag;

    void *checkAlloc(void *p) {
        if (!p)
            *oomFlag = true;
        return p;
    }

  public:
    CompilerAllocPolicy(JSContext *cx, bool *oomFlag)
    : ContextAllocPolicy(cx), oomFlag(oomFlag) {}
    CompilerAllocPolicy(JSContext *cx, Compiler &compiler);

    void *malloc(size_t bytes) { return checkAlloc(ContextAllocPolicy::malloc(bytes)); }
    void *realloc(void *p, size_t bytes) {
        return checkAlloc(ContextAllocPolicy::realloc(p, bytes));
    }
};

namespace ic {
# if defined JS_POLYIC
    struct PICInfo;
    struct GetElementIC;
    struct SetElementIC;
# endif
# if defined JS_MONOIC
    struct MICInfo;
    struct EqualityICInfo;
    struct TraceICInfo;
    struct CallICInfo;
# endif
}
}

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
typedef void (JS_FASTCALL *VoidStubFun)(VMFrame &, JSFunction *);
typedef JSObject * (JS_FASTCALL *JSObjStubJSObj)(VMFrame &, JSObject *);
typedef void (JS_FASTCALL *VoidStubAtom)(VMFrame &, JSAtom *);
typedef JSString * (JS_FASTCALL *JSStrStub)(VMFrame &);
typedef JSString * (JS_FASTCALL *JSStrStubUInt32)(VMFrame &, uint32);
typedef void (JS_FASTCALL *VoidStubJSObj)(VMFrame &, JSObject *);
typedef void (JS_FASTCALL *VoidStubPC)(VMFrame &, jsbytecode *);
typedef JSBool (JS_FASTCALL *BoolStubUInt32)(VMFrame &f, uint32);
#ifdef JS_MONOIC
typedef void (JS_FASTCALL *VoidStubCallIC)(VMFrame &, js::mjit::ic::CallICInfo *);
typedef void * (JS_FASTCALL *VoidPtrStubCallIC)(VMFrame &, js::mjit::ic::CallICInfo *);
typedef void (JS_FASTCALL *VoidStubMIC)(VMFrame &, js::mjit::ic::MICInfo *);
typedef void * (JS_FASTCALL *VoidPtrStubMIC)(VMFrame &, js::mjit::ic::MICInfo *);
typedef JSBool (JS_FASTCALL *BoolStubEqualityIC)(VMFrame &, js::mjit::ic::EqualityICInfo *);
typedef void * (JS_FASTCALL *VoidPtrStubTraceIC)(VMFrame &, js::mjit::ic::TraceICInfo *);
#endif
#ifdef JS_POLYIC
typedef void (JS_FASTCALL *VoidStubPIC)(VMFrame &, js::mjit::ic::PICInfo *);
typedef void (JS_FASTCALL *VoidStubGetElemIC)(VMFrame &, js::mjit::ic::GetElementIC *);
typedef void (JS_FASTCALL *VoidStubSetElemIC)(VMFrame &f, js::mjit::ic::SetElementIC *);
#endif

namespace mjit {

struct CallSite;

struct JITScript {
    typedef JSC::MacroAssemblerCodeRef CodeRef;
    CodeRef         code;       /* pool & code addresses */
    void            **nmap;     /* pc -> JIT code map, sparse */

    js::mjit::CallSite *callSites;
    uint32          nCallSites;
#ifdef JS_MONOIC
    ic::MICInfo     *mics;      /* MICs in this script. */
    uint32          nMICs;      /* number of MonoICs */
    ic::CallICInfo  *callICs;   /* CallICs in this script. */
    uint32          nCallICs;   /* number of call ICs */
    ic::EqualityICInfo *equalityICs;
    uint32          nEqualityICs;
    ic::TraceICInfo *traceICs;
    uint32          nTraceICs;

    // Additional ExecutablePools that IC stubs were generated into.
    typedef Vector<JSC::ExecutablePool *, 0, SystemAllocPolicy> ExecPoolVector;
    ExecPoolVector execPools;
#endif
#ifdef JS_POLYIC
    ic::PICInfo     *pics;      /* PICs in this script */
    uint32          nPICs;      /* number of PolyICs */
    ic::GetElementIC *getElems;
    uint32           nGetElems;
    ic::SetElementIC *setElems;
    uint32           nSetElems;
#endif
    void            *invokeEntry;       /* invoke address */
    void            *fastEntry;         /* cached entry, fastest */
    void            *arityCheckEntry;   /* arity check address */

    bool isValidCode(void *ptr) {
        char *jitcode = (char *)code.m_code.executableAddress();
        char *jcheck = (char *)ptr;
        return jcheck >= jitcode && jcheck < jitcode + code.m_size;
    }

    void sweepCallICs();
    void purgeMICs();
    void purgePICs();
    void release();
};

/*
 * Execute the given mjit code. This is a low-level call and callers must
 * provide the same guarantees as JaegerShot/CheckStackAndEnterMethodJIT.
 */
JSBool EnterMethodJIT(JSContext *cx, JSStackFrame *fp, void *code, Value *stackLimit);

/* Execute a method that has been JIT compiled. */
JSBool JaegerShot(JSContext *cx);

/* Drop into the middle of a method at an arbitrary point, and execute. */
JSBool JaegerShotAtSafePoint(JSContext *cx, void *safePoint);

enum CompileStatus
{
    Compile_Okay,
    Compile_Abort,
    Compile_Error
};

void JS_FASTCALL
ProfileStubCall(VMFrame &f);

CompileStatus JS_NEVER_INLINE
TryCompile(JSContext *cx, JSStackFrame *fp);

void
ReleaseScriptCode(JSContext *cx, JSScript *script);

static inline CompileStatus
CanMethodJIT(JSContext *cx, JSScript *script, JSStackFrame *fp)
{
    if (!cx->methodJitEnabled)
        return Compile_Abort;
    JITScriptStatus status = script->getJITStatus(fp->isConstructing());
    if (status == JITScript_Invalid)
        return Compile_Abort;
    if (status == JITScript_None)
        return TryCompile(cx, fp);
    return Compile_Okay;
}

struct CallSite
{
    uint32 codeOffset;
    uint32 pcOffset;
    uint32 id;
};

/* Re-enables a tracepoint in the method JIT. */
void
EnableTraceHint(JSScript *script, jsbytecode *pc, uint16_t index);

uintN
GetCallTargetCount(JSScript *script, jsbytecode *pc);

} /* namespace mjit */

} /* namespace js */

inline void *
JSScript::maybeNativeCodeForPC(bool constructing, jsbytecode *pc)
{
    js::mjit::JITScript *jit = getJIT(constructing);
    if (!jit)
        return NULL;
    JS_ASSERT(pc >= code && pc < code + length);
    return jit->nmap[pc - code];
}

inline void **
JSScript::nativeMap(bool constructing)
{
    return getJIT(constructing)->nmap;
}

inline void *
JSScript::nativeCodeForPC(bool constructing, jsbytecode *pc)
{
    void **nmap = nativeMap(constructing);
    JS_ASSERT(pc >= code && pc < code + length);
    JS_ASSERT(nmap[pc - code]);
    return nmap[pc - code];
}

#ifdef _MSC_VER
extern "C" void *JaegerThrowpoline(js::VMFrame *vmFrame);
#else
extern "C" void JaegerThrowpoline();
#endif
extern "C" void InjectJaegerReturn();

#endif /* jsjaeger_h__ */

