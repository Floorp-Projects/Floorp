/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"
#include "jsscript.h"
#include "jscntxt.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

using namespace js;

struct VersionFixture;

/*
 * Fast-native callbacks for use from JS.
 * They set their results on the current fixture instance.
 */

static VersionFixture *callbackData = NULL;

JSBool CallSetVersion17(JSContext *cx, unsigned argc, jsval *vp);
JSBool OverrideVersion18(JSContext *cx, unsigned argc, jsval *vp);
JSBool CaptureVersion(JSContext *cx, unsigned argc, jsval *vp);
JSBool CheckOverride(JSContext *cx, unsigned argc, jsval *vp);
JSBool EvalScriptVersion16(JSContext *cx, unsigned argc, jsval *vp);

struct VersionFixture : public JSAPITest
{
    JSVersion captured;

    virtual bool init() {
        if (!JSAPITest::init())
            return false;
        JS_SetOptions(cx, JS_GetOptions(cx));
        callbackData = this;
        captured = JSVERSION_UNKNOWN;
        JS::RootedObject global(cx, JS_GetGlobalForScopeChain(cx));
        return JS_DefineFunction(cx, global, "callSetVersion17", CallSetVersion17, 0, 0) &&
               JS_DefineFunction(cx, global, "overrideVersion18", OverrideVersion18, 0, 0) &&
               JS_DefineFunction(cx, global, "captureVersion", CaptureVersion, 0, 0) &&
               JS_DefineFunction(cx, global, "checkOverride", CheckOverride, 1, 0) &&
               JS_DefineFunction(cx, global, "evalScriptVersion16",
                                 EvalScriptVersion16, 0, 0);
    }

    JSScript *fakeScript(const char *contents, size_t length) {
        JS::RootedObject global(cx, JS_GetGlobalForScopeChain(cx));
        return JS_CompileScript(cx, global, contents, length, "<test>", 1);
    }

    bool checkVersionIsOverridden() {
        CHECK(cx->isVersionOverridden());
        return true;
    }

    bool setVersion(JSVersion version) {
        CHECK(JS_GetVersion(cx) != version);
        JS_SetVersion(cx, version);
        return true;
    }

    bool evalVersion(const jschar *chars, size_t len, JSVersion version) {
        CHECK(JS_GetVersion(cx) != version);
        jsval rval;
        JS::RootedObject global(cx, JS_GetGlobalForScopeChain(cx));
        CHECK(JS_EvaluateUCScriptForPrincipalsVersion(
                cx, global, NULL, chars, len, "<test>", 0, &rval, version));
        return true;
    }
};

/* Callbacks to throw into JS-land. */

JSBool
CallSetVersion17(JSContext *cx, unsigned argc, jsval *vp)
{
    return callbackData->setVersion(JSVERSION_1_7);
}

JSBool
OverrideVersion18(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!callbackData->setVersion(JSVERSION_1_8))
        return false;
    return callbackData->checkVersionIsOverridden();
}

JSBool
EvalScriptVersion16(JSContext *cx, unsigned argc, jsval *vp)
{
    JS_ASSERT(argc == 1);
    jsval *argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_STRING(argv[0]));
    JSStableString *str = JSVAL_TO_STRING(argv[0])->ensureStable(cx);
    JS_ASSERT(str);
    return callbackData->evalVersion(str->chars().get(), str->length(), JSVERSION_1_6);
}

JSBool
CaptureVersion(JSContext *cx, unsigned argc, jsval *vp)
{
    callbackData->captured = JS_GetVersion(cx);
    return true;
}

JSBool
CheckOverride(JSContext *cx, unsigned argc, jsval *vp)
{
    JS_ASSERT(argc == 1);
    jsval *argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_BOOLEAN(argv[0]));
    bool shouldHaveOverride = !!JSVAL_TO_BOOLEAN(argv[0]);
    return shouldHaveOverride == cx->isVersionOverridden();
}

/*
 * When re-entering the virtual machine through a *Version API the version
 * is no longer forced -- it continues with its natural push/pop oriented
 * version progression.  This is maintained by the |AutoVersionAPI| class in
 * jsapi.cpp.
 */
BEGIN_FIXTURE_TEST(VersionFixture, testVersion_EntryLosesOverride)
{
    EXEC("overrideVersion18(); evalScriptVersion16('checkOverride(false); captureVersion()');");
    CHECK_EQUAL(captured, JSVERSION_1_6);

    /*
     * Override gets propagated to default version as non-override when you leave the VM's execute
     * call.
     */
    CHECK_EQUAL(JS_GetVersion(cx), JSVERSION_1_8);
    CHECK(!cx->isVersionOverridden());
    return true;
}
END_FIXTURE_TEST(VersionFixture, testVersion_EntryLosesOverride)

/*
 * EvalScriptVersion does not propagate overrides to its caller, it
 * restores things exactly as they were before the call. This is as opposed to
 * the normal (no Version suffix) API which propagates overrides
 * to the caller.
 */
BEGIN_FIXTURE_TEST(VersionFixture, testVersion_ReturnLosesOverride)
{
    CHECK_EQUAL(JS_GetVersion(cx), JSVERSION_ECMA_5);
    EXEC(
        "checkOverride(false);"
        "evalScriptVersion16('overrideVersion18();');"
        "checkOverride(false);"
        "captureVersion();"
    );
    CHECK_EQUAL(captured, JSVERSION_ECMA_5);
    return true;
}
END_FIXTURE_TEST(VersionFixture, testVersion_ReturnLosesOverride)

BEGIN_FIXTURE_TEST(VersionFixture, testVersion_EvalPropagatesOverride)
{
    CHECK_EQUAL(JS_GetVersion(cx), JSVERSION_ECMA_5);
    EXEC(
        "checkOverride(false);"
        "eval('overrideVersion18();');"
        "checkOverride(true);"
        "captureVersion();"
    );
    CHECK_EQUAL(captured, JSVERSION_1_8);
    return true;
}
END_FIXTURE_TEST(VersionFixture, testVersion_EvalPropagatesOverride)
