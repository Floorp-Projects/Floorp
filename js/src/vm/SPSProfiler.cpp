/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "jsnum.h"
#include "jsscript.h"

#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"

#include "vm/SPSProfiler.h"
#include "vm/StringBuffer.h"

#include "ion/BaselineJIT.h"

#include "jsscriptinlines.h"

using namespace js;

using mozilla::DebugOnly;

SPSProfiler::SPSProfiler(JSRuntime *rt)
  : rt(rt),
    stack_(NULL),
    size_(NULL),
    max_(0),
    slowAssertions(false),
    enabled_(false)
{
    JS_ASSERT(rt != NULL);
}

SPSProfiler::~SPSProfiler()
{
    if (strings.initialized()) {
        for (ProfileStringMap::Enum e(strings); !e.empty(); e.popFront())
            js_free(const_cast<char *>(e.front().value));
    }
#ifdef JS_METHODJIT
    if (jminfo.initialized()) {
        for (JITInfoMap::Enum e(jminfo); !e.empty(); e.popFront())
            js_delete(e.front().value);
    }
#endif
}

void
SPSProfiler::setProfilingStack(ProfileEntry *stack, uint32_t *size, uint32_t max)
{
    JS_ASSERT_IF(size_ && *size_ != 0, !enabled());
    if (!strings.initialized())
        strings.init();
    stack_ = stack;
    size_  = size;
    max_   = max;
}

void
SPSProfiler::enable(bool enabled)
{
    JS_ASSERT(installed());
    enabled_ = enabled;
    /*
     * Ensure all future generated code will be instrumented, or that all
     * currently instrumented code is discarded
     */
    ReleaseAllJITCode(rt->defaultFreeOp());

#ifdef JS_ION
    /* Toggle SPS-related jumps on baseline jitcode.
     * The call to |ReleaseAllJITCode| above will release most baseline jitcode, but not
     * jitcode for scripts with active frames on the stack.  These scripts need to have
     * their profiler state toggled so they behave properly.
     */
    ion::ToggleBaselineSPS(rt, enabled);
#endif
}

/* Lookup the string for the function/script, creating one if necessary */
const char*
SPSProfiler::profileString(JSContext *cx, RawScript script, RawFunction maybeFun)
{
    JS_ASSERT(strings.initialized());
    ProfileStringMap::AddPtr s = strings.lookupForAdd(script);
    if (s)
        return s->value;
    const char *str = allocProfileString(cx, script, maybeFun);
    if (str == NULL)
        return NULL;
    if (!strings.add(s, script, str)) {
        js_free(const_cast<char *>(str));
        return NULL;
    }
    return str;
}

void
SPSProfiler::onScriptFinalized(RawScript script)
{
    /*
     * This function is called whenever a script is destroyed, regardless of
     * whether profiling has been turned on, so don't invoke a function on an
     * invalid hash set. Also, even if profiling was enabled but then turned
     * off, we still want to remove the string, so no check of enabled() is
     * done.
     */
    if (!strings.initialized())
        return;
    if (ProfileStringMap::Ptr entry = strings.lookup(script)) {
        const char *tofree = entry->value;
        strings.remove(entry);
        js_free(const_cast<char *>(tofree));
    }
}

bool
SPSProfiler::enter(JSContext *cx, RawScript script, RawFunction maybeFun)
{
    const char *str = profileString(cx, script, maybeFun);
    if (str == NULL)
        return false;

    JS_ASSERT_IF(*size_ > 0 && *size_ - 1 < max_ && stack_[*size_ - 1].js(),
                 stack_[*size_ - 1].pc() != NULL);
    push(str, NULL, script, script->code);
    return true;
}

void
SPSProfiler::exit(JSContext *cx, RawScript script, RawFunction maybeFun)
{
    pop();

#ifdef DEBUG
    /* Sanity check to make sure push/pop balanced */
    if (*size_ < max_) {
        const char *str = profileString(cx, script, maybeFun);
        /* Can't fail lookup because we should already be in the set */
        JS_ASSERT(str != NULL);

        // Bug 822041
        if (!stack_[*size_].js()) {
            fprintf(stderr, "--- ABOUT TO FAIL ASSERTION ---\n");
            fprintf(stderr, " stack=%p size=%d/%d\n", (void*) stack_, *size_, max_);
            for (int32_t i = *size_; i >= 0; i--) {
                if (stack_[i].js())
                    fprintf(stderr, "  [%d] JS %s\n", i, stack_[i].label());
                else
                    fprintf(stderr, "  [%d] C line %d %s\n", i, stack_[i].line(), stack_[i].label());
            }
        }

        JS_ASSERT(stack_[*size_].js());
        JS_ASSERT(stack_[*size_].script() == script);
        JS_ASSERT(strcmp((const char*) stack_[*size_].label(), str) == 0);
        stack_[*size_].setLabel(NULL);
        stack_[*size_].setPC(NULL);
    }
#endif
}

