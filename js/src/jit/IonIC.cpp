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
      case CacheKind::GetName:
      case CacheKind::SetProp:
        MOZ_CRASH("Baseline-specific for now");
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

    uint8_t* nextCodeRaw = codeRaw_;
    for (IonICStub* stub = firstStub_; stub; stub = stub->next()) {
        JitCode* code = JitCode::FromExecutable(nextCodeRaw);
        TraceManuallyBarrieredEdge(trc, &code, "ion-ic-code");

        TraceCacheIRStub(trc, stub, stub->stubInfo());

        nextCodeRaw = stub->nextCodeRaw();
    }

    MOZ_ASSERT(nextCodeRaw == fallbackLabel_.raw());
}

void
IonGetPropertyIC::maybeDisable(Zone* zone, bool attached)
{
    if (attached) {
        failedUpdates_ = 0;
        return;
    }

    if (!canAttachStub() && kind() == CacheKind::GetProp) {
        // Don't disable the cache (and discard stubs) if we have a GETPROP and
        // attached the maximum number of stubs. This can happen when JS code
        // uses an AST-like data structure and accesses a field of a "base
        // class", like node.nodeType. This should be temporary until we handle
        // this case better, see bug 1107515.
        return;
    }

    if (++failedUpdates_ > MAX_FAILED_UPDATES) {
        JitSpew(JitSpew_IonIC, "Disable inline cache");
        disable(zone);
    }
}

/* static */ bool
IonGetPropertyIC::update(JSContext* cx, HandleScript outerScript, IonGetPropertyIC* ic,
			 HandleValue val, HandleValue idVal, MutableHandleValue res)
{
    // Override the return value if we are invalidated (bug 728188).
    AutoDetectInvalidation adi(cx, res, outerScript->ionScript());

    // If the IC is idempotent, we will redo the op in the interpreter.
    if (ic->idempotent())
        adi.disable();

    bool attached = false;
    if (!JitOptions.disableCacheIR && !ic->disabled()) {
        if (ic->canAttachStub()) {
            // IonBuilder calls PropertyReadNeedsTypeBarrier to determine if it
            // needs a type barrier. Unfortunately, PropertyReadNeedsTypeBarrier
            // does not account for getters, so we should only attach a getter
            // stub if we inserted a type barrier.
            CanAttachGetter canAttachGetter =
                ic->monitoredResult() ? CanAttachGetter::Yes : CanAttachGetter::No;
            jsbytecode* pc = ic->idempotent() ? nullptr : ic->pc();
            bool isTemporarilyUnoptimizable;
            GetPropIRGenerator gen(cx, pc, ic->kind(), ICStubEngine::IonIC,
                                   &isTemporarilyUnoptimizable,
                                   val, idVal, canAttachGetter);
            if (ic->idempotent() ? gen.tryAttachIdempotentStub() : gen.tryAttachStub()) {
                attached = ic->attachCacheIRStub(cx, gen.writerRef(), gen.cacheKind(),
                                                 outerScript);
            }
        }
        ic->maybeDisable(cx->zone(), attached);
    }

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

        // We will redo the potentially effectful lookup in Baseline.
        return true;
    }

    if (ic->kind() == CacheKind::GetProp) {
        RootedPropertyName name(cx, idVal.toString()->asAtom().asPropertyName());
        if (!GetProperty(cx, val, name, res))
            return false;
    } else {
        MOZ_ASSERT(ic->kind() == CacheKind::GetElem);
        if (!GetElementOperation(cx, JSOp(*ic->pc()), val, idVal, res))
            return false;
    }

    if (!ic->idempotent()) {
        // Monitor changes to cache entry.
        if (!ic->monitoredResult())
            TypeScript::Monitor(cx, ic->script(), ic->pc(), res);
    }

    return true;
}

uint8_t*
IonICStub::stubDataStart()
{
    return reinterpret_cast<uint8_t*>(this) + stubInfo_->stubDataOffset();
}

void
IonIC::attachStub(IonICStub* newStub, JitCode* code)
{
    MOZ_ASSERT(canAttachStub());
    MOZ_ASSERT(newStub);
    MOZ_ASSERT(code);

    if (firstStub_) {
        IonICStub* last = firstStub_;
        while (IonICStub* next = last->next())
            last = next;
        last->setNext(newStub, code);
    } else {
        firstStub_ = newStub;
        codeRaw_ = code->raw();
    }

    numStubs_++;
}
