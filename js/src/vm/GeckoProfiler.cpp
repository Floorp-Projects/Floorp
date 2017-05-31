/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/GeckoProfiler-inl.h"

#include "mozilla/DebugOnly.h"

#include "jsnum.h"
#include "jsprf.h"
#include "jsscript.h"

#include "jit/BaselineFrame.h"
#include "jit/BaselineJIT.h"
#include "jit/JitcodeMap.h"
#include "jit/JitFrameIterator.h"
#include "jit/JitFrames.h"
#include "vm/StringBuffer.h"

#include "jsgcinlines.h"

using namespace js;

using mozilla::DebugOnly;

GeckoProfiler::GeckoProfiler(JSRuntime* rt)
  : rt(rt),
    strings(mutexid::GeckoProfilerStrings),
    pseudoStack_(nullptr),
    slowAssertions(false),
    enabled_(false),
    eventMarker_(nullptr)
{
    MOZ_ASSERT(rt != nullptr);
}

bool
GeckoProfiler::init()
{
    auto locked = strings.lock();
    if (!locked->init())
        return false;

    return true;
}

void
GeckoProfiler::setProfilingStack(PseudoStack* pseudoStack)
{
    MOZ_ASSERT_IF(pseudoStack_, !enabled());
    MOZ_ASSERT(strings.lock()->initialized());

    pseudoStack_ = pseudoStack;
}

void
GeckoProfiler::setEventMarker(void (*fn)(const char*))
{
    eventMarker_ = fn;
}

