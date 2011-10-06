#include "tests.h"
#include "jsscript.h"
#include "jscntxt.h"

using namespace js;

struct VersionFixture;

/*
 * Fast-native callbacks for use from JS.
 * They set their results on the current fixture instance.
 */

static VersionFixture *callbackData = NULL;

JSBool CheckVersionHasXML(JSContext *cx, uintN argc, jsval *vp);
JSBool DisableXMLOption(JSContext *cx, uintN argc, jsval *vp);
JSBool CallSetVersion17(JSContext *cx, uintN argc, jsval *vp);
JSBool CheckNewScriptNoXML(JSContext *cx, uintN argc, jsval *vp);
JSBool OverrideVersion15(JSContext *cx, uintN argc, jsval *vp);
JSBool CaptureVersion(JSContext *cx, uintN argc, jsval *vp);
JSBool CheckOverride(JSContext *cx, uintN argc, jsval *vp);
JSBool EvalScriptVersion16(JSContext *cx, uintN argc, jsval *vp);

struct VersionFixture : public JSAPITest
{
    JSVersion captured;

    virtual bool init() {
        if (!JSAPITest::init())
            return false;
        callbackData = this;
        captured = JSVERSION_UNKNOWN;
        return JS_DefineFunction(cx, global, "checkVersionHasXML", CheckVersionHasXML, 0, 0) &&
               JS_DefineFunction(cx, global, "disableXMLOption", DisableXMLOption, 0, 0) &&
               JS_DefineFunction(cx, global, "callSetVersion17", CallSetVersion17, 0, 0) &&
               JS_DefineFunction(cx, global, "checkNewScriptNoXML", CheckNewScriptNoXML, 0, 0) &&
               JS_DefineFunction(cx, global, "overrideVersion15", OverrideVersion15, 0, 0) &&
               JS_DefineFunction(cx, global, "captureVersion", CaptureVersion, 0, 0) &&
               JS_DefineFunction(cx, global, "checkOverride", CheckOverride, 1, 0) &&
               JS_DefineFunction(cx, global, "evalScriptVersion16",
                                 EvalScriptVersion16, 0, 0);
    }

    JSScript *fakeScript(const char *contents, size_t length) {
        return JS_CompileScript(cx, global, contents, length, "<test>", 1);
    }

    bool hasXML(uintN version) {
        return VersionHasXML(JSVersion(version));
    }

    bool hasXML(JSScript *script) {
        return hasXML(script->getVersion());
    }

    bool hasXML() {
        return OptionsHasXML(JS_GetOptions(cx));
    }

    bool checkOptionsHasNoXML() {
        CHECK(!OptionsHasXML(JS_GetOptions(cx)));
        return true;
    }

    bool disableXMLOption() {
        JS_SetOptions(cx, JS_GetOptions(cx) & ~JSOPTION_XML);
        return true;
    }

    bool checkVersionIsOverridden() {
        CHECK(cx->isVersionOverridden());
        return true;
    }

    /* Check that script compilation results in a version without XML. */
    bool checkNewScriptNoXML() {
        JSScript *script = fakeScript("", 0);
        CHECK(script);
        CHECK(!hasXML(script->getVersion()));
        return true;
    }

    bool checkVersionHasXML() {
        CHECK(VersionHasXML(cx->findVersion()));
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
        CHECK(JS_EvaluateUCScriptForPrincipalsVersion(
                cx, global, NULL, chars, len, "<test>", 0, &rval, version));
        return true;
    }

    bool toggleXML(bool shouldEnable) {
        CHECK_EQUAL(hasXML(), !shouldEnable);
        JS_ToggleOptions(cx, JSOPTION_XML);
        CHECK_EQUAL(hasXML(), shouldEnable);
        return true;
    }

    bool disableXML() {
        return toggleXML(false);
    }

    bool enableXML() {
        return toggleXML(true);
    }
};

/* Callbacks to throw into JS-land. */

JSBool
CallSetVersion17(JSContext *cx, uintN argc, jsval *vp)
{
    return callbackData->setVersion(JSVERSION_1_7);
}

JSBool
CheckVersionHasXML(JSContext *cx, uintN argc, jsval *vp)
{
    return callbackData->checkVersionHasXML();
}

JSBool
DisableXMLOption(JSContext *cx, uintN argc, jsval *vp)
{
    return callbackData->disableXMLOption();
}

