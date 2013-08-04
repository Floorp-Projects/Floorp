/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ion/AsmJSModule.h"
#include "ion/IonCode.h"

#include "jsobjinlines.h"

using namespace js;

static void AsmJSModuleObject_finalize(FreeOp *fop, JSObject *obj);
static void AsmJSModuleObject_trace(JSTracer *trc, JSObject *obj);

static const unsigned ASM_CODE_RESERVED_SLOT = 0;
static const unsigned ASM_CODE_NUM_RESERVED_SLOTS = 1;

static Class AsmJSModuleClass = {
    "AsmJSModuleObject",
    JSCLASS_IS_ANONYMOUS | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(ASM_CODE_NUM_RESERVED_SLOTS),
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

bool
js::IsAsmJSModuleObject(JSObject *obj)
{
    return obj->getClass() == &AsmJSModuleClass;
}

AsmJSModule &
js::AsmJSModuleObjectToModule(JSObject *obj)
{
    JS_ASSERT(IsAsmJSModuleObject(obj));
    return *(AsmJSModule *)obj->getReservedSlot(ASM_CODE_RESERVED_SLOT).toPrivate();
}

static void
AsmJSModuleObject_finalize(FreeOp *fop, JSObject *obj)
{
    fop->delete_(&AsmJSModuleObjectToModule(obj));
}

static void
AsmJSModuleObject_trace(JSTracer *trc, JSObject *obj)
{
    AsmJSModuleObjectToModule(obj).trace(trc);
}

JSObject *
js::NewAsmJSModuleObject(JSContext *cx, ScopedJSDeletePtr<AsmJSModule> *module)
{
    JSObject *obj = NewObjectWithGivenProto(cx, &AsmJSModuleClass, NULL, NULL);
    if (!obj)
        return NULL;

    obj->setReservedSlot(ASM_CODE_RESERVED_SLOT, PrivateValue(module->forget()));
    return obj;
}

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
    }

    for (size_t i = 0; i < numFunctionCounts(); i++)
        js_delete(functionCounts(i));
}