bool
GeckoProfiler::enable(bool enabled)
{
    MOZ_ASSERT(installed());

    if (enabled_ == enabled)
        return true;

    // Execution in the runtime must be single threaded if the Gecko profiler
    // is enabled. There is only a single profiler stack in the runtime, from
    // which entries must be added/removed in a LIFO fashion.
    JSContext* cx = rt->activeContextFromOwnThread();
    if (enabled) {
        if (!rt->beginSingleThreadedExecution(cx))
            return false;
    } else {
        rt->endSingleThreadedExecution(cx);
    }

    /*
     * Ensure all future generated code will be instrumented, or that all
     * currently instrumented code is discarded
     */
    ReleaseAllJITCode(rt->defaultFreeOp());

    // This function is called when the Gecko profiler makes a new Sampler
    // (and thus, a new circular buffer). Set all current entries in the
    // JitcodeGlobalTable as expired and reset the buffer generation and lap
    // count.
    if (rt->hasJitRuntime() && rt->jitRuntime()->hasJitcodeGlobalTable())
        rt->jitRuntime()->getJitcodeGlobalTable()->setAllEntriesAsExpired(rt);
    rt->resetProfilerSampleBufferGen();
    rt->resetProfilerSampleBufferLapCount();

    // Ensure that lastProfilingFrame is null for all threads before 'enabled' becomes true.
    for (const CooperatingContext& target : rt->cooperatingContexts()) {
        if (target.context()->jitActivation) {
            target.context()->jitActivation->setLastProfilingFrame(nullptr);
            target.context()->jitActivation->setLastProfilingCallSite(nullptr);
        }
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
    for (const CooperatingContext& target : rt->cooperatingContexts()) {
        if (target.context()->jitActivation) {
            // Walk through all activations, and set their lastProfilingFrame appropriately.
            if (enabled) {
                void* lastProfilingFrame = GetTopProfilingJitFrame(target.context()->jitTop);
                jit::JitActivation* jitActivation = target.context()->jitActivation;
                while (jitActivation) {
                    jitActivation->setLastProfilingFrame(lastProfilingFrame);
                    jitActivation->setLastProfilingCallSite(nullptr);

                    lastProfilingFrame = GetTopProfilingJitFrame(jitActivation->prevJitTop());
                    jitActivation = jitActivation->prevJitActivation();
                }
            } else {
                jit::JitActivation* jitActivation = target.context()->jitActivation;
                while (jitActivation) {
                    jitActivation->setLastProfilingFrame(nullptr);
                    jitActivation->setLastProfilingCallSite(nullptr);
                    jitActivation = jitActivation->prevJitActivation();
                }
            }
        }
    }

    // WebAssembly code does not need to be released, but profiling string
    // labels have to be generated so that they are available during async
    // profiling stack iteration.
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
        c->wasm.ensureProfilingLabels(enabled);

    return true;
}

/* Lookup the string for the function/script, creating one if necessary */
const char*
GeckoProfiler::profileString(JSScript* script, JSFunction* maybeFun)
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
GeckoProfiler::onScriptFinalized(JSScript* script)
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
GeckoProfiler::markEvent(const char* event)
{
    MOZ_ASSERT(enabled());
    if (eventMarker_) {
        JS::AutoSuppressGCAnalysis nogc;
        eventMarker_(event);
    }
}

bool
GeckoProfiler::enter(JSContext* cx, JSScript* script, JSFunction* maybeFun)
{
    const char* dynamicString = profileString(script, maybeFun);
    if (dynamicString == nullptr) {
        ReportOutOfMemory(cx);
        return false;
    }

#ifdef DEBUG
    // In debug builds, assert the JS pseudo frames already on the stack
    // have a non-null pc. Only look at the top frames to avoid quadratic
    // behavior.
    uint32_t sp = pseudoStack_->stackPointer;
    if (sp > 0 && sp - 1 < PseudoStack::MaxEntries) {
        size_t start = (sp > 4) ? sp - 4 : 0;
        for (size_t i = start; i < sp - 1; i++)
            MOZ_ASSERT_IF(pseudoStack_->entries[i].isJs(), pseudoStack_->entries[i].pc());
    }
#endif

    pseudoStack_->pushJsFrame("", dynamicString, script, script->code());
    return true;
}

void
GeckoProfiler::exit(JSScript* script, JSFunction* maybeFun)
{
    pseudoStack_->pop();

#ifdef DEBUG
    /* Sanity check to make sure push/pop balanced */
    uint32_t sp = pseudoStack_->stackPointer;
    if (sp < PseudoStack::MaxEntries) {
        const char* dynamicString = profileString(script, maybeFun);
        /* Can't fail lookup because we should already be in the set */
        MOZ_ASSERT(dynamicString);

        // Bug 822041
        if (!pseudoStack_->entries[sp].isJs()) {
            fprintf(stderr, "--- ABOUT TO FAIL ASSERTION ---\n");
            fprintf(stderr, " entries=%p size=%u/%u\n",
                            (void*) pseudoStack_->entries,
                            uint32_t(pseudoStack_->stackPointer),
                            PseudoStack::MaxEntries);
            for (int32_t i = sp; i >= 0; i--) {
                volatile ProfileEntry& entry = pseudoStack_->entries[i];
                if (entry.isJs())
                    fprintf(stderr, "  [%d] JS %s\n", i, entry.dynamicString());
                else
                    fprintf(stderr, "  [%d] C line %d %s\n", i, entry.line(), entry.dynamicString());
            }
        }

        volatile ProfileEntry& entry = pseudoStack_->entries[sp];
        MOZ_ASSERT(entry.isJs());
        MOZ_ASSERT(entry.script() == script);
        MOZ_ASSERT(strcmp((const char*) entry.dynamicString(), dynamicString) == 0);
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
GeckoProfiler::allocProfileString(JSScript* script, JSFunction* maybeFun)
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
GeckoProfiler::trace(JSTracer* trc) volatile
{
    if (pseudoStack_) {
        size_t size = pseudoStack_->stackSize();
        for (size_t i = 0; i < size; i++)
            pseudoStack_->entries[i].trace(trc);
    }
}

void
GeckoProfiler::fixupStringsMapAfterMovingGC()
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
GeckoProfiler::checkStringsMapAfterMovingGC()
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
ProfileEntry::trace(JSTracer* trc) volatile
{
    if (isJs()) {
        JSScript* s = rawScript();
        TraceNullableRoot(trc, &s, "ProfileEntry script");
        spOrScript = s;
    }
}

GeckoProfilerEntryMarker::GeckoProfilerEntryMarker(JSRuntime* rt,
                                                   JSScript* script
                                                   MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : profiler(&rt->geckoProfiler())
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (!profiler->installed()) {
        profiler = nullptr;
        return;
    }
    spBefore_ = profiler->stackPointer();

    // We want to push a CPP frame so the profiler can correctly order JS and native stacks.
    profiler->pseudoStack_->pushCppFrame(
        "js::RunScript", /* dynamicString = */ nullptr, /* sp = */ this, /* line = */ 0,
        js::ProfileEntry::Category::OTHER, js::ProfileEntry::BEGIN_PSEUDO_JS);

    profiler->pseudoStack_->pushJsFrame(
        "js::RunScript", /* dynamicString = */ nullptr, script, script->code());
}

GeckoProfilerEntryMarker::~GeckoProfilerEntryMarker()
{
    if (profiler == nullptr)
        return;

    profiler->pseudoStack_->pop();    // the JS frame
    profiler->pseudoStack_->pop();    // the BEGIN_PSEUDO_JS frame
    MOZ_ASSERT(spBefore_ == profiler->stackPointer());
}

AutoGeckoProfilerEntry::AutoGeckoProfilerEntry(JSRuntime* rt, const char* label,
                                               ProfileEntry::Category category
                                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : profiler_(&rt->geckoProfiler())
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (!profiler_->installed()) {
        profiler_ = nullptr;
        return;
    }
    spBefore_ = profiler_->stackPointer();

    profiler_->pseudoStack_->pushCppFrame(
        label, /* dynamicString = */ nullptr, /* sp = */ this, /* line = */ 0, category);
}

AutoGeckoProfilerEntry::~AutoGeckoProfilerEntry()
{
    if (!profiler_)
        return;

    profiler_->pseudoStack_->pop();
    MOZ_ASSERT(spBefore_ == profiler_->stackPointer());
}

GeckoProfilerBaselineOSRMarker::GeckoProfilerBaselineOSRMarker(JSRuntime* rt, bool hasProfilerFrame
                                                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : profiler(&rt->geckoProfiler())
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (!hasProfilerFrame || !profiler->enabled()) {
        profiler = nullptr;
        return;
    }

    uint32_t sp = profiler->pseudoStack_->stackPointer;
    if (sp >= PseudoStack::MaxEntries) {
        profiler = nullptr;
        return;
    }

    spBefore_ = sp;
    if (sp == 0)
        return;

    volatile ProfileEntry& entry = profiler->pseudoStack_->entries[sp - 1];
    MOZ_ASSERT(entry.isJs());
    entry.setOSR();
}

GeckoProfilerBaselineOSRMarker::~GeckoProfilerBaselineOSRMarker()
{
    if (profiler == nullptr)
        return;

    uint32_t sp = profiler->stackPointer();
    MOZ_ASSERT(spBefore_ == sp);
    if (sp == 0)
        return;

    volatile ProfileEntry& entry = profiler->stack()[sp - 1];
    MOZ_ASSERT(entry.isJs());
    entry.unsetOSR();
}

JS_PUBLIC_API(JSScript*)
ProfileEntry::script() const volatile
{
    MOZ_ASSERT(isJs());
    auto script = reinterpret_cast<JSScript*>(spOrScript);
    if (!script)
        return nullptr;

    // If profiling is supressed then we can't trust the script pointers to be
    // valid as they could be in the process of being moved by a compacting GC
    // (although it's still OK to get the runtime from them).
    //
    // We only need to check the active context here, as
    // AutoSuppressProfilerSampling prohibits the runtime's active context from
    // being changed while it exists.
    JSContext* cx = script->runtimeFromAnyThread()->activeContext();
    if (!cx->isProfilerSamplingEnabled())
        return nullptr;

    MOZ_ASSERT(!IsForwarded(script));
    return script;
}

JS_FRIEND_API(jsbytecode*)
ProfileEntry::pc() const volatile
{
    MOZ_ASSERT(isJs());
    if (lineOrPcOffset == NullPCOffset)
        return nullptr;

    JSScript* script = this->script();
    return script ? script->offsetToPC(lineOrPcOffset) : nullptr;
}

/* static */ int32_t
ProfileEntry::pcToOffset(JSScript* aScript, jsbytecode* aPc) {
    return aPc ? aScript->pcToOffset(aPc) : NullPCOffset;
}

void
ProfileEntry::setPC(jsbytecode* pc) volatile
{
    MOZ_ASSERT(isJs());
    JSScript* script = this->script();
    MOZ_ASSERT(script); // This should not be called while profiling is suppressed.
    lineOrPcOffset = pcToOffset(script, pc);
}

JS_FRIEND_API(void)
js::SetContextProfilingStack(JSContext* cx, PseudoStack* pseudoStack)
{
    cx->runtime()->geckoProfiler().setProfilingStack(pseudoStack);
}

JS_FRIEND_API(void)
js::EnableContextProfilingStack(JSContext* cx, bool enabled)
{
    if (!cx->runtime()->geckoProfiler().enable(enabled))
        MOZ_CRASH("Execution in this runtime should already be single threaded");
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
    previouslyEnabled_(cx->isProfilerSamplingEnabled()),
    prohibitContextChange_(cx->runtime())
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

void*
js::GetTopProfilingJitFrame(uint8_t* exitFramePtr)
{
    // For null exitFrame, there is no previous exit frame, just return.
    if (!exitFramePtr)
        return nullptr;

    jit::JitProfilingFrameIterator iter(exitFramePtr);
    MOZ_ASSERT(!iter.done());
    return iter.fp();
}
