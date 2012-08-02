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

JSBool CheckVersionHasMoarXML(JSContext *cx, unsigned argc, jsval *vp);
JSBool DisableMoarXMLOption(JSContext *cx, unsigned argc, jsval *vp);
JSBool CallSetVersion17(JSContext *cx, unsigned argc, jsval *vp);
JSBool CheckNewScriptNoXML(JSContext *cx, unsigned argc, jsval *vp);
JSBool OverrideVersion15(JSContext *cx, unsigned argc, jsval *vp);
JSBool CaptureVersion(JSContext *cx, unsigned argc, jsval *vp);
JSBool CheckOverride(JSContext *cx, unsigned argc, jsval *vp);
JSBool EvalScriptVersion16(JSContext *cx, unsigned argc, jsval *vp);

struct VersionFixture : public JSAPITest
{
    JSVersion captured;

    virtual bool init() {
        if (!JSAPITest::init())
            return false;
        JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_ALLOW_XML);
        callbackData = this;
        captured = JSVERSION_UNKNOWN;
        JS::RootedObject global(cx, JS_GetGlobalObject(cx));
        return JS_DefineFunction(cx, global, "checkVersionHasMoarXML", CheckVersionHasMoarXML, 0, 0) &&
               JS_DefineFunction(cx, global, "disableMoarXMLOption", DisableMoarXMLOption, 0, 0) &&
               JS_DefineFunction(cx, global, "callSetVersion17", CallSetVersion17, 0, 0) &&
               JS_DefineFunction(cx, global, "checkNewScriptNoXML", CheckNewScriptNoXML, 0, 0) &&
               JS_DefineFunction(cx, global, "overrideVersion15", OverrideVersion15, 0, 0) &&
               JS_DefineFunction(cx, global, "captureVersion", CaptureVersion, 0, 0) &&
               JS_DefineFunction(cx, global, "checkOverride", CheckOverride, 1, 0) &&
               JS_DefineFunction(cx, global, "evalScriptVersion16",
                                 EvalScriptVersion16, 0, 0);
    }

    JSScript *fakeScript(const char *contents, size_t length) {
        JS::RootedObject global(cx, JS_GetGlobalObject(cx));
        return JS_CompileScript(cx, global, contents, length, "<test>", 1);
    }

    bool hasMoarXML(unsigned version) {
        return VersionHasMoarXML(JSVersion(version));
    }

    bool hasMoarXML(JSScript *script) {
        return hasMoarXML(script->getVersion());
    }

    bool hasMoarXML() {
        return OptionsHasMoarXML(JS_GetOptions(cx));
    }

    bool disableMoarXMLOption() {
        JS_SetOptions(cx, JS_GetOptions(cx) & ~JSOPTION_MOAR_XML);
        return true;
    }

    bool checkVersionIsOverridden() {
        CHECK(cx->isVersionOverridden());
        return true;
    }

    /* Check that script compilation results in a version without moar XML. */
    bool checkNewScriptNoXML() {
        JSScript *script = fakeScript("", 0);
        CHECK(script);
        CHECK(!hasMoarXML(script->getVersion()));
        return true;
    }

    bool checkVersionHasMoarXML() {
        CHECK(VersionHasMoarXML(cx->findVersion()));
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
        JS::RootedObject global(cx, JS_GetGlobalObject(cx));
        CHECK(JS_EvaluateUCScriptForPrincipalsVersion(
                cx, global, NULL, chars, len, "<test>", 0, &rval, version));
        return true;
    }

    bool toggleMoarXML(bool shouldEnable) {
        CHECK_EQUAL(hasMoarXML(), !shouldEnable);
        JS_ToggleOptions(cx, JSOPTION_MOAR_XML);
        CHECK_EQUAL(hasMoarXML(), shouldEnable);
        return true;
    }
    
    bool disableMoarXML() {
        return toggleMoarXML(false);
    }

    bool enableXML() {
        return toggleMoarXML(true);
    }
};

/* Callbacks to throw into JS-land. */

JSBool
CallSetVersion17(JSContext *cx, unsigned argc, jsval *vp)
{
    return callbackData->setVersion(JSVERSION_1_7);
}

JSBool
CheckVersionHasMoarXML(JSContext *cx, unsigned argc, jsval *vp)
{
    return callbackData->checkVersionHasMoarXML();
}

JSBool
DisableMoarXMLOption(JSContext *cx, unsigned argc, jsval *vp)
{
    return callbackData->disableMoarXMLOption();
}

JSBool
CheckNewScriptNoXML(JSContext *cx, unsigned argc, jsval *vp)
{
    return callbackData->checkNewScriptNoXML();
}

JSBool
OverrideVersion15(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!callbackData->setVersion(JSVERSION_1_5))
        return false;
    return callbackData->checkVersionIsOverridden();
}

JSBool
EvalScriptVersion16(JSContext *cx, unsigned argc, jsval *vp)
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
 * See bug 611462. We are checking that the MOAR_XML option setting from a
 * JSAPI call is propagated to newly compiled scripts, instead of inheriting
 * the XML setting from a script on the stack.
 */
BEGIN_FIXTURE_TEST(VersionFixture, testVersion_OptionsAreUsedForVersionFlags)
{
    callbackData = this;

    /* Enable XML and compile a script to activate. */
    enableXML();
    static const char toActivateChars[] =
        "checkVersionHasMoarXML();"
        "disableMoarXMLOption();"
        "callSetVersion17();"
        "checkNewScriptNoXML();";
    JSScript *toActivate = fakeScript(toActivateChars, sizeof(toActivateChars) - 1);
    CHECK(toActivate);
    CHECK(hasMoarXML(toActivate));

    disableMoarXML();

    /* Activate the script. */
    jsval dummy;
    CHECK(JS_ExecuteScript(cx, global, toActivate, &dummy));
    return true;
}
END_FIXTURE_TEST(VersionFixture, testVersion_OptionsAreUsedForVersionFlags)

/*
 * When re-entering the virtual machine through a *Version API the version
 * is no longer forced -- it continues with its natural push/pop oriented
 * version progression.  This is maintained by the |AutoVersionAPI| class in
 * jsapi.cpp.
 */
BEGIN_FIXTURE_TEST(VersionFixture, testVersion_EntryLosesOverride)
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
        "evalScriptVersion16('overrideVersion15();');"
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
        "eval('overrideVersion15();');"
        "checkOverride(true);"
        "captureVersion();"
    );
    CHECK_EQUAL(captured, JSVERSION_1_5);
    return true;
}
END_FIXTURE_TEST(VersionFixture, testVersion_EvalPropagatesOverride)

BEGIN_TEST(testVersion_AllowXML)
{
    JSBool ok;

    static const char code[] = "var m = <x/>;";
    ok = JS_EvaluateScriptForPrincipalsVersion(cx, global, NULL, code, strlen(code),
                                               __FILE__, __LINE__, NULL, JSVERSION_ECMA_5);
    CHECK_EQUAL(ok, JS_FALSE);
    JS_ClearPendingException(cx);

    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_ALLOW_XML);
    ok = JS_EvaluateScriptForPrincipalsVersion(cx, global, NULL, code, strlen(code),
                                               __FILE__, __LINE__, NULL, JSVERSION_ECMA_5);
    CHECK_EQUAL(ok, JS_TRUE);
    return true;
}
END_TEST(testVersion_AllowXML)
