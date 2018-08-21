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
ICEntry::trace(JSTracer* trc)
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


} // namespace jit
} // namespace js
