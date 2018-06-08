/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/SharedIC.h"

#include "mozilla/Casting.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Sprintf.h"

#include "jslibmath.h"
#include "jstypes.h"

#include "gc/Policy.h"
#include "jit/BaselineCacheIRCompiler.h"
#include "jit/BaselineDebugModeOSR.h"
#include "jit/BaselineIC.h"
#include "jit/JitSpewer.h"
#include "jit/Linker.h"
#include "jit/SharedICHelpers.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "jit/VMFunctions.h"
#include "vm/Interpreter.h"
#include "vm/StringType.h"

#include "jit/MacroAssembler-inl.h"
#include "jit/SharedICHelpers-inl.h"
#include "vm/Interpreter-inl.h"

using mozilla::BitwiseCast;

namespace js {
namespace jit {

#ifdef JS_JITSPEW
void
FallbackICSpew(JSContext* cx, ICFallbackStub* stub, const char* fmt, ...)
{
    if (JitSpewEnabled(JitSpew_BaselineICFallback)) {
        RootedScript script(cx, GetTopJitJSScript(cx));
        jsbytecode* pc = stub->icEntry()->pc(script);

        char fmtbuf[100];
        va_list args;
        va_start(args, fmt);
        (void) VsprintfLiteral(fmtbuf, fmt, args);
        va_end(args);

        JitSpew(JitSpew_BaselineICFallback,
                "Fallback hit for (%s:%u) (pc=%zu,line=%d,uses=%d,stubs=%zu): %s",
                script->filename(),
                script->lineno(),
                script->pcToOffset(pc),
                PCToLineNumber(script, pc),
                script->getWarmUpCount(),
                stub->numOptimizedStubs(),
                fmtbuf);
    }
}

void
TypeFallbackICSpew(JSContext* cx, ICTypeMonitor_Fallback* stub, const char* fmt, ...)
{
    if (JitSpewEnabled(JitSpew_BaselineICFallback)) {
        RootedScript script(cx, GetTopJitJSScript(cx));
        jsbytecode* pc = stub->icEntry()->pc(script);

        char fmtbuf[100];
        va_list args;
        va_start(args, fmt);
        (void) VsprintfLiteral(fmtbuf, fmt, args);
        va_end(args);

        JitSpew(JitSpew_BaselineICFallback,
                "Type monitor fallback hit for (%s:%u) (pc=%zu,line=%d,uses=%d,stubs=%d): %s",
                script->filename(),
                script->lineno(),
                script->pcToOffset(pc),
                PCToLineNumber(script, pc),
                script->getWarmUpCount(),
                (int) stub->numOptimizedMonitorStubs(),
                fmtbuf);
    }
}
#endif // JS_JITSPEW

ICFallbackStub*
ICEntry::fallbackStub() const
{
    return firstStub()->getChainFallback();
}

void
IonICEntry::trace(JSTracer* trc)
{
    TraceManuallyBarrieredEdge(trc, &script_, "IonICEntry::script_");
    traceEntry(trc);
}

void
BaselineICEntry::trace(JSTracer* trc)
{
    traceEntry(trc);
}

void
ICEntry::traceEntry(JSTracer* trc)
{
    if (!hasStub())
        return;
    for (ICStub* stub = firstStub(); stub; stub = stub->next())
        stub->trace(trc);
}

ICStubConstIterator&
ICStubConstIterator::operator++()
{
    MOZ_ASSERT(currentStub_ != nullptr);
    currentStub_ = currentStub_->next();
    return *this;
}


ICStubIterator::ICStubIterator(ICFallbackStub* fallbackStub, bool end)
  : icEntry_(fallbackStub->icEntry()),
    fallbackStub_(fallbackStub),
    previousStub_(nullptr),
    currentStub_(end ? fallbackStub : icEntry_->firstStub()),
    unlinked_(false)
{ }

ICStubIterator&
ICStubIterator::operator++()
{
    MOZ_ASSERT(currentStub_->next() != nullptr);
    if (!unlinked_)
        previousStub_ = currentStub_;
    currentStub_ = currentStub_->next();
    unlinked_ = false;
    return *this;
}

void
ICStubIterator::unlink(JSContext* cx)
{
    MOZ_ASSERT(currentStub_->next() != nullptr);
    MOZ_ASSERT(currentStub_ != fallbackStub_);
    MOZ_ASSERT(!unlinked_);

    fallbackStub_->unlinkStub(cx->zone(), previousStub_, currentStub_);

    // Mark the current iterator position as unlinked, so operator++ works properly.
    unlinked_ = true;
}

/* static */ bool
ICStub::NonCacheIRStubMakesGCCalls(Kind kind)
{
    MOZ_ASSERT(IsValidKind(kind));
    MOZ_ASSERT(!IsCacheIRKind(kind));

    switch (kind) {
      case Call_Fallback:
      case Call_Scripted:
      case Call_AnyScripted:
      case Call_Native:
      case Call_ClassHook:
      case Call_ScriptedApplyArray:
      case Call_ScriptedApplyArguments:
      case Call_ScriptedFunCall:
      case Call_ConstStringSplit:
      case WarmUpCounter_Fallback:
      case RetSub_Fallback:
      // These two fallback stubs don't actually make non-tail calls,
      // but the fallback code for the bailout path needs to pop the stub frame
      // pushed during the bailout.
      case GetProp_Fallback:
      case SetProp_Fallback:
        return true;
      default:
        return false;
    }
}

bool
ICStub::makesGCCalls() const
{
    switch (kind()) {
      case CacheIR_Regular:
        return toCacheIR_Regular()->stubInfo()->makesGCCalls();
      case CacheIR_Monitored:
        return toCacheIR_Monitored()->stubInfo()->makesGCCalls();
      case CacheIR_Updated:
        return toCacheIR_Updated()->stubInfo()->makesGCCalls();
      default:
        return NonCacheIRStubMakesGCCalls(kind());
    }
}

void
ICStub::traceCode(JSTracer* trc, const char* name)
{
    JitCode* stubJitCode = jitCode();
    TraceManuallyBarrieredEdge(trc, &stubJitCode, name);
}

void
ICStub::updateCode(JitCode* code)
{
    // Write barrier on the old code.
    JitCode::writeBarrierPre(jitCode());
    stubCode_ = code->raw();
}

/* static */ void
ICStub::trace(JSTracer* trc)
{
    traceCode(trc, "shared-stub-jitcode");

    // If the stub is a monitored fallback stub, then trace the monitor ICs hanging
    // off of that stub.  We don't need to worry about the regular monitored stubs,
    // because the regular monitored stubs will always have a monitored fallback stub
    // that references the same stub chain.
    if (isMonitoredFallback()) {
        ICTypeMonitor_Fallback* lastMonStub =
            toMonitoredFallbackStub()->maybeFallbackMonitorStub();
        if (lastMonStub) {
            for (ICStubConstIterator iter(lastMonStub->firstMonitorStub());
                 !iter.atEnd();
                 iter++)
            {
                MOZ_ASSERT_IF(iter->next() == nullptr, *iter == lastMonStub);
                iter->trace(trc);
            }
        }
    }

    if (isUpdated()) {
        for (ICStubConstIterator iter(toUpdatedStub()->firstUpdateStub()); !iter.atEnd(); iter++) {
            MOZ_ASSERT_IF(iter->next() == nullptr, iter->isTypeUpdate_Fallback());
            iter->trace(trc);
        }
    }

    switch (kind()) {
      case ICStub::Call_Scripted: {
        ICCall_Scripted* callStub = toCall_Scripted();
        TraceEdge(trc, &callStub->callee(), "baseline-callscripted-callee");
        TraceNullableEdge(trc, &callStub->templateObject(), "baseline-callscripted-template");
        break;
      }
      case ICStub::Call_Native: {
        ICCall_Native* callStub = toCall_Native();
        TraceEdge(trc, &callStub->callee(), "baseline-callnative-callee");
        TraceNullableEdge(trc, &callStub->templateObject(), "baseline-callnative-template");
        break;
      }
      case ICStub::Call_ClassHook: {
        ICCall_ClassHook* callStub = toCall_ClassHook();
        TraceNullableEdge(trc, &callStub->templateObject(), "baseline-callclasshook-template");
        break;
      }
      case ICStub::Call_ConstStringSplit: {
        ICCall_ConstStringSplit* callStub = toCall_ConstStringSplit();
        TraceEdge(trc, &callStub->templateObject(), "baseline-callstringsplit-template");
        TraceEdge(trc, &callStub->expectedSep(), "baseline-callstringsplit-sep");
        TraceEdge(trc, &callStub->expectedStr(), "baseline-callstringsplit-str");
        break;
      }
      case ICStub::TypeMonitor_SingleObject: {
        ICTypeMonitor_SingleObject* monitorStub = toTypeMonitor_SingleObject();
        TraceEdge(trc, &monitorStub->object(), "baseline-monitor-singleton");
        break;
      }
      case ICStub::TypeMonitor_ObjectGroup: {
        ICTypeMonitor_ObjectGroup* monitorStub = toTypeMonitor_ObjectGroup();
        TraceEdge(trc, &monitorStub->group(), "baseline-monitor-group");
        break;
      }
      case ICStub::TypeUpdate_SingleObject: {
        ICTypeUpdate_SingleObject* updateStub = toTypeUpdate_SingleObject();
        TraceEdge(trc, &updateStub->object(), "baseline-update-singleton");
        break;
      }
      case ICStub::TypeUpdate_ObjectGroup: {
        ICTypeUpdate_ObjectGroup* updateStub = toTypeUpdate_ObjectGroup();
        TraceEdge(trc, &updateStub->group(), "baseline-update-group");
        break;
      }
      case ICStub::NewArray_Fallback: {
        ICNewArray_Fallback* stub = toNewArray_Fallback();
        TraceNullableEdge(trc, &stub->templateObject(), "baseline-newarray-template");
        TraceEdge(trc, &stub->templateGroup(), "baseline-newarray-template-group");
        break;
      }
      case ICStub::NewObject_Fallback: {
        ICNewObject_Fallback* stub = toNewObject_Fallback();
        TraceNullableEdge(trc, &stub->templateObject(), "baseline-newobject-template");
        break;
      }
      case ICStub::Rest_Fallback: {
        ICRest_Fallback* stub = toRest_Fallback();
        TraceEdge(trc, &stub->templateObject(), "baseline-rest-template");
        break;
      }
      case ICStub::CacheIR_Regular:
        TraceCacheIRStub(trc, this, toCacheIR_Regular()->stubInfo());
        break;
      case ICStub::CacheIR_Monitored:
        TraceCacheIRStub(trc, this, toCacheIR_Monitored()->stubInfo());
        break;
      case ICStub::CacheIR_Updated: {
        ICCacheIR_Updated* stub = toCacheIR_Updated();
        TraceNullableEdge(trc, &stub->updateStubGroup(), "baseline-update-stub-group");
        TraceEdge(trc, &stub->updateStubId(), "baseline-update-stub-id");
        TraceCacheIRStub(trc, this, stub->stubInfo());
        break;
      }
      default:
        break;
    }
}

void
ICFallbackStub::unlinkStub(Zone* zone, ICStub* prev, ICStub* stub)
{
    MOZ_ASSERT(stub->next());

    // If stub is the last optimized stub, update lastStubPtrAddr.
    if (stub->next() == this) {
        MOZ_ASSERT(lastStubPtrAddr_ == stub->addressOfNext());
        if (prev)
            lastStubPtrAddr_ = prev->addressOfNext();
        else
            lastStubPtrAddr_ = icEntry()->addressOfFirstStub();
        *lastStubPtrAddr_ = this;
    } else {
        if (prev) {
            MOZ_ASSERT(prev->next() == stub);
            prev->setNext(stub->next());
        } else {
            MOZ_ASSERT(icEntry()->firstStub() == stub);
            icEntry()->setFirstStub(stub->next());
        }
    }

    state_.trackUnlinkedStub();

    if (zone->needsIncrementalBarrier()) {
        // We are removing edges from ICStub to gcthings. Perform one final trace
        // of the stub for incremental GC, as it must know about those edges.
        stub->trace(zone->barrierTracer());
    }

    if (stub->makesGCCalls() && stub->isMonitored()) {
        // This stub can make calls so we can return to it if it's on the stack.
        // We just have to reset its firstMonitorStub_ field to avoid a stale
        // pointer when purgeOptimizedStubs destroys all optimized monitor
        // stubs (unlinked stubs won't be updated).
        ICTypeMonitor_Fallback* monitorFallback =
            toMonitoredFallbackStub()->maybeFallbackMonitorStub();
        MOZ_ASSERT(monitorFallback);
        stub->toMonitoredStub()->resetFirstMonitorStub(monitorFallback);
    }

#ifdef DEBUG
    // Poison stub code to ensure we don't call this stub again. However, if
    // this stub can make calls, a pointer to it may be stored in a stub frame
    // on the stack, so we can't touch the stubCode_ or GC will crash when
    // tracing this pointer.
    if (!stub->makesGCCalls())
        stub->stubCode_ = (uint8_t*)0xbad;
#endif
}

void
ICFallbackStub::unlinkStubsWithKind(JSContext* cx, ICStub::Kind kind)
{
    for (ICStubIterator iter = beginChain(); !iter.atEnd(); iter++) {
        if (iter->kind() == kind)
            iter.unlink(cx);
    }
}

void
ICFallbackStub::discardStubs(JSContext* cx)
{
    for (ICStubIterator iter = beginChain(); !iter.atEnd(); iter++)
        iter.unlink(cx);
}

void
ICTypeMonitor_Fallback::resetMonitorStubChain(Zone* zone)
{
    if (zone->needsIncrementalBarrier()) {
        // We are removing edges from monitored stubs to gcthings (JitCode).
        // Perform one final trace of all monitor stubs for incremental GC,
        // as it must know about those edges.
        for (ICStub* s = firstMonitorStub_; !s->isTypeMonitor_Fallback(); s = s->next())
            s->trace(zone->barrierTracer());
    }

    firstMonitorStub_ = this;
    numOptimizedMonitorStubs_ = 0;

    if (hasFallbackStub_) {
        lastMonitorStubPtrAddr_ = nullptr;

        // Reset firstMonitorStub_ field of all monitored stubs.
        for (ICStubConstIterator iter = mainFallbackStub_->beginChainConst();
             !iter.atEnd(); iter++)
        {
            if (!iter->isMonitored())
                continue;
            iter->toMonitoredStub()->resetFirstMonitorStub(this);
        }
    } else {
        icEntry_->setFirstStub(this);
        lastMonitorStubPtrAddr_ = icEntry_->addressOfFirstStub();
    }
}

void
ICUpdatedStub::resetUpdateStubChain(Zone* zone)
{
    while (!firstUpdateStub_->isTypeUpdate_Fallback()) {
        if (zone->needsIncrementalBarrier()) {
            // We are removing edges from update stubs to gcthings (JitCode).
            // Perform one final trace of all update stubs for incremental GC,
            // as it must know about those edges.
            firstUpdateStub_->trace(zone->barrierTracer());
        }
        firstUpdateStub_ = firstUpdateStub_->next();
    }

    numOptimizedStubs_ = 0;
}

ICMonitoredStub::ICMonitoredStub(Kind kind, JitCode* stubCode, ICStub* firstMonitorStub)
  : ICStub(kind, ICStub::Monitored, stubCode),
    firstMonitorStub_(firstMonitorStub)
{
    // In order to silence Coverity - null pointer dereference checker
    MOZ_ASSERT(firstMonitorStub_);
    // If the first monitored stub is a ICTypeMonitor_Fallback stub, then
    // double check that _its_ firstMonitorStub is the same as this one.
    MOZ_ASSERT_IF(firstMonitorStub_->isTypeMonitor_Fallback(),
                  firstMonitorStub_->toTypeMonitor_Fallback()->firstMonitorStub() ==
                     firstMonitorStub_);
}

bool
ICMonitoredFallbackStub::initMonitoringChain(JSContext* cx, JSScript* script)
{
    MOZ_ASSERT(fallbackMonitorStub_ == nullptr);

    ICTypeMonitor_Fallback::Compiler compiler(cx, this);
    ICStubSpace* space = script->baselineScript()->fallbackStubSpace();
    ICTypeMonitor_Fallback* stub = compiler.getStub(space);
    if (!stub)
        return false;
    fallbackMonitorStub_ = stub;
    return true;
}

bool
ICMonitoredFallbackStub::addMonitorStubForValue(JSContext* cx, BaselineFrame* frame,
                                                StackTypeSet* types, HandleValue val)
{
    ICTypeMonitor_Fallback* typeMonitorFallback = getFallbackMonitorStub(cx, frame->script());
    if (!typeMonitorFallback)
        return false;
    return typeMonitorFallback->addMonitorStubForValue(cx, frame, types, val);
}

bool
ICUpdatedStub::initUpdatingChain(JSContext* cx, ICStubSpace* space)
{
    MOZ_ASSERT(firstUpdateStub_ == nullptr);

    ICTypeUpdate_Fallback::Compiler compiler(cx);
    ICTypeUpdate_Fallback* stub = compiler.getStub(space);
    if (!stub)
        return false;

    firstUpdateStub_ = stub;
    return true;
}

JitCode*
ICStubCompiler::getStubCode()
{
    JitRealm* realm = cx->realm()->jitRealm();

    // Check for existing cached stubcode.
    uint32_t stubKey = getKey();
    JitCode* stubCode = realm->getStubCode(stubKey);
    if (stubCode)
        return stubCode;

    // Compile new stubcode.
    JitContext jctx(cx, nullptr);
    StackMacroAssembler masm;
#ifndef JS_USE_LINK_REGISTER
    // The first value contains the return addres,
    // which we pull into ICTailCallReg for tail calls.
    masm.adjustFrame(sizeof(intptr_t));
#endif
#ifdef JS_CODEGEN_ARM
    masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif

    if (!generateStubCode(masm))
        return nullptr;
    Linker linker(masm);
    AutoFlushICache afc("getStubCode");
    Rooted<JitCode*> newStubCode(cx, linker.newCode(cx, CodeKind::Baseline));
    if (!newStubCode)
        return nullptr;

    // Cache newly compiled stubcode.
    if (!realm->putStubCode(cx, stubKey, newStubCode))
        return nullptr;

    // After generating code, run postGenerateStubCode().  We must not fail
    // after this point.
    postGenerateStubCode(masm, newStubCode);

    MOZ_ASSERT(entersStubFrame_ == ICStub::NonCacheIRStubMakesGCCalls(kind));
    MOZ_ASSERT(!inStubFrame_);

#ifdef JS_ION_PERF
    writePerfSpewerJitCodeProfile(newStubCode, "BaselineIC");
#endif

    return newStubCode;
}

bool
ICStubCompiler::tailCallVM(const VMFunction& fun, MacroAssembler& masm)
{
    TrampolinePtr code = cx->runtime()->jitRuntime()->getVMWrapper(fun);
    MOZ_ASSERT(fun.expectTailCall == TailCall);
    uint32_t argSize = fun.explicitStackSlots() * sizeof(void*);
    if (engine_ == Engine::Baseline) {
        EmitBaselineTailCallVM(code, masm, argSize);
    } else {
        uint32_t stackSize = argSize + fun.extraValuesToPop * sizeof(Value);
        EmitIonTailCallVM(code, masm, stackSize);
    }
    return true;
}

bool
ICStubCompiler::callVM(const VMFunction& fun, MacroAssembler& masm)
{
    MOZ_ASSERT(inStubFrame_);

    TrampolinePtr code = cx->runtime()->jitRuntime()->getVMWrapper(fun);
    MOZ_ASSERT(fun.expectTailCall == NonTailCall);
    MOZ_ASSERT(engine_ == Engine::Baseline);

    EmitBaselineCallVM(code, masm);
    return true;
}

void
ICStubCompiler::enterStubFrame(MacroAssembler& masm, Register scratch)
{
    MOZ_ASSERT(engine_ == Engine::Baseline);
    EmitBaselineEnterStubFrame(masm, scratch);
#ifdef DEBUG
    framePushedAtEnterStubFrame_ = masm.framePushed();
#endif

    MOZ_ASSERT(!inStubFrame_);
    inStubFrame_ = true;

#ifdef DEBUG
    entersStubFrame_ = true;
#endif
}

void
ICStubCompiler::assumeStubFrame()
{
    MOZ_ASSERT(!inStubFrame_);
    inStubFrame_ = true;

#ifdef DEBUG
    entersStubFrame_ = true;

    // |framePushed| isn't tracked precisely in ICStubs, so simply assume it to
    // be STUB_FRAME_SIZE so that assertions don't fail in leaveStubFrame.
    framePushedAtEnterStubFrame_ = STUB_FRAME_SIZE;
#endif
}

void
ICStubCompiler::leaveStubFrame(MacroAssembler& masm, bool calledIntoIon)
{
    MOZ_ASSERT(entersStubFrame_ && inStubFrame_);
    inStubFrame_ = false;

    MOZ_ASSERT(engine_ == Engine::Baseline);
#ifdef DEBUG
    masm.setFramePushed(framePushedAtEnterStubFrame_);
    if (calledIntoIon)
        masm.adjustFrame(sizeof(intptr_t)); // Calls into ion have this extra.
#endif
    EmitBaselineLeaveStubFrame(masm, calledIntoIon);
}

void
ICStubCompiler::pushStubPayload(MacroAssembler& masm, Register scratch)
{
    if (engine_ == Engine::IonSharedIC) {
        masm.push(Imm32(0));
        return;
    }

    if (inStubFrame_) {
        masm.loadPtr(Address(BaselineFrameReg, 0), scratch);
        masm.pushBaselineFramePtr(scratch, scratch);
    } else {
        masm.pushBaselineFramePtr(BaselineFrameReg, scratch);
    }
}

void
ICStubCompiler::PushStubPayload(MacroAssembler& masm, Register scratch)
{
    pushStubPayload(masm, scratch);
    masm.adjustFrame(sizeof(intptr_t));
}

SharedStubInfo::SharedStubInfo(JSContext* cx, void* payload, ICEntry* icEntry)
  : maybeFrame_(nullptr),
    outerScript_(cx),
    innerScript_(cx),
    icEntry_(icEntry)
{
    if (payload) {
        maybeFrame_ = (BaselineFrame*) payload;
        outerScript_ = maybeFrame_->script();
        innerScript_ = maybeFrame_->script();
    } else {
        IonICEntry* entry = (IonICEntry*) icEntry;
        innerScript_ = entry->script();
        // outerScript_ is initialized lazily.
    }
}

HandleScript
SharedStubInfo::outerScript(JSContext* cx)
{
    if (!outerScript_) {
        js::jit::JitActivationIterator actIter(cx);
        JSJitFrameIter it(actIter->asJit());
        MOZ_ASSERT(it.isExitFrame());
        ++it;
        MOZ_ASSERT(it.isIonJS());
        outerScript_ = it.script();
        MOZ_ASSERT(!it.ionScript()->invalidated());
    }
    return outerScript_;
}

//
void
LoadTypedThingData(MacroAssembler& masm, TypedThingLayout layout, Register obj, Register result)
{
    switch (layout) {
      case Layout_TypedArray:
        masm.loadPtr(Address(obj, TypedArrayObject::dataOffset()), result);
        break;
      case Layout_OutlineTypedObject:
        masm.loadPtr(Address(obj, OutlineTypedObject::offsetOfData()), result);
        break;
      case Layout_InlineTypedObject:
        masm.computeEffectiveAddress(Address(obj, InlineTypedObject::offsetOfDataStart()), result);
        break;
      default:
        MOZ_CRASH();
    }
}

void
BaselineScript::noteAccessedGetter(uint32_t pcOffset)
{
    ICEntry& entry = icEntryFromPCOffset(pcOffset);
    ICFallbackStub* stub = entry.fallbackStub();

    if (stub->isGetProp_Fallback())
        stub->toGetProp_Fallback()->noteAccessedGetter();
}

// TypeMonitor_Fallback
//

bool
ICTypeMonitor_Fallback::addMonitorStubForValue(JSContext* cx, BaselineFrame* frame,
                                               StackTypeSet* types, HandleValue val)
{
    MOZ_ASSERT(types);

    // Don't attach too many SingleObject/ObjectGroup stubs. If the value is a
    // primitive or if we will attach an any-object stub, we can handle this
    // with a single PrimitiveSet or AnyValue stub so we always optimize.
    if (numOptimizedMonitorStubs_ >= MAX_OPTIMIZED_STUBS &&
        val.isObject() &&
        !types->unknownObject())
    {
        return true;
    }

    bool wasDetachedMonitorChain = lastMonitorStubPtrAddr_ == nullptr;
    MOZ_ASSERT_IF(wasDetachedMonitorChain, numOptimizedMonitorStubs_ == 0);

    if (types->unknown()) {
        // The TypeSet got marked as unknown so attach a stub that always
        // succeeds.

        // Check for existing TypeMonitor_AnyValue stubs.
        for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
            if (iter->isTypeMonitor_AnyValue())
                return true;
        }

        // Discard existing stubs.
        resetMonitorStubChain(cx->zone());
        wasDetachedMonitorChain = (lastMonitorStubPtrAddr_ == nullptr);

        ICTypeMonitor_AnyValue::Compiler compiler(cx);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(frame->script()));
        if (!stub) {
            ReportOutOfMemory(cx);
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  Added TypeMonitor stub %p for any value", stub);
        addOptimizedMonitorStub(stub);

    } else if (val.isPrimitive() || types->unknownObject()) {
        if (val.isMagic(JS_UNINITIALIZED_LEXICAL))
            return true;
        MOZ_ASSERT(!val.isMagic());
        JSValueType type = val.isDouble() ? JSVAL_TYPE_DOUBLE : val.extractNonDoubleType();

        // Check for existing TypeMonitor stub.
        ICTypeMonitor_PrimitiveSet* existingStub = nullptr;
        for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
            if (iter->isTypeMonitor_PrimitiveSet()) {
                existingStub = iter->toTypeMonitor_PrimitiveSet();
                if (existingStub->containsType(type))
                    return true;
            }
        }

