#include "tests.h"
#include "jsstr.h"

static JSGCCallback oldGCCallback;

static void **checkPointers;
static jsuint checkPointersLength;

static JSBool
TestAboutToBeFinalizedCallback(JSContext *cx, JSGCStatus status)
{
    if (status == JSGC_MARK_END && checkPointers) {
        for (jsuint i = 0; i != checkPointersLength; ++i) {
            void *p = checkPointers[i];
            JS_ASSERT(p);
            if (JS_IsAboutToBeFinalized(cx, p))
                checkPointers[i] = NULL;
        }
    }

    return !oldGCCallback || oldGCCallback(cx, status);
}

BEGIN_TEST(testIsAboutToBeFinalized_bug528645)
{
    jsvalRoot root(cx);

    /*
     * Check various types of GC things against JS_IsAboutToBeFinalized.
     * Make sure to include unit and numeric strings to the set.
     */
    EVAL("var x = 1.1; "
         "[x + 0.1, ''+x, 'a', '42', 'something'.substring(1), "
         "{}, [], new Function('return 10;'), <xml/>];",
         root.addr());

    JSObject *array = JSVAL_TO_OBJECT(root.value());
    JS_ASSERT(JS_IsArrayObject(cx, array));

    JSBool ok = JS_GetArrayLength(cx, array, &checkPointersLength);
    CHECK(ok);

    void **elems = (void **) malloc(sizeof(void *) * checkPointersLength);
    CHECK(elems);

    size_t staticStrings = 0;
    for (jsuint i = 0; i != checkPointersLength; ++i) {
        jsval v;
        ok = JS_GetElement(cx, array, i, &v);
        CHECK(ok);
        JS_ASSERT(JSVAL_IS_GCTHING(v));
        JS_ASSERT(!JSVAL_IS_NULL(v));
        elems[i] = JSVAL_TO_GCTHING(v);
        if (JSString::isStatic(elems[i]))
            ++staticStrings;
    }

    oldGCCallback = JS_SetGCCallback(cx, TestAboutToBeFinalizedCallback);
    checkPointers = elems;
    JS_GC(cx);

    /*
     * All GC things are rooted via the root holding the array containing them
     * and TestAboutToBeFinalizedCallback must keep them as is.
     */
    for (jsuint i = 0; i != checkPointersLength; ++i)
        CHECK(checkPointers[i]);

    root = JSVAL_NULL;
    JS_GC(cx);

    /* Everything is unrooted except unit strings. */
    for (jsuint i = 0; i != checkPointersLength; ++i) {
        void *p = checkPointers[i];
        if (p) {
            CHECK(JSString::isStatic(p));
            CHECK(staticStrings != 0);
            --staticStrings;
        }
    }
    CHECK(staticStrings == 0);

    checkPointers = NULL;
    JS_SetGCCallback(cx, oldGCCallback);
    free(elems);

    return true;
}
END_TEST(testIsAboutToBeFinalized_bug528645)
