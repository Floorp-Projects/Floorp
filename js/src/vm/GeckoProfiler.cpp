/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/GeckoProfiler-inl.h"

#include "mozilla/DebugOnly.h"

#include "jsnum.h"

#include "gc/PublicIterators.h"
#include "jit/BaselineFrame.h"
#include "jit/BaselineJIT.h"
#include "jit/JitcodeMap.h"
#include "jit/JitFrames.h"
#include "jit/JSJitFrameIter.h"
#include "util/StringBuffer.h"
#include "vm/JSScript.h"

#include "gc/Marking-inl.h"

using namespace js;

using mozilla::DebugOnly;

GeckoProfilerThread::GeckoProfilerThread()
  : profilingStack_(nullptr)
{
}

GeckoProfilerRuntime::GeckoProfilerRuntime(JSRuntime* rt)
  : rt(rt),
    strings(mutexid::GeckoProfilerStrings),
    slowAssertions(false),
    enabled_(false),
    eventMarker_(nullptr)
{
    MOZ_ASSERT(rt != nullptr);
}

bool
GeckoProfilerRuntime::init()
{
    auto locked = strings.lock();
    if (!locked->init())
        return false;

    return true;
}

void
GeckoProfilerThread::setProfilingStack(ProfilingStack* profilingStack)
{
    profilingStack_ = profilingStack;
}

void
GeckoProfilerRuntime::setEventMarker(void (*fn)(const char*))
{
    eventMarker_ = fn;
}

// Get a pointer to the top-most profiling frame, given the exit frame pointer.
static void*
GetTopProfilingJitFrame(Activation* act)
{
    if (!act || !act->isJit())
        return nullptr;

    jit::JitActivation* jitActivation = act->asJit();

    // If there is no exit frame set, just return.
    if (!jitActivation->hasExitFP())
        return nullptr;

    // Skip wasm frames that might be in the way.
    OnlyJSJitFrameIter iter(jitActivation);
    if (iter.done())
        return nullptr;

    jit::JSJitProfilingFrameIterator jitIter((jit::CommonFrameLayout*) iter.frame().fp());
    MOZ_ASSERT(!jitIter.done());
    return jitIter.fp();
}

void
GeckoProfilerRuntime::enable(bool enabled)
{
    JSContext* cx = rt->mainContextFromAnyThread();
    MOZ_ASSERT(cx->geckoProfiler().installed());

    if (enabled_ == enabled)
        return;

    /*
     * Ensure all future generated code will be instrumented, or that all
     * currently instrumented code is discarded
     */
    ReleaseAllJITCode(rt->defaultFreeOp());

    // This function is called when the Gecko profiler makes a new Sampler
    // (and thus, a new circular buffer). Set all current entries in the
    // JitcodeGlobalTable as expired and reset the buffer range start.
    if (rt->hasJitRuntime() && rt->jitRuntime()->hasJitcodeGlobalTable())
        rt->jitRuntime()->getJitcodeGlobalTable()->setAllEntriesAsExpired();
    rt->setProfilerSampleBufferRangeStart(0);

    // Ensure that lastProfilingFrame is null for the main thread.
    if (cx->jitActivation) {
        cx->jitActivation->setLastProfilingFrame(nullptr);
        cx->jitActivation->setLastProfilingCallSite(nullptr);
    }

    enabled_ = enabled;

    /* Toggle Gecko Profiler-related jumps on baseline jitcode.
     * The call to |ReleaseAllJITCode| above will release most baseline jitcode, but not
     * jitcode for scripts with active frames on the stack.  These scripts need to have
     * their profiler state toggled so they behave properly.
     */
    jit::ToggleBaselineProfiling(rt, enabled);

    /* Update lastProfilingFrame to point to the top-most JS jit-frame currently on
     * stack.
     */
    if (cx->jitActivation) {
        // Walk through all activations, and set their lastProfilingFrame appropriately.
        if (enabled) {
            Activation* act = cx->activation();
            void* lastProfilingFrame = GetTopProfilingJitFrame(act);

            jit::JitActivation* jitActivation = cx->jitActivation;
            while (jitActivation) {
                jitActivation->setLastProfilingFrame(lastProfilingFrame);
                jitActivation->setLastProfilingCallSite(nullptr);

                jitActivation = jitActivation->prevJitActivation();
                lastProfilingFrame = GetTopProfilingJitFrame(jitActivation);
            }
        } else {
            jit::JitActivation* jitActivation = cx->jitActivation;
            while (jitActivation) {
                jitActivation->setLastProfilingFrame(nullptr);
                jitActivation->setLastProfilingCallSite(nullptr);
                jitActivation = jitActivation->prevJitActivation();
            }
        }
    }

    // WebAssembly code does not need to be released, but profiling string
    // labels have to be generated so that they are available during async
    // profiling stack iteration.
    for (RealmsIter r(rt, SkipAtoms); !r.done(); r.next())
        r->wasm.ensureProfilingLabels(enabled);
}