        if (val.isObject()) {
            // Check for existing SingleObject/ObjectGroup stubs and discard
            // stubs if we find one. Ideally we would discard just these stubs,
            // but unlinking individual type monitor stubs is somewhat
            // complicated.
            MOZ_ASSERT(types->unknownObject());
            bool hasObjectStubs = false;
            for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
                if (iter->isTypeMonitor_SingleObject() || iter->isTypeMonitor_ObjectGroup()) {
                    hasObjectStubs = true;
                    break;
                }
            }
            if (hasObjectStubs) {
                resetMonitorStubChain(cx->zone());
                wasDetachedMonitorChain = (lastMonitorStubPtrAddr_ == nullptr);
                existingStub = nullptr;
            }
        }

        ICTypeMonitor_PrimitiveSet::Compiler compiler(cx, existingStub, type);
        ICStub* stub = existingStub
                       ? compiler.updateStub()
                       : compiler.getStub(compiler.getStubSpace(frame->script()));
        if (!stub) {
            ReportOutOfMemory(cx);
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  %s TypeMonitor stub %p for primitive type %d",
                existingStub ? "Modified existing" : "Created new", stub, type);

        if (!existingStub) {
            MOZ_ASSERT(!hasStub(TypeMonitor_PrimitiveSet));
            addOptimizedMonitorStub(stub);
        }

    } else if (val.toObject().isSingleton()) {
        RootedObject obj(cx, &val.toObject());

        // Check for existing TypeMonitor stub.
        for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
            if (iter->isTypeMonitor_SingleObject() &&
                iter->toTypeMonitor_SingleObject()->object() == obj)
            {
                return true;
            }
        }

        ICTypeMonitor_SingleObject::Compiler compiler(cx, obj);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(frame->script()));
        if (!stub) {
            ReportOutOfMemory(cx);
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  Added TypeMonitor stub %p for singleton %p",
                stub, obj.get());

        addOptimizedMonitorStub(stub);

    } else {
        RootedObjectGroup group(cx, val.toObject().group());

        // Check for existing TypeMonitor stub.
        for (ICStubConstIterator iter(firstMonitorStub()); !iter.atEnd(); iter++) {
            if (iter->isTypeMonitor_ObjectGroup() &&
                iter->toTypeMonitor_ObjectGroup()->group() == group)
            {
                return true;
            }
        }

        ICTypeMonitor_ObjectGroup::Compiler compiler(cx, group);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(frame->script()));
        if (!stub) {
            ReportOutOfMemory(cx);
            return false;
        }

        JitSpew(JitSpew_BaselineIC, "  Added TypeMonitor stub %p for ObjectGroup %p",
                stub, group.get());

        addOptimizedMonitorStub(stub);
    }

    bool firstMonitorStubAdded = wasDetachedMonitorChain && (numOptimizedMonitorStubs_ > 0);

    if (firstMonitorStubAdded) {
        // Was an empty monitor chain before, but a new stub was added.  This is the
        // only time that any main stubs' firstMonitorStub fields need to be updated to
        // refer to the newly added monitor stub.
        ICStub* firstStub = mainFallbackStub_->icEntry()->firstStub();
        for (ICStubConstIterator iter(firstStub); !iter.atEnd(); iter++) {
            // Non-monitored stubs are used if the result has always the same type,
            // e.g. a StringLength stub will always return int32.
            if (!iter->isMonitored())
                continue;

            // Since we just added the first optimized monitoring stub, any
            // existing main stub's |firstMonitorStub| MUST be pointing to the fallback
            // monitor stub (i.e. this stub).
            MOZ_ASSERT(iter->toMonitoredStub()->firstMonitorStub() == this);
            iter->toMonitoredStub()->updateFirstMonitorStub(firstMonitorStub_);
        }
    }

    return true;
}