JSBool
CheckNewScriptNoXML(JSContext *cx, uintN argc, jsval *vp)
{
    return callbackData->checkNewScriptNoXML();
}

JSBool
OverrideVersion15(JSContext *cx, uintN argc, jsval *vp)
{
    if (!callbackData->setVersion(JSVERSION_1_5))
        return false;
    return callbackData->checkVersionIsOverridden();
}

JSBool
EvalScriptVersion16(JSContext *cx, uintN argc, jsval *vp)
{
    JS_ASSERT(argc == 1);
    jsval *argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_STRING(argv[0]));
    JSString *str = JSVAL_TO_STRING(argv[0]);
    const jschar *chars = str->getChars(cx);
    JS_ASSERT(chars);
    size_t len = str->length();
    return callbackData->evalVersion(chars, len, JSVERSION_1_6);
}

JSBool
CaptureVersion(JSContext *cx, uintN argc, jsval *vp)
{
    callbackData->captured = JS_GetVersion(cx);
    return true;
}

JSBool
CheckOverride(JSContext *cx, uintN argc, jsval *vp)
{
    JS_ASSERT(argc == 1);
    jsval *argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_BOOLEAN(argv[0]));
    bool shouldHaveOverride = !!JSVAL_TO_BOOLEAN(argv[0]);
    return shouldHaveOverride == cx->isVersionOverridden();
}

/*
 * See bug 611462. We are checking that the XML option setting from a JSAPI
 * call is propagated to newly compiled scripts, instead of inheriting the XML
 * setting from a script on the stack.
 */
BEGIN_FIXTURE_TEST(VersionFixture, testOptionsAreUsedForVersionFlags)
{
    callbackData = this;

    /* Enable XML and compile a script to activate. */
    enableXML();
    const char toActivateChars[] =
        "checkVersionHasXML();"
        "disableXMLOption();"
        "callSetVersion17();"
        "checkNewScriptNoXML();";
    JSScript *toActivate = fakeScript(toActivateChars, sizeof(toActivateChars) - 1);
    CHECK(toActivate);
    CHECK(hasXML(toActivate));

    disableXML();

    /* Activate the script. */
    jsval dummy;
    CHECK(JS_ExecuteScript(cx, global, toActivate, &dummy));
    return true;
}
END_FIXTURE_TEST(VersionFixture, testOptionsAreUsedForVersionFlags)

/*
 * When re-entering the virtual machine through a *Version API the version
 * is no longer forced -- it continues with its natural push/pop oriented
 * version progression.  This is maintained by the |AutoVersionAPI| class in
 * jsapi.cpp.
 */
BEGIN_FIXTURE_TEST(VersionFixture, testEntryLosesOverride)
{
    EXEC("overrideVersion15(); evalScriptVersion16('checkOverride(false); captureVersion()');");
    CHECK_EQUAL(captured, JSVERSION_1_6);

    /* 
     * Override gets propagated to default version as non-override when you leave the VM's execute
     * call.
     */
    CHECK_EQUAL(JS_GetVersion(cx), JSVERSION_1_5);
    CHECK(!cx->isVersionOverridden());
    return true;
}
END_FIXTURE_TEST(VersionFixture, testEntryLosesOverride)

/* 
 * EvalScriptVersion does not propagate overrides to its caller, it
 * restores things exactly as they were before the call. This is as opposed to
 * the normal (no Version suffix) API which propagates overrides
 * to the caller.
 */
BEGIN_FIXTURE_TEST(VersionFixture, testReturnLosesOverride)
{
    CHECK_EQUAL(JS_GetVersion(cx), JSVERSION_ECMA_5);
    EXEC(
        "checkOverride(false);"
        "evalScriptVersion16('overrideVersion15();');"
        "checkOverride(false);"
        "captureVersion();"
    );
    CHECK_EQUAL(captured, JSVERSION_ECMA_5);
    return true;
}
END_FIXTURE_TEST(VersionFixture, testReturnLosesOverride)

BEGIN_FIXTURE_TEST(VersionFixture, testEvalPropagatesOverride)
{
    CHECK_EQUAL(JS_GetVersion(cx), JSVERSION_ECMA_5);
    EXEC(
        "checkOverride(false);"
        "eval('overrideVersion15();');"
        "checkOverride(true);"
        "captureVersion();"
    );
    CHECK_EQUAL(captured, JSVERSION_1_5);
    return true;
}
END_FIXTURE_TEST(VersionFixture, testEvalPropagatesOverride)