/* Lookup the string for the function/script, creating one if necessary */
const char*
GeckoProfilerRuntime::profileString(JSScript* script, JSFunction* maybeFun)
{
    auto locked = strings.lock();
    MOZ_ASSERT(locked->initialized());

    ProfileStringMap::AddPtr s = locked->lookupForAdd(script);

    if (!s) {
        auto str = allocProfileString(script, maybeFun);
        if (!str || !locked->add(s, script, mozilla::Move(str)))
            return nullptr;
    }

    return s->value().get();
}

void
GeckoProfilerRuntime::onScriptFinalized(JSScript* script)
{
    /*
     * This function is called whenever a script is destroyed, regardless of
     * whether profiling has been turned on, so don't invoke a function on an
     * invalid hash set. Also, even if profiling was enabled but then turned
     * off, we still want to remove the string, so no check of enabled() is
     * done.
     */
    auto locked = strings.lock();
    if (!locked->initialized())
        return;
    if (ProfileStringMap::Ptr entry = locked->lookup(script))
        locked->remove(entry);
}

void
GeckoProfilerRuntime::markEvent(const char* event)
{
    MOZ_ASSERT(enabled());
    if (eventMarker_) {
        JS::AutoSuppressGCAnalysis nogc;
        eventMarker_(event);
    }
}

bool
GeckoProfilerThread::enter(JSContext* cx, JSScript* script, JSFunction* maybeFun)
{
    const char* dynamicString = cx->runtime()->geckoProfiler().profileString(script, maybeFun);
    if (dynamicString == nullptr) {
        ReportOutOfMemory(cx);
        return false;
    }

#ifdef DEBUG
    // In debug builds, assert the JS profiling stack frames already on the
    // stack have a non-null pc. Only look at the top frames to avoid quadratic
    // behavior.
    uint32_t sp = profilingStack_->stackPointer;
    if (sp > 0 && sp - 1 < profilingStack_->stackCapacity()) {
        size_t start = (sp > 4) ? sp - 4 : 0;
        for (size_t i = start; i < sp - 1; i++)
            MOZ_ASSERT_IF(profilingStack_->frames[i].isJsFrame(), profilingStack_->frames[i].pc());
    }
#endif

    profilingStack_->pushJsFrame("", dynamicString, script, script->code());
    return true;
}

void
GeckoProfilerThread::exit(JSScript* script, JSFunction* maybeFun)
{
    profilingStack_->pop();

#ifdef DEBUG
    /* Sanity check to make sure push/pop balanced */
    uint32_t sp = profilingStack_->stackPointer;
    if (sp < profilingStack_->stackCapacity()) {
        JSRuntime* rt = script->runtimeFromMainThread();
        const char* dynamicString = rt->geckoProfiler().profileString(script, maybeFun);
        /* Can't fail lookup because we should already be in the set */
        MOZ_ASSERT(dynamicString);

        // Bug 822041
        if (!profilingStack_->frames[sp].isJsFrame()) {
            fprintf(stderr, "--- ABOUT TO FAIL ASSERTION ---\n");
            fprintf(stderr, " frames=%p size=%u/%u\n",
                            (void*) profilingStack_->frames,
                            uint32_t(profilingStack_->stackPointer),
                            profilingStack_->stackCapacity());
            for (int32_t i = sp; i >= 0; i--) {
                ProfilingStackFrame& frame = profilingStack_->frames[i];
                if (frame.isJsFrame())
                    fprintf(stderr, "  [%d] JS %s\n", i, frame.dynamicString());
                else
                    fprintf(stderr, "  [%d] C line %d %s\n", i, frame.line(), frame.dynamicString());
            }
        }

        ProfilingStackFrame& frame = profilingStack_->frames[sp];
        MOZ_ASSERT(frame.isJsFrame());
        MOZ_ASSERT(frame.script() == script);
        MOZ_ASSERT(strcmp((const char*) frame.dynamicString(), dynamicString) == 0);
    }
#endif
}