static bool
DoTypeMonitorFallback(JSContext* cx, BaselineFrame* frame, ICTypeMonitor_Fallback* stub,
                      HandleValue value, MutableHandleValue res)
{
    JSScript* script = frame->script();
    jsbytecode* pc = stub->icEntry()->pc(script);
    TypeFallbackICSpew(cx, stub, "TypeMonitor");

    // Copy input value to res.
    res.set(value);

    if (MOZ_UNLIKELY(value.isMagic())) {
        // It's possible that we arrived here from bailing out of Ion, and that
        // Ion proved that the value is dead and optimized out. In such cases,
        // do nothing. However, it's also possible that we have an uninitialized
        // this, in which case we should not look for other magic values.

        if (value.whyMagic() == JS_OPTIMIZED_OUT) {
            MOZ_ASSERT(!stub->monitorsThis());
            return true;
        }

        // In derived class constructors (including nested arrows/eval), the
        // |this| argument or GETALIASEDVAR can return the magic TDZ value.
        MOZ_ASSERT(value.isMagic(JS_UNINITIALIZED_LEXICAL));
        MOZ_ASSERT(frame->isFunctionFrame() || frame->isEvalFrame());
        MOZ_ASSERT(stub->monitorsThis() ||
                   *GetNextPc(pc) == JSOP_CHECKTHIS ||
                   *GetNextPc(pc) == JSOP_CHECKTHISREINIT ||
                   *GetNextPc(pc) == JSOP_CHECKRETURN);
        if (stub->monitorsThis())
            TypeScript::SetThis(cx, script, TypeSet::UnknownType());
        else
            TypeScript::Monitor(cx, script, pc, TypeSet::UnknownType());
        return true;
    }

    StackTypeSet* types;
    uint32_t argument;
    if (stub->monitorsArgument(&argument)) {
        MOZ_ASSERT(pc == script->code());
        types = TypeScript::ArgTypes(script, argument);
        TypeScript::SetArgument(cx, script, argument, value);
    } else if (stub->monitorsThis()) {
        MOZ_ASSERT(pc == script->code());
        types = TypeScript::ThisTypes(script);
        TypeScript::SetThis(cx, script, value);
    } else {
        types = TypeScript::BytecodeTypes(script, pc);
        TypeScript::Monitor(cx, script, pc, types, value);
    }

    if (MOZ_UNLIKELY(stub->invalid()))
        return true;

    return stub->addMonitorStubForValue(cx, frame, types, value);
}

