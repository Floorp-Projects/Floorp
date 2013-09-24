/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AsmJSModule.h"
#include "jit/IonCode.h"

#ifndef XP_WIN
# include <sys/mman.h>
#endif

#include "jslibmath.h"
#include "jsmath.h"
#ifdef XP_WIN
# include "jswin.h"
#endif

#include "js/MemoryMetrics.h"

#include "jsobjinlines.h"

using namespace js;
using namespace jit;

void
AsmJSModule::initHeap(Handle<ArrayBufferObject*> heap, JSContext *cx)
{
    JS_ASSERT(linked_);
    JS_ASSERT(!maybeHeap_);
    maybeHeap_ = heap;
    heapDatum() = heap->dataPointer();

    JS_ASSERT(IsValidAsmJSHeapLength(heap->byteLength()));
#if defined(JS_CPU_X86)
    uint8_t *heapOffset = heap->dataPointer();
    void *heapLength = (void*)heap->byteLength();
    for (unsigned i = 0; i < heapAccesses_.length(); i++) {
        const jit::AsmJSHeapAccess &access = heapAccesses_[i];
        if (access.hasLengthCheck())
            JSC::X86Assembler::setPointer(access.patchLengthAt(code_), heapLength);
        void *addr = access.patchOffsetAt(code_);
        uint32_t disp = reinterpret_cast<uint32_t>(JSC::X86Assembler::getPointer(addr));
        JS_ASSERT(disp <= INT32_MAX);
        JSC::X86Assembler::setPointer(addr, (void *)(heapOffset + disp));
    }
#elif defined(JS_CPU_ARM)
    uint32_t heapLength = heap->byteLength();
    for (unsigned i = 0; i < heapAccesses_.length(); i++) {
        jit::Assembler::updateBoundsCheck(heapLength,
                                          (jit::Instruction*)(heapAccesses_[i].offset() + code_));
    }
    // We already know the exact extent of areas that need to be patched, just make sure we
    // flush all of them at once.
    jit::AutoFlushCache::updateTop(uintptr_t(code_), pod.codeBytes_);
#endif
}

static uint8_t *
AllocateExecutableMemory(ExclusiveContext *cx, size_t totalBytes)
{
    JS_ASSERT(totalBytes % AsmJSPageSize == 0);

#ifdef XP_WIN
    void *p = VirtualAlloc(NULL, totalBytes, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!p) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }
#else  // assume Unix
    void *p = mmap(NULL, totalBytes, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }
#endif

    return (uint8_t *)p;
}

static void
DeallocateExecutableMemory(uint8_t *code, size_t totalBytes)
{
#ifdef XP_WIN
    JS_ALWAYS_TRUE(VirtualFree(code, 0, MEM_RELEASE));
#else
    JS_ALWAYS_TRUE(munmap(code, totalBytes) == 0);
#endif
}

bool
AsmJSModule::allocateAndCopyCode(ExclusiveContext *cx, MacroAssembler &masm)
{
    JS_ASSERT(!code_);

    // The global data section sits immediately after the executable (and
    // other) data allocated by the MacroAssembler, so ensure it is
    // double-aligned.
    pod.codeBytes_ = AlignBytes(masm.bytesNeeded(), sizeof(double));

    // The entire region is allocated via mmap/VirtualAlloc which requires
    // units of pages.
    pod.totalBytes_ = AlignBytes(pod.codeBytes_ + globalDataBytes(), AsmJSPageSize);

    code_ = AllocateExecutableMemory(cx, pod.totalBytes_);
    if (!code_)
        return false;

    JS_ASSERT(uintptr_t(code_) % AsmJSPageSize == 0);
    masm.executableCopy(code_);
    return true;
}

static int32_t
CoerceInPlace_ToInt32(JSContext *cx, MutableHandleValue val)
{
    int32_t i32;
    if (!ToInt32(cx, val, &i32))
        return false;
    val.set(Int32Value(i32));

    return true;
}

static int32_t
CoerceInPlace_ToNumber(JSContext *cx, MutableHandleValue val)
{
    double dbl;
    if (!ToNumber(cx, val, &dbl))
        return false;
    val.set(DoubleValue(dbl));

    return true;
}

static void
EnableActivationFromAsmJS(AsmJSActivation *activation)
{
    JSContext *cx = activation->cx();
    Activation *act = cx->mainThread().activation();
    JS_ASSERT(act->isJit());
    act->asJit()->setActive(cx);
}

