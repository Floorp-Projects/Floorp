/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Runtime-inl.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Util.h"

#include <locale.h>
#include <string.h>

#include "jsatom.h"
#include "jsgc.h"
#include "jsmath.h"
#include "jsobj.h"
#include "jsscript.h"

#include "js/MemoryMetrics.h"
#include "yarr/BumpPointerAllocator.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

using mozilla::PodZero;

void
NewObjectCache::clearNurseryObjects(JSRuntime *rt)
{
    for (unsigned i = 0; i < mozilla::ArrayLength(entries); ++i) {
        Entry &e = entries[i];
        JSObject *obj = reinterpret_cast<JSObject *>(&e.templateObject);
        if (IsInsideNursery(rt, e.key) ||
            IsInsideNursery(rt, obj->slots) ||
            IsInsideNursery(rt, obj->elements))
        {
            PodZero(&e);
        }
    }
}

void
JSRuntime::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::RuntimeSizes *rtSizes)
{
    // Several tables in the runtime enumerated below can be used off thread.
    AutoLockForExclusiveAccess lock(this);

    rtSizes->object = mallocSizeOf(this);

    rtSizes->atomsTable = atoms.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->contexts = 0;
    for (ContextIter acx(this); !acx.done(); acx.next())
        rtSizes->contexts += acx->sizeOfIncludingThis(mallocSizeOf);

    rtSizes->dtoa = mallocSizeOf(mainThread.dtoaState);

    rtSizes->temporary = tempLifoAlloc.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->code = JS::CodeSizes();
    if (execAlloc_)
        execAlloc_->sizeOfCode(&rtSizes->code);

    rtSizes->regexpData = bumpAlloc_ ? bumpAlloc_->sizeOfNonHeapData() : 0;

    rtSizes->interpreterStack = interpreterStack_.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->gcMarker = gcMarker.sizeOfExcludingThis(mallocSizeOf);

    rtSizes->mathCache = mathCache_ ? mathCache_->sizeOfIncludingThis(mallocSizeOf) : 0;

    rtSizes->scriptData = scriptDataTable.sizeOfExcludingThis(mallocSizeOf);
    for (ScriptDataTable::Range r = scriptDataTable.all(); !r.empty(); r.popFront())
        rtSizes->scriptData += mallocSizeOf(r.front());
}

void
JSRuntime::triggerOperationCallback()
{
    AutoLockForOperationCallback lock(this);

    /*
     * Invalidate ionTop to trigger its over-recursion check. Note this must be
     * set before interrupt, to avoid racing with js_InvokeOperationCallback,
     * into a weird state where interrupt is stuck at 0 but ionStackLimit is
     * MAXADDR.
     */
    mainThread.setIonStackLimit(-1);

    /*
     * Use JS_ATOMIC_SET in the hope that it ensures the write will become
     * immediately visible to other processors polling the flag.
     */
    JS_ATOMIC_SET(&interrupt, 1);

#ifdef JS_ION
    /* asm.js code uses a separate mechanism to halt running code. */
    TriggerOperationCallbackForAsmJSCode(this);
#endif
}

void
JSRuntime::setJitHardening(bool enabled)
{
    jitHardening = enabled;
    if (execAlloc_)
        execAlloc_->setRandomize(enabled);
}

JSC::ExecutableAllocator *
JSRuntime::createExecutableAllocator(JSContext *cx)
{
    JS_ASSERT(!execAlloc_);
    JS_ASSERT(cx->runtime() == this);

    JSC::AllocationBehavior randomize =
        jitHardening ? JSC::AllocationCanRandomize : JSC::AllocationDeterministic;
    execAlloc_ = js_new<JSC::ExecutableAllocator>(randomize);
    if (!execAlloc_)
        js_ReportOutOfMemory(cx);
    return execAlloc_;
}

