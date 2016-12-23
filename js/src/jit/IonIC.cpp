/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonIC.h"

#include "mozilla/Maybe.h"
#include "mozilla/SizePrintfMacros.h"

#include "jit/CacheIRCompiler.h"
#include "jit/Linker.h"

#include "jit/MacroAssembler-inl.h"
#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;

void
IonIC::updateBaseAddress(JitCode* code, MacroAssembler& masm)
{
    fallbackLabel_.repoint(code, &masm);
    rejoinLabel_.repoint(code, &masm);

    codeRaw_ = fallbackLabel_.raw();
}

Register
IonIC::scratchRegisterForEntryJump()
{
    switch (kind_) {
      case CacheKind::GetProp:
      case CacheKind::GetElem: {
        Register temp = asGetPropertyIC()->maybeTemp();
        if (temp != InvalidReg)
            return temp;
        TypedOrValueRegister output = asGetPropertyIC()->output();
        return output.hasValue() ? output.valueReg().scratchReg() : output.typedReg().gpr();
      }
    }

    MOZ_CRASH("Invalid kind");
}

void
IonIC::reset(Zone* zone)
{
    if (firstStub_ && zone->needsIncrementalBarrier()) {
        // We are removing edges from IonIC to gcthings. Perform one final trace
        // of the stub for incremental GC, as it must know about those edges.
        trace(zone->barrierTracer());
    }

#ifdef JS_CRASH_DIAGNOSTICS
    IonICStub* stub = firstStub_;
    while (stub) {
        IonICStub* next = stub->next();
        stub->poison();
        stub = next;
    }
#endif

    firstStub_ = nullptr;
    codeRaw_ = fallbackLabel_.raw();
    numStubs_ = 0;
}

void
IonIC::trace(JSTracer* trc)
{
    if (script_)
        TraceManuallyBarrieredEdge(trc, &script_, "IonIC::script_");
}

/* static */ bool
IonGetPropertyIC::update(JSContext* cx, HandleScript outerScript, IonGetPropertyIC* ic,
			 HandleObject obj, HandleValue idVal, MutableHandleValue res)
{
    // Override the return value if we are invalidated (bug 728188).
    AutoDetectInvalidation adi(cx, res, outerScript->ionScript());

    // If the IC is idempotent, we will redo the op in the interpreter.
    if (ic->idempotent())
        adi.disable();

    bool attached = false;
    if (!attached && ic->idempotent()) {
        // Invalidate the cache if the property was not found, or was found on
        // a non-native object. This ensures:
        // 1) The property read has no observable side-effects.
        // 2) There's no need to dynamically monitor the return type. This would
        //    be complicated since (due to GVN) there can be multiple pc's
        //    associated with a single idempotent cache.
        JitSpew(JitSpew_IonIC, "Invalidating from idempotent cache %s:%" PRIuSIZE,
                outerScript->filename(), outerScript->lineno());

        outerScript->setInvalidatedIdempotentCache();

        // Do not re-invalidate if the lookup already caused invalidation.
        if (outerScript->hasIonScript())
            Invalidate(cx, outerScript);
    }

    if (ic->kind() == CacheKind::GetProp) {
        if (!GetProperty(cx, obj, obj, idVal.toString()->asAtom().asPropertyName(), res))
            return false;
    } else {
        MOZ_ASSERT(ic->kind() == CacheKind::GetElem);
        if (!GetObjectElementOperation(cx, JSOp(*ic->pc()), obj, obj, idVal, res))
            return false;
    }

    if (!ic->idempotent()) {
        // Monitor changes to cache entry.
        if (!ic->monitoredResult())
            TypeScript::Monitor(cx, ic->script(), ic->pc(), res);
    }

    return true;
}