typedef bool (*DoTypeMonitorFallbackFn)(JSContext*, BaselineFrame*, ICTypeMonitor_Fallback*,
                                        HandleValue, MutableHandleValue);
static const VMFunction DoTypeMonitorFallbackInfo =
    FunctionInfo<DoTypeMonitorFallbackFn>(DoTypeMonitorFallback, "DoTypeMonitorFallback",
                                          TailCall);

bool
ICTypeMonitor_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    MOZ_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(ICStubReg);
    masm.pushBaselineFramePtr(BaselineFrameReg, R0.scratchReg());

    return tailCallVM(DoTypeMonitorFallbackInfo, masm);
}

bool
ICTypeMonitor_PrimitiveSet::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label success;
    if ((flags_ & TypeToFlag(JSVAL_TYPE_INT32)) && !(flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE)))
        masm.branchTestInt32(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_DOUBLE))
        masm.branchTestNumber(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_UNDEFINED))
        masm.branchTestUndefined(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_BOOLEAN))
        masm.branchTestBoolean(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_STRING))
        masm.branchTestString(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_SYMBOL))
        masm.branchTestSymbol(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_OBJECT))
        masm.branchTestObject(Assembler::Equal, R0, &success);

    if (flags_ & TypeToFlag(JSVAL_TYPE_NULL))
        masm.branchTestNull(Assembler::Equal, R0, &success);

    EmitStubGuardFailure(masm);

    masm.bind(&success);
    EmitReturnFromIC(masm);
    return true;
}