void
SPSProfiler::enterNative(const char *string, void *sp)
{
    /* these operations cannot be re-ordered, so volatile-ize operations */
    volatile ProfileEntry *stack = stack_;
    volatile uint32_t *size = size_;
    uint32_t current = *size;

    JS_ASSERT(enabled());
    if (current < max_) {
        stack[current].setLabel(string);
        stack[current].setStackAddress(sp);
        stack[current].setScript(NULL);
        stack[current].setLine(0);
    }
    *size = current + 1;
}

void
SPSProfiler::push(const char *string, void *sp, RawScript script, jsbytecode *pc)
{
    /* these operations cannot be re-ordered, so volatile-ize operations */
    volatile ProfileEntry *stack = stack_;
    volatile uint32_t *size = size_;
    uint32_t current = *size;

    JS_ASSERT(enabled());
    if (current < max_) {
        stack[current].setLabel(string);
        stack[current].setStackAddress(sp);
        stack[current].setScript(script);
        stack[current].setPC(pc);
    }
    *size = current + 1;
}

void
SPSProfiler::pop()
{
    JS_ASSERT(installed());
    (*size_)--;
    JS_ASSERT(*(int*)size_ >= 0);
}

/*
 * Serializes the script/function pair into a "descriptive string" which is
 * allowed to fail. This function cannot trigger a GC because it could finalize
 * some scripts, resize the hash table of profile strings, and invalidate the
 * AddPtr held while invoking allocProfileString.
 */
const char*
SPSProfiler::allocProfileString(JSContext *cx, RawScript script, RawFunction maybeFun)
{
    DebugOnly<uint64_t> gcBefore = cx->runtime->gcNumber;
    StringBuffer buf(cx);
    bool hasAtom = maybeFun != NULL && maybeFun->displayAtom() != NULL;
    if (hasAtom) {
        if (!buf.append(maybeFun->displayAtom()))
            return NULL;
        if (!buf.append(" ("))
            return NULL;
    }
    if (script->filename()) {
        if (!buf.appendInflated(script->filename(), strlen(script->filename())))
            return NULL;
    } else if (!buf.append("<unknown>")) {
        return NULL;
    }
    if (!buf.append(":"))
        return NULL;
    if (!NumberValueToStringBuffer(cx, NumberValue(script->lineno), buf))
        return NULL;
    if (hasAtom && !buf.append(")"))
        return NULL;

    size_t len = buf.length();
    char *cstr = js_pod_malloc<char>(len + 1);
    if (cstr == NULL)
        return NULL;

    const jschar *ptr = buf.begin();
    for (size_t i = 0; i < len; i++)
        cstr[i] = ptr[i];
    cstr[len] = 0;

    JS_ASSERT(gcBefore == cx->runtime->gcNumber);
    return cstr;
}

#ifdef JS_METHODJIT
typedef SPSProfiler::JMChunkInfo JMChunkInfo;

JMChunkInfo::JMChunkInfo(mjit::JSActiveFrame *frame,
                         mjit::PCLengthEntry *pcLengths,
                         mjit::JITChunk *chunk)
  : mainStart(frame->mainCodeStart),
    mainEnd(frame->mainCodeEnd),
    stubStart(frame->stubCodeStart),
    stubEnd(frame->stubCodeEnd),
    pcLengths(pcLengths),
    chunk(chunk)
{}

jsbytecode*
SPSProfiler::ipToPC(RawScript script, size_t ip)
{
    if (!jminfo.initialized())
        return NULL;

    JITInfoMap::Ptr ptr = jminfo.lookup(script);
    if (!ptr)
        return NULL;
    JMScriptInfo *info = ptr->value;

    /* First check if this ip is in any of the ICs compiled for the script */
    for (unsigned i = 0; i < info->ics.length(); i++) {
        ICInfo &ic = info->ics[i];
        if (ic.base <= ip && ip < ic.base + ic.size)
            return ic.pc;
    }

    /* Otherwise if it's not in any of the chunks, then we can't find it */
    for (unsigned i = 0; i < info->chunks.length(); i++) {
        jsbytecode *pc = info->chunks[i].convert(script, ip);
        if (pc != NULL)
            return pc;
    }

    return NULL;
}

jsbytecode*
JMChunkInfo::convert(RawScript script, size_t ip)
{
    if (mainStart <= ip && ip < mainEnd) {
        size_t offset = 0;
        uint32_t i;
        for (i = 0; i < script->length - 1; i++) {
            offset += (uint32_t) pcLengths[i].inlineLength;
            if (mainStart + offset > ip)
                break;
        }
        return &script->code[i];
    } else if (stubStart <= ip && ip < stubEnd) {
        size_t offset = 0;
        uint32_t i;
        for (i = 0; i < script->length - 1; i++) {
            offset += (uint32_t) pcLengths[i].stubLength;
            if (stubStart + offset > ip)
                break;
        }
        return &script->code[i];
    }

    return NULL;
}

