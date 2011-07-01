#include "tests.h"
#include "jsfun.h"
#include "jscntxt.h"

// For TRACING_ENABLED
#include "jstracer.h"
#include "jsobjinlines.h"

#ifdef MOZ_TRACE_JSCALLS

static int depth = 0;
static int enters = 0;
static int leaves = 0;
static int interpreted = 0;

static void
funcTransition(const JSFunction *,
               const JSScript *,
               const JSContext *cx,
               int entering)
{
    if (entering > 0) {
        ++depth;
        ++enters;
        if (! JS_ON_TRACE(cx))
            ++interpreted;
    } else {
        --depth;
        ++leaves;
    }
}

static JSBool called2 = false;

static void
funcTransition2(const JSFunction *, const JSScript*, const JSContext*, int)
{
    called2 = true;
}

static int overlays = 0;
static JSFunctionCallback innerCallback = NULL;
static void
funcTransitionOverlay(const JSFunction *fun,
                      const JSScript *script,
                      const JSContext *cx,
                      int entering)
{
    (*innerCallback)(fun, script, cx, entering);
    overlays++;
}
#endif

BEGIN_TEST(testFuncCallback_bug507012)
{
#ifdef MOZ_TRACE_JSCALLS
    // Call funcTransition() whenever a Javascript method is invoked
    JS_SetFunctionCallback(cx, funcTransition);

    EXEC("x = 0; function f (n) { if (n > 1) { f(n - 1); } }");
    interpreted = enters = leaves = depth = 0;

    // Check whether JS_Execute() tracking works
    EXEC("42");
    CHECK_EQUAL(enters, 1);
    CHECK_EQUAL(leaves, 1);
    CHECK_EQUAL(depth, 0);
    interpreted = enters = leaves = depth = 0;

    // Check whether the basic function tracking works
    EXEC("f(1)");
    CHECK_EQUAL(enters, 1+1);
    CHECK_EQUAL(leaves, 1+1);
    CHECK_EQUAL(depth, 0);

    // Can we switch to a different callback?
    enters = 777;
    JS_SetFunctionCallback(cx, funcTransition2);
    EXEC("f(1)");
    CHECK(called2);
    CHECK_EQUAL(enters, 777);

    // Check whether we can turn off function tracing
    JS_SetFunctionCallback(cx, NULL);
    EXEC("f(1)");
    CHECK_EQUAL(enters, 777);
    interpreted = enters = leaves = depth = 0;

    // Check nested invocations
    JS_SetFunctionCallback(cx, funcTransition);
    enters = leaves = depth = 0;
    EXEC("f(3)");
    CHECK_EQUAL(enters, 1+3);
    CHECK_EQUAL(leaves, 1+3);
    CHECK_EQUAL(depth, 0);
    interpreted = enters = leaves = depth = 0;

    // Check calls invoked while running on trace
    EXEC("function g () { ++x; }");
    interpreted = enters = leaves = depth = 0;
    EXEC("for (i = 0; i < 50; ++i) { g(); }");
    CHECK_EQUAL(enters, 1+50);
    CHECK_EQUAL(leaves, 1+50);
    CHECK_EQUAL(depth, 0);

    // If this fails, it means that the code was interpreted rather
    // than trace-JITted, and so is not testing what it's supposed to
    // be testing. Which doesn't necessarily imply that the
    // functionality is broken.
#ifdef JS_TRACER
    if (TRACING_ENABLED(cx))
        CHECK(interpreted < enters);
#endif

    // Test nesting callbacks via JS_GetFunctionCallback()
    JS_SetFunctionCallback(cx, funcTransition);
    innerCallback = JS_GetFunctionCallback(cx);
    JS_SetFunctionCallback(cx, funcTransitionOverlay);

    EXEC("x = 0; function f (n) { if (n > 1) { f(n - 1); } }");
    interpreted = enters = leaves = depth = overlays = 0;

    EXEC("42.5");
    CHECK_EQUAL(enters, 1);
    CHECK_EQUAL(leaves, 1);
    CHECK_EQUAL(depth, 0);
    CHECK_EQUAL(overlays, enters + leaves);
    interpreted = enters = leaves = depth = overlays = 0;
#endif

    // Uncomment this to validate whether you're hitting all runmodes (interp,
    // tjit, mjit, ...?) Unfortunately, that still doesn't cover all
    // transitions between the various runmodes, but it's a start.
    //JS_DumpAllProfiles(cx);

    return true;
}

// Not strictly necessary, but part of the test attempts to check
// whether these callbacks still trigger when traced, so force
// JSOPTION_JIT just to be sure. Once the method jit and tracing jit
// are integrated, this'll probably have to change (and we'll probably
// want to test in all modes.)
virtual
JSContext *createContext()
{
    JSContext *cx = JSAPITest::createContext();
    if (cx)
        JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_JIT | JSOPTION_METHODJIT | JSOPTION_PCCOUNT);
    return cx;
}

END_TEST(testFuncCallback_bug507012)