static void
MaybeWorkAroundAmdBug(MacroAssembler& masm)
{
    // Attempt to work around an AMD bug (see bug 1034706 and bug 1281759), by
    // inserting 32-bytes of NOPs.
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    if (CPUInfo::NeedAmdBugWorkaround()) {
        masm.nop(9);
        masm.nop(9);
        masm.nop(9);
        masm.nop(5);
    }
#endif
}

bool
ICTypeMonitor_SingleObject::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    MaybeWorkAroundAmdBug(masm);

    // Guard on the object's identity.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    Address expectedObject(ICStubReg, ICTypeMonitor_SingleObject::offsetOfObject());
    masm.branchPtr(Assembler::NotEqual, expectedObject, obj, &failure);
    MaybeWorkAroundAmdBug(masm);

    EmitReturnFromIC(masm);
    MaybeWorkAroundAmdBug(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICTypeMonitor_ObjectGroup::Compiler::generateStubCode(MacroAssembler& masm)
{
    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    MaybeWorkAroundAmdBug(masm);

    // Guard on the object's ObjectGroup. No Spectre mitigations are needed
    // here: we're just recording type information for Ion compilation and
    // it's safe to speculatively return.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    Address expectedGroup(ICStubReg, ICTypeMonitor_ObjectGroup::offsetOfGroup());
    masm.branchTestObjGroupNoSpectreMitigations(Assembler::NotEqual, obj, expectedGroup,
                                                R1.scratchReg(), &failure);
    MaybeWorkAroundAmdBug(masm);

    EmitReturnFromIC(masm);
    MaybeWorkAroundAmdBug(masm);

    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

bool
ICTypeMonitor_AnyValue::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitReturnFromIC(masm);
    return true;
}

bool
ICUpdatedStub::addUpdateStubForValue(JSContext* cx, HandleScript outerScript, HandleObject obj,
                                     HandleObjectGroup group, HandleId id, HandleValue val)
{
    EnsureTrackPropertyTypes(cx, obj, id);

    // Make sure that undefined values are explicitly included in the property
    // types for an object if generating a stub to write an undefined value.
    if (val.isUndefined() && CanHaveEmptyPropertyTypesForOwnProperty(obj)) {
        MOZ_ASSERT(obj->group() == group);
        AddTypePropertyId(cx, obj, id, val);
    }

    bool unknown = false, unknownObject = false;
    AutoSweepObjectGroup sweep(group);
    if (group->unknownProperties(sweep)) {
        unknown = unknownObject = true;
    } else {
        if (HeapTypeSet* types = group->maybeGetProperty(sweep, id)) {
            unknown = types->unknown();
            unknownObject = types->unknownObject();
        } else {
            // We don't record null/undefined types for certain TypedObject
            // properties. In these cases |types| is allowed to be nullptr
            // without implying unknown types. See DoTypeUpdateFallback.
            MOZ_ASSERT(obj->is<TypedObject>());
            MOZ_ASSERT(val.isNullOrUndefined());
        }
    }
    MOZ_ASSERT_IF(unknown, unknownObject);

    // Don't attach too many SingleObject/ObjectGroup stubs unless we can
    // replace them with a single PrimitiveSet or AnyValue stub.
    if (numOptimizedStubs_ >= MAX_OPTIMIZED_STUBS &&
        val.isObject() &&
        !unknownObject)
    {
        return true;
    }

    if (unknown) {
        // Attach a stub that always succeeds. We should not have a
        // TypeUpdate_AnyValue stub yet.
        MOZ_ASSERT(!hasTypeUpdateStub(TypeUpdate_AnyValue));

        // Discard existing stubs.
        resetUpdateStubChain(cx->zone());

        ICTypeUpdate_AnyValue::Compiler compiler(cx);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(outerScript));
        if (!stub)
            return false;

        JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for any value", stub);
        addOptimizedUpdateStub(stub);

    } else if (val.isPrimitive() || unknownObject) {
        JSValueType type = val.isDouble() ? JSVAL_TYPE_DOUBLE : val.extractNonDoubleType();

        // Check for existing TypeUpdate stub.
        ICTypeUpdate_PrimitiveSet* existingStub = nullptr;
        for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
            if (iter->isTypeUpdate_PrimitiveSet()) {
                existingStub = iter->toTypeUpdate_PrimitiveSet();
                MOZ_ASSERT(!existingStub->containsType(type));
            }
        }

        if (val.isObject()) {
            // Discard existing ObjectGroup/SingleObject stubs.
            resetUpdateStubChain(cx->zone());
            if (existingStub)
                addOptimizedUpdateStub(existingStub);
        }

        ICTypeUpdate_PrimitiveSet::Compiler compiler(cx, existingStub, type);
        ICStub* stub = existingStub ? compiler.updateStub()
                                    : compiler.getStub(compiler.getStubSpace(outerScript));
        if (!stub)
            return false;
        if (!existingStub) {
            MOZ_ASSERT(!hasTypeUpdateStub(TypeUpdate_PrimitiveSet));
            addOptimizedUpdateStub(stub);
        }

        JitSpew(JitSpew_BaselineIC, "  %s TypeUpdate stub %p for primitive type %d",
                existingStub ? "Modified existing" : "Created new", stub, type);

    } else if (val.toObject().isSingleton()) {
        RootedObject obj(cx, &val.toObject());

#ifdef DEBUG
        // We should not have a stub for this object.
        for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
            MOZ_ASSERT_IF(iter->isTypeUpdate_SingleObject(),
                          iter->toTypeUpdate_SingleObject()->object() != obj);
        }
