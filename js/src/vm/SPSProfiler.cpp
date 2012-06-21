/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsnum.h"

#include "vm/SPSProfiler.h"
#include "vm/StringBuffer.h"

using namespace js;

SPSProfiler::~SPSProfiler()
{
    if (strings.initialized()) {
        for (ProfileStringMap::Enum e(strings); !e.empty(); e.popFront())
            js_free((void*) e.front().value);
    }
}

void
SPSProfiler::setProfilingStack(ProfileEntry *stack, uint32_t *size, uint32_t max)
{
    if (!strings.initialized())
        strings.init(max);
    stack_ = stack;
    size_  = size;
    max_   = max;
}

/* Lookup the string for the function/script, creating one if necessary */
const char*
SPSProfiler::profileString(JSContext *cx, JSScript *script, JSFunction *maybeFun)
{
    JS_ASSERT(enabled());
    JS_ASSERT(strings.initialized());
    ProfileStringMap::AddPtr s = strings.lookupForAdd(script);
    if (s)
        return s->value;
    const char *str = allocProfileString(cx, script, maybeFun);
    if (str == NULL)
        return NULL;
    if (!strings.add(s, script, str)) {
        js_free((void*) str);
        return NULL;
    }
    return str;
}

void
SPSProfiler::onScriptFinalized(JSScript *script)
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
        js_free((void*) tofree);
    }
}

bool
SPSProfiler::enter(JSContext *cx, JSScript *script, JSFunction *maybeFun)
{
    JS_ASSERT(enabled());
    const char *str = profileString(cx, script, maybeFun);
    if (str == NULL)
        return false;

    if (*size_ < max_) {
        stack_[*size_].string = str;
        stack_[*size_].sp     = NULL;
    }
    (*size_)++;
    return true;
}

void
SPSProfiler::exit(JSContext *cx, JSScript *script, JSFunction *maybeFun)
{
    JS_ASSERT(enabled());
    (*size_)--;
    JS_ASSERT(*(int*)size_ >= 0);

#ifdef DEBUG
    /* Sanity check to make sure push/pop balanced */
    if (*size_ < max_) {
        const char *str = profileString(cx, script, maybeFun);
        /* Can't fail lookup because we should already be in the set */
        JS_ASSERT(str != NULL);
        JS_ASSERT(strcmp((const char*) stack_[*size_].string, str) == 0);
        stack_[*size_].string = NULL;
        stack_[*size_].sp     = NULL;
    }
#endif
}

/*
 * Serializes the script/function pair into a "descriptive string" which is
 * allowed to fail. This function cannot trigger a GC because it could finalize
 * some scripts, resize the hash table of profile strings, and invalidate the
 * AddPtr held while invoking allocProfileString.
 */
const char*
SPSProfiler::allocProfileString(JSContext *cx, JSScript *script, JSFunction *maybeFun)
{
    DebugOnly<uint64_t> gcBefore = cx->runtime->gcNumber;
    StringBuffer buf(cx);
    bool hasAtom = maybeFun != NULL && maybeFun->atom != NULL;
    if (hasAtom) {
        if (!buf.append(maybeFun->atom))
            return NULL;
        if (!buf.append(" ("))
            return NULL;
    }
    if (script->filename) {
        if (!buf.appendInflated(script->filename, strlen(script->filename)))
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
    char *cstr = (char*) js_malloc(len + 1);
    if (cstr == NULL)
        return NULL;

    const jschar *ptr = buf.begin();
    for (size_t i = 0; i < len; i++)
        cstr[i] = ptr[i];
    cstr[len] = 0;

    JS_ASSERT(gcBefore == cx->runtime->gcNumber);
    return cstr;
}

SPSEntryMarker::SPSEntryMarker(JSRuntime *rt) : profiler(&rt->spsProfiler), pushed(false)
{
    if (!profiler->enabled())
        return;
    uint32_t *size = profiler->size_;
    size_before = *size;
    if (*size < profiler->max_) {
        profiler->stack_[*size].string = "js::RunScript";
        profiler->stack_[*size].sp = this;
    }
    (*size)++;
    pushed = true;
}

SPSEntryMarker::~SPSEntryMarker()
{
    if (!pushed || !profiler->enabled())
        return;
    (*profiler->size_)--;
    JS_ASSERT(*profiler->size_ == size_before);
}
