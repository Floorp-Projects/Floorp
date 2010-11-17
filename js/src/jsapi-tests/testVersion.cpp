#include "tests.h"
#include "jsscript.h"
#include "jscntxt.h"

using namespace js;

struct VersionFixture;

static VersionFixture *callbackData = NULL;

struct VersionFixture : public JSAPITest
{
    JSScript *fakeScript(const char *contents, size_t length) {
        return JS_CompileScript(cx, global, contents, length, "<test>", 1);
    }

    bool hasXML(uintN version) {
        return VersionHasXML(JSVersion(version));
    }

    bool hasXML(JSScript *script) {
        return hasXML(script->version);
    }

    bool hasXML() {
        return OptionsHasXML(JS_GetOptions(cx));
    }

    bool checkVersionHasXML() {
        CHECK(hasXML(cx->findVersion()));
        return true;
    }

    bool checkOptionsHasNoXML() {
        CHECK(!OptionsHasXML(cx->options));
        return true;
    }

    /* Check that script compilation results in a version without XML. */
    bool checkNewScriptNoXML() {
        JSScript *script = fakeScript("", 0);
        CHECK(script);
        CHECK(!hasXML(script->version));
        return true;
    }

    bool setVersion(JSVersion version) {
        CHECK(JS_GetVersion(cx) != version);
        JS_SetVersion(cx, version);
        return true;
    }

    bool toggleXML(bool shouldEnable) {
        CHECK(hasXML() == !shouldEnable);
        JS_ToggleOptions(cx, JSOPTION_XML);
        CHECK(hasXML() == shouldEnable);
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
CallSetVersion(JSContext *cx, uintN argc, jsval *vp)
{
    return callbackData->setVersion(JSVERSION_1_7);
}

JSBool
CheckVersionHasXML(JSContext *cx, uintN argc, jsval *vp)
{
    return callbackData->checkVersionHasXML();
}

JSBool
CheckOptionsHasNoXML(JSContext *cx, uintN argc, jsval *vp)
{
    return callbackData->checkOptionsHasNoXML();
}

JSBool
CheckNewScriptNoXML(JSContext *cx, uintN argc, jsval *vp)
{
    return callbackData->checkNewScriptNoXML();
}

BEGIN_FIXTURE_TEST(VersionFixture, testOptionsAreUsedForVersionFlags)
{
    /*
     * See bug 611462. We are checking that the XML option setting from a JSAPI
     * call is propagated to newly compiled scripts, instead of inheriting the
     * XML setting from a script on the stack.
     */
    callbackData = this;

    /* Enable XML and compile a script to activate. */
    enableXML();
    const char toActivateChars[] =
        "checkVersionHasXML();"
        "checkOptionsHasNoXML();"
        "callSetVersion();"
        "checkNewScriptNoXML();";
    JSScript *toActivate = fakeScript(toActivateChars, sizeof(toActivateChars) - 1);
    CHECK(toActivate);
    JSObject *scriptObject = JS_GetScriptObject(toActivate);
    CHECK(hasXML(toActivate));

    disableXML();

    JSFunction *f1 = JS_DefineFunction(cx, global, "checkVersionHasXML", CheckVersionHasXML, 0, 0);
    CHECK(f1);
    JSFunction *f2 = JS_DefineFunction(cx, global, "checkOptionsHasNoXML", CheckOptionsHasNoXML,
                                       0, 0);
    CHECK(f2);
    JSFunction *f3 = JS_DefineFunction(cx, global, "callSetVersion", CallSetVersion, 0, 0);
    CHECK(f3);
    JSFunction *f4 = JS_DefineFunction(cx, global, "checkNewScriptNoXML", CheckNewScriptNoXML,
                                       0, 0);
    CHECK(f4);
    CHECK(scriptObject);

    /* Activate the script. */
    jsval dummy;
    CHECK(JS_ExecuteScript(cx, global, toActivate, &dummy));
    return true;
}
END_FIXTURE_TEST(VersionFixture, testOptionsAreUsedForVersionFlags)
