/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Igor Bukanov
 */

#include "tests.h"
#include "jsgcchunk.h"
#include "jscntxt.h"

/* We allow to allocate 2 (system/user) chunks. */

/* XXX: using pool[0] and pool[1] is a hack;  bug 669123 will fix this. */

class CustomGCChunkAllocator: public js::GCChunkAllocator {
  public:
    CustomGCChunkAllocator() { pool[0] = NULL; pool[1] = NULL; }
    void *pool[2];
    
  private:

    virtual void *doAlloc() {
        if (!pool[0] && !pool[1])
            return NULL;
        void *chunk = NULL;
        if (pool[0]) {
            chunk = pool[0];
            pool[0] = NULL;
        } else {
            chunk = pool[1];
            pool[1] = NULL;
        }
        return chunk;
    }
        
    virtual void doFree(void *chunk) {
        JS_ASSERT(!pool[0] || !pool[1]);
        if (!pool[0]) {
            pool[0] = chunk;
        } else {
            pool[1] = chunk;
        }
    }
};

static CustomGCChunkAllocator customGCChunkAllocator;

static unsigned errorCount = 0;

static void
ErrorCounter(JSContext *cx, const char *message, JSErrorReport *report)
{
    ++errorCount;
}

BEGIN_TEST(testGCChunkAlloc)
{
    JS_SetErrorReporter(cx, ErrorCounter);

    jsvalRoot root(cx);

    /*
     * We loop until out-of-memory happens during the chunk allocation. But
     * we have to disable the jit since it cannot tolerate OOM during the
     * chunk allocation.
     */
    JS_ToggleOptions(cx, JSOPTION_JIT);

    static const char source[] =
        "var max = 0; (function() {"
        "    var array = [];"
        "    for (; ; ++max)"
        "        array.push({});"
        "})();";
    JSBool ok = JS_EvaluateScript(cx, global, source, strlen(source), "", 1,
                                  root.addr());

    /* Check that we get OOM. */
    CHECK(!ok);
    CHECK(!JS_IsExceptionPending(cx));
    CHECK_EQUAL(errorCount, 1);
    CHECK(!customGCChunkAllocator.pool[0]);
    CHECK(!customGCChunkAllocator.pool[1]);
    JS_GC(cx);
    JS_ToggleOptions(cx, JSOPTION_JIT);
    EVAL("(function() {"
         "    var array = [];"
         "    for (var i = max >> 1; i != 0;) {"
         "        --i;"
         "        array.push({});"
         "    }"
         "})();", root.addr());
    CHECK_EQUAL(errorCount, 1);
    return true;
}

virtual JSRuntime * createRuntime() {
    /*
     * To test failure of chunk allocation allow to use GC twice the memory
     * the single chunk contains.
     */
    JSRuntime *rt = JS_NewRuntime(2 * js::GC_CHUNK_SIZE);
    if (!rt)
        return NULL;

    customGCChunkAllocator.pool[0] = js::AllocGCChunk();
    customGCChunkAllocator.pool[1] = js::AllocGCChunk();
    JS_ASSERT(customGCChunkAllocator.pool[0]);
    JS_ASSERT(customGCChunkAllocator.pool[1]);

    rt->setCustomGCChunkAllocator(&customGCChunkAllocator);
    return rt;
}

virtual void destroyRuntime() {
    JS_DestroyRuntime(rt);

    /* We should get the initial chunk back at this point. */
    JS_ASSERT(customGCChunkAllocator.pool[0]);
    JS_ASSERT(customGCChunkAllocator.pool[1]);
    js::FreeGCChunk(customGCChunkAllocator.pool[0]);
    js::FreeGCChunk(customGCChunkAllocator.pool[1]);
    customGCChunkAllocator.pool[0] = NULL;
    customGCChunkAllocator.pool[1] = NULL;
}

END_TEST(testGCChunkAlloc)