/*
 * Serializes the script/function pair into a "descriptive string" which is
 * allowed to fail. This function cannot trigger a GC because it could finalize
 * some scripts, resize the hash table of profile strings, and invalidate the
 * AddPtr held while invoking allocProfileString.
 */
UniqueChars
GeckoProfilerRuntime::allocProfileString(JSScript* script, JSFunction* maybeFun)
{
    // Note: this profiler string is regexp-matched by
    // devtools/client/profiler/cleopatra/js/parserWorker.js.

    // Get the function name, if any.
    JSAtom* atom = maybeFun ? maybeFun->displayAtom() : nullptr;

    // Get the script filename, if any, and its length.
    const char* filename = script->filename();
    if (filename == nullptr)
        filename = "<unknown>";
    size_t lenFilename = strlen(filename);

    // Get the line number and its length as a string.
    uint64_t lineno = script->lineno();
    size_t lenLineno = 1;
    for (uint64_t i = lineno; i /= 10; lenLineno++);

    // Determine the required buffer size.
    size_t len = lenFilename + lenLineno + 1; // +1 for the ":" separating them.
    if (atom) {
        len += JS::GetDeflatedUTF8StringLength(atom) + 3; // +3 for the " (" and ")" it adds.
    }

    // Allocate the buffer.
    UniqueChars cstr(js_pod_malloc<char>(len + 1));
    if (!cstr)
        return nullptr;

    // Construct the descriptive string.
    DebugOnly<size_t> ret;
    if (atom) {
        UniqueChars atomStr = StringToNewUTF8CharsZ(nullptr, *atom);
        if (!atomStr)
            return nullptr;

        ret = snprintf(cstr.get(), len + 1, "%s (%s:%" PRIu64 ")", atomStr.get(), filename, lineno);
    } else {
        ret = snprintf(cstr.get(), len + 1, "%s:%" PRIu64, filename, lineno);
    }

    MOZ_ASSERT(ret == len, "Computed length should match actual length!");

    return cstr;
}

void
GeckoProfilerThread::trace(JSTracer* trc)
{
    if (profilingStack_) {
        size_t size = profilingStack_->stackSize();
        for (size_t i = 0; i < size; i++)
            profilingStack_->frames[i].trace(trc);
    }
}

void
GeckoProfilerRuntime::fixupStringsMapAfterMovingGC()
{
    auto locked = strings.lock();
    if (!locked->initialized())
        return;

    for (ProfileStringMap::Enum e(locked.get()); !e.empty(); e.popFront()) {
        JSScript* script = e.front().key();
        if (IsForwarded(script)) {
            script = Forwarded(script);
            e.rekeyFront(script);
        }
    }
}

#ifdef JSGC_HASH_TABLE_CHECKS
void
GeckoProfilerRuntime::checkStringsMapAfterMovingGC()
{
    auto locked = strings.lock();
    if (!locked->initialized())
        return;

    for (auto r = locked->all(); !r.empty(); r.popFront()) {
        JSScript* script = r.front().key();
        CheckGCThingAfterMovingGC(script);
        auto ptr = locked->lookup(script);
        MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
    }
}
#endif

void
ProfilingStackFrame::trace(JSTracer* trc)
{
    if (isJsFrame()) {
        JSScript* s = rawScript();
        TraceNullableRoot(trc, &s, "ProfilingStackFrame script");
        spOrScript = s;
    }
}

