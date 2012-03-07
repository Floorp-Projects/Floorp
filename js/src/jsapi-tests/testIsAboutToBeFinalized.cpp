/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"
#include "jsstr.h"

static JSGCCallback oldGCCallback;

static void **checkPointers;
static unsigned checkPointersLength;
static size_t checkPointersStaticStrings;

static JSBool
TestAboutToBeFinalizedCallback(JSContext *cx, JSGCStatus status)
{
    if (status == JSGC_MARK_END && checkPointers) {
        for (unsigned i = 0; i != checkPointersLength; ++i) {
            void *p = checkPointers[i];
            JS_ASSERT(p);
            if (JS_IsAboutToBeFinalized(p))
                checkPointers[i] = NULL;
        }
    }

    return !oldGCCallback || oldGCCallback(cx, status);
}

/*
 * NativeFrameCleaner write to sink so a compiler nwould not be able to optimze out
 * the buffer memset there.
 */
volatile void *ptrSink;

static JS_NEVER_INLINE void
NativeFrameCleaner()
{
    char buffer[1 << 16];
    memset(buffer, 0, sizeof buffer);
    ptrSink = buffer;
}

BEGIN_TEST(testIsAboutToBeFinalized_bug528645)
{
    /*
     * Due to the conservative GC we use separated never-inline function to
     * test rooted elements.
     */
    CHECK(createAndTestRooted());
    NativeFrameCleaner();

    JS_GC(cx);

    /* Everything is unrooted except unit strings. */
    for (unsigned i = 0; i != checkPointersLength; ++i) {
        void *p = checkPointers[i];
        if (p) {
            CHECK(JSString::isStatic(p));
            CHECK(checkPointersStaticStrings != 0);
            --checkPointersStaticStrings;
        }
    }
    CHECK_EQUAL(checkPointersStaticStrings, 0);

    free(checkPointers);
    checkPointers = NULL;
    JS_SetGCCallback(cx, oldGCCallback);

    return true;
}

JS_NEVER_INLINE bool createAndTestRooted();

END_TEST(testIsAboutToBeFinalized_bug528645)

JS_NEVER_INLINE bool
cls_testIsAboutToBeFinalized_bug528645::createAndTestRooted()
{
    jsvalRoot root(cx);

    /*
     * Check various types of GC things against JS_IsAboutToBeFinalized.
     * Make sure to include unit and numeric strings to the set.
     */
    EVAL("var x = 1.1; "
         "[''+x, 'a', '123456789', 'something'.substring(1), "
         "{}, [], new Function('return 10;'), <xml/>];",
         root.addr());

    JSObject *array = JSVAL_TO_OBJECT(root.value());
    JS_ASSERT(JS_IsArrayObject(cx, array));

    JSBool ok = JS_GetArrayLength(cx, array, &checkPointersLength);
    CHECK(ok);

    checkPointers = (void **) malloc(sizeof(void *) * checkPointersLength);
    CHECK(checkPointers);

    checkPointersStaticStrings = 0;
    for (unsigned i = 0; i != checkPointersLength; ++i) {
        jsval v;
        ok = JS_GetElement(cx, array, i, &v);
        CHECK(ok);
        JS_ASSERT(JSVAL_IS_GCTHING(v));
        JS_ASSERT(!JSVAL_IS_NULL(v));
        checkPointers[i] = JSVAL_TO_GCTHING(v);
        if (JSString::isStatic(checkPointers[i]))
            ++checkPointersStaticStrings;
    }

    oldGCCallback = JS_SetGCCallback(cx, TestAboutToBeFinalizedCallback);
    JS_GC(cx);

    /*
     * All GC things are rooted via the root holding the array containing them
     * and TestAboutToBeFinalizedCallback must keep them as is.
     */
    for (unsigned i = 0; i != checkPointersLength; ++i)
        CHECK(checkPointers[i]);

    /*
     * Overwrite the registers and stack with new GC things to avoid false
     * positives with the finalization test.
     */
    EVAL("[]", root.addr());

    array = JSVAL_TO_OBJECT(root.value());
    JS_ASSERT(JS_IsArrayObject(cx, array));

    uint32_t tmp;
    CHECK(JS_GetArrayLength(cx, array, &tmp));
    CHECK(ok);

    return true;
}