WTF::BumpPointerAllocator *
JSRuntime::createBumpPointerAllocator(JSContext *cx)
{
    JS_ASSERT(!bumpAlloc_);
    JS_ASSERT(cx->runtime() == this);

    bumpAlloc_ = js_new<WTF::BumpPointerAllocator>();
    if (!bumpAlloc_)
        js_ReportOutOfMemory(cx);
    return bumpAlloc_;
}

MathCache *
JSRuntime::createMathCache(JSContext *cx)
{
    JS_ASSERT(!mathCache_);
    JS_ASSERT(cx->runtime() == this);

    MathCache *newMathCache = js_new<MathCache>();
    if (!newMathCache) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    mathCache_ = newMathCache;
    return mathCache_;
}

bool
JSRuntime::setDefaultLocale(const char *locale)
{
    if (!locale)
        return false;
    resetDefaultLocale();
    defaultLocale = JS_strdup(this, locale);
    return defaultLocale != NULL;
}

void
JSRuntime::resetDefaultLocale()
{
    js_free(defaultLocale);
    defaultLocale = NULL;
}

const char *
JSRuntime::getDefaultLocale()
{
    if (defaultLocale)
        return defaultLocale;

    char *locale, *lang, *p;
#ifdef HAVE_SETLOCALE
    locale = setlocale(LC_ALL, NULL);
#else
    locale = getenv("LANG");
#endif
    // convert to a well-formed BCP 47 language tag
    if (!locale || !strcmp(locale, "C"))
        locale = const_cast<char*>("und");
    lang = JS_strdup(this, locale);
    if (!lang)
        return NULL;
    if ((p = strchr(lang, '.')))
        *p = '\0';
    while ((p = strchr(lang, '_')))
        *p = '-';

    defaultLocale = lang;
    return defaultLocale;
}

void
JSRuntime::setGCMaxMallocBytes(size_t value)
{
    /*
     * For compatibility treat any value that exceeds PTRDIFF_T_MAX to
     * mean that value.
     */
    gcMaxMallocBytes = (ptrdiff_t(value) >= 0) ? value : size_t(-1) >> 1;
    resetGCMallocBytes();
    for (ZonesIter zone(this); !zone.done(); zone.next())
        zone->setGCMaxMallocBytes(value);
}

void
JSRuntime::updateMallocCounter(size_t nbytes)
{
    updateMallocCounter(NULL, nbytes);
}

void
JSRuntime::updateMallocCounter(JS::Zone *zone, size_t nbytes)
{
    /* We tolerate any thread races when updating gcMallocBytes. */
    ptrdiff_t oldCount = gcMallocBytes;
    ptrdiff_t newCount = oldCount - ptrdiff_t(nbytes);
    gcMallocBytes = newCount;
    if (JS_UNLIKELY(newCount <= 0 && oldCount > 0))
        onTooMuchMalloc();
    else if (zone)
        zone->updateMallocCounter(nbytes);
}

JS_FRIEND_API(void)
JSRuntime::onTooMuchMalloc()
{
    TriggerGC(this, JS::gcreason::TOO_MUCH_MALLOC);
}

JS_FRIEND_API(void *)
JSRuntime::onOutOfMemory(void *p, size_t nbytes)
{
    return onOutOfMemory(p, nbytes, NULL);
}

JS_FRIEND_API(void *)
JSRuntime::onOutOfMemory(void *p, size_t nbytes, JSContext *cx)
{
    if (isHeapBusy())
        return NULL;

    /*
     * Retry when we are done with the background sweeping and have stopped
     * all the allocations and released the empty GC chunks.
     */
    JS::ShrinkGCBuffers(this);
    gcHelperThread.waitBackgroundSweepOrAllocEnd();
    if (!p)
        p = js_malloc(nbytes);
    else if (p == reinterpret_cast<void *>(1))
        p = js_calloc(nbytes);
    else
      p = js_realloc(p, nbytes);
    if (p)
        return p;
    if (cx)
        js_ReportOutOfMemory(cx);
    return NULL;
}