static void
DisableActivationFromAsmJS(AsmJSActivation *activation)
{
    JSContext *cx = activation->cx();
    Activation *act = cx->mainThread().activation();
    JS_ASSERT(act->isJit());
    act->asJit()->setActive(cx, false);
}

namespace js {

// Defined in AsmJS.cpp:

int32_t
InvokeFromAsmJS_Ignore(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv);

int32_t
InvokeFromAsmJS_ToInt32(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv);

int32_t
InvokeFromAsmJS_ToNumber(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv);

}

#if defined(JS_CPU_ARM)
extern "C" {

extern int
__aeabi_idivmod(int, int);

extern int
__aeabi_uidivmod(int, int);

}
#endif

template <class F>
static inline void *
FuncCast(F *pf)
{
    return JS_FUNC_TO_DATA_PTR(void *, pf);
}

static void *
AddressOf(AsmJSImmKind kind, ExclusiveContext *cx)
{
    switch (kind) {
      case AsmJSImm_Runtime:
        return cx->runtimeAddressForJit();
      case AsmJSImm_StackLimit:
        return cx->stackLimitAddress(StackForUntrustedScript);
      case AsmJSImm_ReportOverRecursed:
        return FuncCast<void (JSContext*)>(js_ReportOverRecursed);
      case AsmJSImm_HandleExecutionInterrupt:
        return FuncCast(js_HandleExecutionInterrupt);
      case AsmJSImm_InvokeFromAsmJS_Ignore:
        return FuncCast(InvokeFromAsmJS_Ignore);
      case AsmJSImm_InvokeFromAsmJS_ToInt32:
        return FuncCast(InvokeFromAsmJS_ToInt32);
      case AsmJSImm_InvokeFromAsmJS_ToNumber:
        return FuncCast(InvokeFromAsmJS_ToNumber);
      case AsmJSImm_CoerceInPlace_ToInt32:
        return FuncCast(CoerceInPlace_ToInt32);
      case AsmJSImm_CoerceInPlace_ToNumber:
        return FuncCast(CoerceInPlace_ToNumber);
      case AsmJSImm_ToInt32:
        return FuncCast<int32_t (double)>(js::ToInt32);
      case AsmJSImm_EnableActivationFromAsmJS:
        return FuncCast(EnableActivationFromAsmJS);
      case AsmJSImm_DisableActivationFromAsmJS:
        return FuncCast(DisableActivationFromAsmJS);
#if defined(JS_CPU_ARM)
      case AsmJSImm_aeabi_idivmod:
        return FuncCast(__aeabi_idivmod);
      case AsmJSImm_aeabi_uidivmod:
        return FuncCast(__aeabi_uidivmod);
#endif
      case AsmJSImm_ModD:
        return FuncCast(NumberMod);
      case AsmJSImm_SinD:
        return FuncCast<double (double)>(sin);
      case AsmJSImm_CosD:
        return FuncCast<double (double)>(cos);
      case AsmJSImm_TanD:
        return FuncCast<double (double)>(tan);
      case AsmJSImm_ASinD:
        return FuncCast<double (double)>(asin);
      case AsmJSImm_ACosD:
        return FuncCast<double (double)>(acos);
      case AsmJSImm_ATanD:
        return FuncCast<double (double)>(atan);
      case AsmJSImm_CeilD:
        return FuncCast<double (double)>(ceil);
      case AsmJSImm_FloorD:
        return FuncCast<double (double)>(floor);
      case AsmJSImm_ExpD:
        return FuncCast<double (double)>(exp);
      case AsmJSImm_LogD:
        return FuncCast<double (double)>(log);
      case AsmJSImm_PowD:
        return FuncCast(ecmaPow);
      case AsmJSImm_ATan2D:
        return FuncCast(ecmaAtan2);
    }

    MOZ_ASSUME_UNREACHABLE("Bad AsmJSImmKind");
    return NULL;
}

