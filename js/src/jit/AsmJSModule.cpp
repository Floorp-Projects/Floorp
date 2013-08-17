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

#ifdef XP_WIN
# include "jswin.h"
#endif

#include "js/MemoryMetrics.h"

#include "jsobjinlines.h"

using namespace js;

void
AsmJSModule::patchHeapAccesses(ArrayBufferObject *heap, JSContext *cx)
{
    JS_ASSERT(IsPowerOfTwo(heap->byteLength()));
#if defined(JS_CPU_X86)
    void *heapOffset = (void*)heap->dataPointer();
    void *heapLength = (void*)heap->byteLength();
    for (unsigned i = 0; i < heapAccesses_.length(); i++) {
        JSC::X86Assembler::setPointer(heapAccesses_[i].patchLengthAt(code_), heapLength);
        JSC::X86Assembler::setPointer(heapAccesses_[i].patchOffsetAt(code_), heapOffset);
    }
#elif defined(JS_CPU_ARM)
    ion::IonContext ic(cx, NULL);
    ion::AutoFlushCache afc("patchBoundsCheck");
    uint32_t bits = mozilla::CeilingLog2(heap->byteLength());
    for (unsigned i = 0; i < heapAccesses_.length(); i++)
        ion::Assembler::updateBoundsCheck(bits, (ion::Instruction*)(heapAccesses_[i].offset() + code_));
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

uint8_t *
AsmJSModule::allocateCodeAndGlobalSegment(ExclusiveContext *cx, size_t bytesNeeded)
{
    JS_ASSERT(!code_);

    // The global data section sits immediately after the executable (and
    // other) data allocated by the MacroAssembler, so ensure it is
    // double-aligned.
    codeBytes_ = AlignBytes(bytesNeeded, sizeof(double));

    // The entire region is allocated via mmap/VirtualAlloc which requires
    // units of pages.
    totalBytes_ = AlignBytes(codeBytes_ + globalDataBytes(), AsmJSPageSize);

    code_ = AllocateExecutableMemory(cx, totalBytes_);
    if (!code_)
        return NULL;

    JS_ASSERT(uintptr_t(code_) % AsmJSPageSize == 0);
    return code_;
}

AsmJSModule::~AsmJSModule()
{
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

            ion::DependentAsmJSModuleExit exit(this, i);
            script->ionScript()->removeDependentAsmJSModule(exit);
        }

        DeallocateExecutableMemory(code_, totalBytes_);
    }

    for (size_t i = 0; i < numFunctionCounts(); i++)
        js_delete(functionCounts(i));
}

void
AsmJSModule::sizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t *asmJSModuleCode,
                        size_t *asmJSModuleData)
{
    *asmJSModuleCode = totalBytes_;
    *asmJSModuleData = mallocSizeOf(this) +
                       globals_.sizeOfExcludingThis(mallocSizeOf) +
                       exits_.sizeOfExcludingThis(mallocSizeOf) +
                       exports_.sizeOfExcludingThis(mallocSizeOf) +
                       heapAccesses_.sizeOfExcludingThis(mallocSizeOf) +
#if defined(MOZ_VTUNE)
                       profiledFunctions_.sizeOfExcludingThis(mallocSizeOf) +
#endif
#if defined(JS_ION_PERF)
                       perfProfiledFunctions_.sizeOfExcludingThis(mallocSizeOf) +
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

Class AsmJSModuleObject::class_ = {
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