bool
SPSProfiler::registerMJITCode(mjit::JITChunk *chunk,
                              mjit::JSActiveFrame *outerFrame,
                              mjit::JSActiveFrame **inlineFrames)
{
    if (!jminfo.initialized() && !jminfo.init(100))
        return false;

    JS_ASSERT(chunk->pcLengths != NULL);

    JMChunkInfo *info = registerScript(outerFrame, chunk->pcLengths, chunk);
    if (!info)
        return false;

    /*
     * The pcLengths array has entries for both the outerFrame's script and also
     * all of the inlineFrames' scripts. The layout is something like:
     *
     *    [ outerFrame info ] [ inline frame 1 ] [ inline frame 2 ] ...
     *
     * This local pcLengths pointer tracks the position of each inline frame's
     * pcLengths array. Each section of the array has length script->length for
     * the corresponding script for that frame.
     */
    mjit::PCLengthEntry *pcLengths = chunk->pcLengths + outerFrame->script->length;
    for (unsigned i = 0; i < chunk->nInlineFrames; i++) {
        JMChunkInfo *child = registerScript(inlineFrames[i], pcLengths, chunk);
        if (!child)
            return false;
        /*
         * When JM tells us about new code, each inline ActiveFrame only has the
         * start/end listed relative to the start of the main instruction
         * streams. This is corrected here so the addresses listed on the
         * JMChunkInfo structure are absolute and can be tested directly.
         */
        child->mainStart += info->mainStart;
        child->mainEnd   += info->mainStart;
        child->stubStart += info->stubStart;
        child->stubEnd   += info->stubStart;

        pcLengths += inlineFrames[i]->script->length;
    }

    return true;
}

JMChunkInfo*
SPSProfiler::registerScript(mjit::JSActiveFrame *frame,
                            mjit::PCLengthEntry *entries,
                            mjit::JITChunk *chunk)
{
    /*
     * An inlined script could possibly be compiled elsewhere as not having been
     * inlined, so each JSScript* must be associated with a list of chunks
     * instead of just one. Also, our script may already be in the map.
     */
    JITInfoMap::AddPtr ptr = jminfo.lookupForAdd(frame->script);
    JMScriptInfo *info;
    if (ptr) {
        info = ptr->value;
        JS_ASSERT(info->chunks.length() > 0);
    } else {
        info = rt->new_<JMScriptInfo>();
        if (info == NULL || !jminfo.add(ptr, frame->script, info))
            return NULL;
    }
    if (!info->chunks.append(JMChunkInfo(frame, entries, chunk)))
        return NULL;
    return info->chunks.end() - 1;
}

bool
SPSProfiler::registerICCode(mjit::JITChunk *chunk,
                            RawScript script, jsbytecode *pc,
                            void *base, size_t size)
{
    JS_ASSERT(jminfo.initialized());
    JITInfoMap::Ptr ptr = jminfo.lookup(script);
    JS_ASSERT(ptr);
    return ptr->value->ics.append(ICInfo(base, size, pc));
}

void
SPSProfiler::discardMJITCode(mjit::JITScript *jscr,
                             mjit::JITChunk *chunk, void* address)
{
    if (!jminfo.initialized())
        return;

    unregisterScript(jscr->script, chunk);
    for (unsigned i = 0; i < chunk->nInlineFrames; i++)
        unregisterScript(chunk->inlineFrames()[i].fun->nonLazyScript(), chunk);
}

void
SPSProfiler::unregisterScript(RawScript script, mjit::JITChunk *chunk)
{
    JITInfoMap::Ptr ptr = jminfo.lookup(script);
    if (!ptr)
        return;
    JMScriptInfo *info = ptr->value;
    for (unsigned i = 0; i < info->chunks.length(); i++) {
        if (info->chunks[i].chunk == chunk) {
            info->chunks.erase(&info->chunks[i]);
            break;
        }
    }
    if (info->chunks.length() == 0) {
        jminfo.remove(ptr);
        js_delete(info);
    }
}
#endif

SPSEntryMarker::SPSEntryMarker(JSRuntime *rt
                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
    : profiler(&rt->spsProfiler)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (!profiler->enabled()) {
        profiler = NULL;
        return;
    }
    size_before = *profiler->size_;
    profiler->push("js::RunScript", this, NULL, NULL);
}

SPSEntryMarker::~SPSEntryMarker()
{
    if (profiler != NULL) {
        profiler->pop();
        JS_ASSERT(size_before == *profiler->size_);
    }
}

JS_FRIEND_API(jsbytecode*)
ProfileEntry::pc() volatile {
    JS_ASSERT_IF(idx != NullPCIndex, idx >= 0 && uint32_t(idx) < script()->length);
    return idx == NullPCIndex ? NULL : script()->code + idx;
}

JS_FRIEND_API(void)
ProfileEntry::setPC(jsbytecode *pc) volatile {
    JS_ASSERT_IF(pc != NULL, script()->code <= pc &&
                             pc < script()->code + script()->length);
    idx = pc == NULL ? NullPCIndex : pc - script()->code;
}