GeckoProfilerBaselineOSRMarker::GeckoProfilerBaselineOSRMarker(JSContext* cx, bool hasProfilerFrame
                                                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : profiler(&cx->geckoProfiler())
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (!hasProfilerFrame || !cx->runtime()->geckoProfiler().enabled()) {
        profiler = nullptr;
        return;
    }

    uint32_t sp = profiler->profilingStack_->stackPointer;
    if (sp >= profiler->profilingStack_->stackCapacity()) {
        profiler = nullptr;
        return;
    }

    spBefore_ = sp;
    if (sp == 0)
        return;

    ProfilingStackFrame& frame = profiler->profilingStack_->frames[sp - 1];
    MOZ_ASSERT(frame.kind() == ProfilingStackFrame::Kind::JS_NORMAL);
    frame.setKind(ProfilingStackFrame::Kind::JS_OSR);
}

GeckoProfilerBaselineOSRMarker::~GeckoProfilerBaselineOSRMarker()
{
    if (profiler == nullptr)
        return;

    uint32_t sp = profiler->stackPointer();
    MOZ_ASSERT(spBefore_ == sp);
    if (sp == 0)
        return;

    ProfilingStackFrame& frame = profiler->stack()[sp - 1];
    MOZ_ASSERT(frame.kind() == ProfilingStackFrame::Kind::JS_OSR);
    frame.setKind(ProfilingStackFrame::Kind::JS_NORMAL);
}

JS_PUBLIC_API(JSScript*)
ProfilingStackFrame::script() const
{
    MOZ_ASSERT(isJsFrame());
    auto script = reinterpret_cast<JSScript*>(spOrScript.operator void*());
    if (!script)
        return nullptr;

    // If profiling is supressed then we can't trust the script pointers to be
    // valid as they could be in the process of being moved by a compacting GC
    // (although it's still OK to get the runtime from them).
    JSContext* cx = script->runtimeFromAnyThread()->mainContextFromAnyThread();
    if (!cx->isProfilerSamplingEnabled())
        return nullptr;

    MOZ_ASSERT(!IsForwarded(script));
    return script;
}

JS_FRIEND_API(jsbytecode*)
ProfilingStackFrame::pc() const
{
    MOZ_ASSERT(isJsFrame());
    if (lineOrPcOffset == NullPCOffset)
        return nullptr;

    JSScript* script = this->script();
    return script ? script->offsetToPC(lineOrPcOffset) : nullptr;
}

/* static */ int32_t
ProfilingStackFrame::pcToOffset(JSScript* aScript, jsbytecode* aPc) {
    return aPc ? aScript->pcToOffset(aPc) : NullPCOffset;
}

void
ProfilingStackFrame::setPC(jsbytecode* pc)
{
    MOZ_ASSERT(isJsFrame());
    JSScript* script = this->script();
    MOZ_ASSERT(script); // This should not be called while profiling is suppressed.
    lineOrPcOffset = pcToOffset(script, pc);
}

JS_FRIEND_API(void)
js::SetContextProfilingStack(JSContext* cx, ProfilingStack* profilingStack)
{
    cx->geckoProfiler().setProfilingStack(profilingStack);
}

JS_FRIEND_API(void)
js::EnableContextProfilingStack(JSContext* cx, bool enabled)
{
    cx->runtime()->geckoProfiler().enable(enabled);
}

JS_FRIEND_API(void)
js::RegisterContextProfilingEventMarker(JSContext* cx, void (*fn)(const char*))
{
    MOZ_ASSERT(cx->runtime()->geckoProfiler().enabled());
    cx->runtime()->geckoProfiler().setEventMarker(fn);
}

AutoSuppressProfilerSampling::AutoSuppressProfilerSampling(JSContext* cx
                                                           MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : cx_(cx),
    previouslyEnabled_(cx->isProfilerSamplingEnabled())
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (previouslyEnabled_)
        cx_->disableProfilerSampling();
}

AutoSuppressProfilerSampling::~AutoSuppressProfilerSampling()
{
    if (previouslyEnabled_)
        cx_->enableProfilerSampling();
}