void
AsmJSModule::staticallyLink(const AsmJSStaticLinkData &linkData, ExclusiveContext *cx)
{
    // Process AsmJSStaticLinkData:

    operationCallbackExit_ = code_ + linkData.operationCallbackExitOffset;

    for (size_t i = 0; i < linkData.relativeLinks.length(); i++) {
        AsmJSStaticLinkData::RelativeLink link = linkData.relativeLinks[i];
        *(void **)(code_ + link.patchAtOffset) = code_ + link.targetOffset;
    }

    for (size_t i = 0; i < linkData.absoluteLinks.length(); i++) {
        AsmJSStaticLinkData::AbsoluteLink link = linkData.absoluteLinks[i];
        Assembler::patchDataWithValueCheck(code_ + link.patchAt.offset(),
                                           PatchedImmPtr(AddressOf(link.target, cx)),
                                           PatchedImmPtr((void*)-1));
    }

    // Initialize global data segment

    for (size_t i = 0; i < exits_.length(); i++) {
        exitIndexToGlobalDatum(i).exit = interpExitTrampoline(exits_[i]);
        exitIndexToGlobalDatum(i).fun = NULL;
    }
}

AsmJSModule::AsmJSModule(ScriptSource *scriptSource, uint32_t charsBegin)
  : globalArgumentName_(NULL),
    importArgumentName_(NULL),
    bufferArgumentName_(NULL),
    minHeapLength_(AsmJSAllocationGranularity),
    code_(NULL),
    operationCallbackExit_(NULL),
    linked_(false),
    charsBegin_(charsBegin),
    scriptSource_(scriptSource)
{
    mozilla::PodZero(&pod);
    scriptSource_->incref();
}

AsmJSModule::~AsmJSModule()
{
    scriptSource_->decref();

    if (code_) {
        for (unsigned i = 0; i < numExits(); i++) {
            AsmJSModule::ExitDatum &exitDatum = exitIndexToGlobalDatum(i);
            if (!exitDatum.fun)
                continue;

            if (!exitDatum.fun->hasScript())
                continue;

            JSScript *script = exitDatum.fun->nonLazyScript();
            if (!script->hasIonScript())
                continue;

            jit::DependentAsmJSModuleExit exit(this, i);
            script->ionScript()->removeDependentAsmJSModule(exit);
        }

        DeallocateExecutableMemory(code_, pod.totalBytes_);
    }

    for (size_t i = 0; i < numFunctionCounts(); i++)
        js_delete(functionCounts(i));
}

void
AsmJSModule::sizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t *asmJSModuleCode,
                        size_t *asmJSModuleData)
{
    *asmJSModuleCode = pod.totalBytes_;
    *asmJSModuleData = mallocSizeOf(this) +
                       globals_.sizeOfExcludingThis(mallocSizeOf) +
                       exits_.sizeOfExcludingThis(mallocSizeOf) +
                       exports_.sizeOfExcludingThis(mallocSizeOf) +
                       heapAccesses_.sizeOfExcludingThis(mallocSizeOf) +
#if defined(MOZ_VTUNE)
                       profiledFunctions_.sizeOfExcludingThis(mallocSizeOf) +
#endif
#if defined(JS_ION_PERF)
                       profiledFunctions_.sizeOfExcludingThis(mallocSizeOf) +
                       perfProfiledBlocksFunctions_.sizeOfExcludingThis(mallocSizeOf) +
#endif
                       functionCounts_.sizeOfExcludingThis(mallocSizeOf);
}

static void
AsmJSModuleObject_finalize(FreeOp *fop, JSObject *obj)
{
    fop->delete_(&obj->as<AsmJSModuleObject>().module());
}

static void
AsmJSModuleObject_trace(JSTracer *trc, JSObject *obj)
{
    obj->as<AsmJSModuleObject>().module().trace(trc);
}

const Class AsmJSModuleObject::class_ = {
    "AsmJSModuleObject",
    JSCLASS_IS_ANONYMOUS | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(AsmJSModuleObject::RESERVED_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    NULL,                    /* convert     */
    AsmJSModuleObject_finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* hasInstance */
    NULL,                    /* construct   */
    AsmJSModuleObject_trace
};

AsmJSModuleObject *
AsmJSModuleObject::create(ExclusiveContext *cx, ScopedJSDeletePtr<AsmJSModule> *module)
{
    JSObject *obj = NewObjectWithGivenProto(cx, &AsmJSModuleObject::class_, NULL, NULL);
    if (!obj)
        return NULL;

    obj->setReservedSlot(MODULE_SLOT, PrivateValue(module->forget()));
    return &obj->as<AsmJSModuleObject>();
}

AsmJSModule &
AsmJSModuleObject::module() const
{
    JS_ASSERT(is<AsmJSModuleObject>());
    return *(AsmJSModule *)getReservedSlot(MODULE_SLOT).toPrivate();
}