#endif

        ICTypeUpdate_SingleObject::Compiler compiler(cx, obj);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(outerScript));
        if (!stub)
            return false;

        JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for singleton %p", stub, obj.get());

        addOptimizedUpdateStub(stub);

    } else {
        RootedObjectGroup group(cx, val.toObject().group());

#ifdef DEBUG
        // We should not have a stub for this group.
        for (ICStubConstIterator iter(firstUpdateStub_); !iter.atEnd(); iter++) {
            MOZ_ASSERT_IF(iter->isTypeUpdate_ObjectGroup(),
                          iter->toTypeUpdate_ObjectGroup()->group() != group);
        }
#endif

        ICTypeUpdate_ObjectGroup::Compiler compiler(cx, group);
        ICStub* stub = compiler.getStub(compiler.getStubSpace(outerScript));
        if (!stub)
            return false;

        JitSpew(JitSpew_BaselineIC, "  Added TypeUpdate stub %p for ObjectGroup %p",
                stub, group.get());

        addOptimizedUpdateStub(stub);
    }

    return true;
}

//
// NewObject_Fallback
//

// Unlike typical baseline IC stubs, the code for NewObject_WithTemplate is
// specialized for the template object being allocated.
static JitCode*
GenerateNewObjectWithTemplateCode(JSContext* cx, JSObject* templateObject)
{
    JitContext jctx(cx, nullptr);
    StackMacroAssembler masm;
#ifdef JS_CODEGEN_ARM
    masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif

    Label failure;
    Register objReg = R0.scratchReg();
    Register tempReg = R1.scratchReg();
    masm.branchIfPretenuredGroup(templateObject->group(), tempReg, &failure);
    masm.branchPtr(Assembler::NotEqual, AbsoluteAddress(cx->realm()->addressOfMetadataBuilder()),
                   ImmWord(0), &failure);
    TemplateObject templateObj(templateObject);
    masm.createGCObject(objReg, tempReg, templateObj, gc::DefaultHeap, &failure);
    masm.tagValue(JSVAL_TYPE_OBJECT, objReg, R0);

    EmitReturnFromIC(masm);
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    Linker linker(masm);
    AutoFlushICache afc("GenerateNewObjectWithTemplateCode");
    return linker.newCode(cx, CodeKind::Baseline);
}

static bool
DoNewObject(JSContext* cx, void* payload, ICNewObject_Fallback* stub, MutableHandleValue res)
{
    SharedStubInfo info(cx, payload, stub->icEntry());

    FallbackICSpew(cx, stub, "NewObject");

    RootedObject obj(cx);

    RootedObject templateObject(cx, stub->templateObject());
    if (templateObject) {
        MOZ_ASSERT(!templateObject->group()->maybePreliminaryObjectsDontCheckGeneration());
        obj = NewObjectOperationWithTemplate(cx, templateObject);
    } else {
        HandleScript script = info.script();
        jsbytecode* pc = info.pc();
        obj = NewObjectOperation(cx, script, pc);

        if (obj && !obj->isSingleton() &&
            !obj->group()->maybePreliminaryObjectsDontCheckGeneration())
        {
            JSObject* templateObject = NewObjectOperation(cx, script, pc, TenuredObject);
            if (!templateObject)
                return false;

            if (!stub->invalid() &&
                (templateObject->is<UnboxedPlainObject>() ||
                 !templateObject->as<PlainObject>().hasDynamicSlots()))
            {
                JitCode* code = GenerateNewObjectWithTemplateCode(cx, templateObject);
                if (!code)
                    return false;

                ICStubSpace* space =
                    ICStubCompiler::StubSpaceForStub(/* makesGCCalls = */ false, script,
                                                     ICStubCompiler::Engine::Baseline);
                ICStub* templateStub = ICStub::New<ICNewObject_WithTemplate>(cx, space, code);
                if (!templateStub)
                    return false;

                stub->addNewStub(templateStub);
            }

            stub->setTemplateObject(templateObject);
        }
    }

    if (!obj)
        return false;

    res.setObject(*obj);
    return true;
}

typedef bool(*DoNewObjectFn)(JSContext*, void*, ICNewObject_Fallback*, MutableHandleValue);
static const VMFunction DoNewObjectInfo =
    FunctionInfo<DoNewObjectFn>(DoNewObject, "DoNewObject", TailCall);

bool
ICNewObject_Fallback::Compiler::generateStubCode(MacroAssembler& masm)
{
    EmitRestoreTailCallReg(masm);

    masm.push(ICStubReg); // stub.
    pushStubPayload(masm, R0.scratchReg());

    return tailCallVM(DoNewObjectInfo, masm);
}

} // namespace jit
} // namespace js
