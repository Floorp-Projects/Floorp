/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/TestingFunctions.h"
#include "js/UbiNode.h"
#include "jsapi-tests/tests.h"
#include "vm/SavedFrame.h"

using JS::RootedObject;
using JS::RootedScript;
using JS::RootedString;

// ubi::Node::zone works
BEGIN_TEST(test_ubiNodeZone)
{
    RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(global1);
    CHECK(JS::ubi::Node(global1).zone() == cx->zone());

    RootedObject global2(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                                JS::FireOnNewGlobalHook));
    CHECK(global2);
    CHECK(global1->zone() != global2->zone());
    CHECK(JS::ubi::Node(global2).zone() == global2->zone());
    CHECK(JS::ubi::Node(global2).zone() != global1->zone());

    JS::CompileOptions options(cx);

    // Create a string and a script in the original zone...
    RootedString string1(cx, JS_NewStringCopyZ(cx, "Simpson's Individual Stringettes!"));
    CHECK(string1);
    RootedScript script1(cx);
    CHECK(JS::Compile(cx, options, "", 0, &script1));

    {
        // ... and then enter global2's zone and create a string and script
        // there, too.
        JSAutoCompartment ac(cx, global2);

        RootedString string2(cx, JS_NewStringCopyZ(cx, "A million household uses!"));
        CHECK(string2);
        RootedScript script2(cx);
        CHECK(JS::Compile(cx, options, "", 0, &script2));

        CHECK(JS::ubi::Node(string1).zone() == global1->zone());
        CHECK(JS::ubi::Node(script1).zone() == global1->zone());

        CHECK(JS::ubi::Node(string2).zone() == global2->zone());
        CHECK(JS::ubi::Node(script2).zone() == global2->zone());
    }

    return true;
}
END_TEST(test_ubiNodeZone)

// ubi::Node::compartment works
BEGIN_TEST(test_ubiNodeCompartment)
{
    RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(global1);
    CHECK(JS::ubi::Node(global1).compartment() == cx->compartment());

    RootedObject global2(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                                JS::FireOnNewGlobalHook));
    CHECK(global2);
    CHECK(global1->compartment() != global2->compartment());
    CHECK(JS::ubi::Node(global2).compartment() == global2->compartment());
    CHECK(JS::ubi::Node(global2).compartment() != global1->compartment());

    JS::CompileOptions options(cx);

    // Create a script in the original compartment...
    RootedScript script1(cx);
    CHECK(JS::Compile(cx, options, "", 0, &script1));

    {
        // ... and then enter global2's compartment and create a script
        // there, too.
        JSAutoCompartment ac(cx, global2);

        RootedScript script2(cx);
        CHECK(JS::Compile(cx, options, "", 0, &script2));

        CHECK(JS::ubi::Node(script1).compartment() == global1->compartment());
        CHECK(JS::ubi::Node(script2).compartment() == global2->compartment());
    }

    return true;
}
END_TEST(test_ubiNodeCompartment)

BEGIN_TEST(test_ubiNodeJSObjectConstructorName)
{
    JS::RootedValue val(cx);
    EVAL("this.Ctor = function Ctor() {}; new Ctor", &val);
    CHECK(val.isObject());

    mozilla::UniquePtr<char16_t[], JS::FreePolicy> ctorName;
    CHECK(JS::ubi::Node(&val.toObject()).jsObjectConstructorName(cx, ctorName));
    CHECK(ctorName);
    CHECK(js_strcmp(ctorName.get(), MOZ_UTF16("Ctor")) == 0);

    return true;
}
END_TEST(test_ubiNodeJSObjectConstructorName)

template <typename F, typename G>
static bool
checkString(const char* expected, F fillBufferFunction, G stringGetterFunction)
{
    auto expectedLength = strlen(expected);
    char16_t buf[1024];
    if (fillBufferFunction(mozilla::RangedPtr<char16_t>(buf, 1024), 1024) != expectedLength ||
        !EqualChars(buf, expected, expectedLength))
    {
        return false;
    }

    auto string = stringGetterFunction();
    // Expecting a |JSAtom*| from a live |JS::ubi::StackFrame|.
    if (!string.template is<JSAtom*>() ||
        !StringEqualsAscii(string.template as<JSAtom*>(), expected))
    {
        return false;
    }

    return true;
}

BEGIN_TEST(test_ubiStackFrame)
{
    CHECK(js::DefineTestingFunctions(cx, global, false));

    JS::RootedValue val(cx);
    CHECK(evaluate("(function one() {                      \n"  // 1
                   "  return (function two() {             \n"  // 2
                   "    return (function three() {         \n"  // 3
                   "      return saveStack();              \n"  // 4
                   "    }());                              \n"  // 5
                   "  }());                                \n"  // 6
                   "}());                                  \n", // 7
                   "filename.js",
                   1,
                   &val));

    CHECK(val.isObject());
    JS::RootedObject obj(cx, &val.toObject());

    CHECK(obj->is<SavedFrame>());
    JS::Rooted<SavedFrame*> savedFrame(cx, &obj->as<SavedFrame>());

    JS::ubi::StackFrame ubiFrame(savedFrame);

    // All frames should be from the "filename.js" source.
    while (ubiFrame) {
        CHECK(checkString("filename.js",
                          [&] (mozilla::RangedPtr<char16_t> ptr, size_t length) {
                              return ubiFrame.source(ptr, length);
                          },
                          [&] {
                              return ubiFrame.source();
                          }));
        ubiFrame = ubiFrame.parent();
    }

    ubiFrame = savedFrame;

    auto bufferFunctionDisplayName = [&] (mozilla::RangedPtr<char16_t> ptr, size_t length) {
        return ubiFrame.functionDisplayName(ptr, length);
    };
    auto getFunctionDisplayName = [&] {
        return ubiFrame.functionDisplayName();
    };

    CHECK(checkString("three", bufferFunctionDisplayName, getFunctionDisplayName));
    CHECK(ubiFrame.line() == 4);

    ubiFrame = ubiFrame.parent();
    CHECK(checkString("two", bufferFunctionDisplayName, getFunctionDisplayName));
    CHECK(ubiFrame.line() == 3);

    ubiFrame = ubiFrame.parent();
    CHECK(checkString("one", bufferFunctionDisplayName, getFunctionDisplayName));
    CHECK(ubiFrame.line() == 2);

    ubiFrame = ubiFrame.parent();
    CHECK(ubiFrame.functionDisplayName().is<JSAtom*>());
    CHECK(ubiFrame.functionDisplayName().as<JSAtom*>() == nullptr);
    CHECK(ubiFrame.line() == 1);

    ubiFrame = ubiFrame.parent();
    CHECK(!ubiFrame);

    return true;
}
END_TEST(test_ubiStackFrame)

BEGIN_TEST(test_ubiCoarseType)
{
    // Test that our explicit coarseType() overrides work as expected.

    JSObject* obj = nullptr;
    CHECK(JS::ubi::Node(obj).coarseType() == JS::ubi::CoarseType::Object);

    JSScript* script = nullptr;
    CHECK(JS::ubi::Node(script).coarseType() == JS::ubi::CoarseType::Script);

    js::LazyScript* lazyScript = nullptr;
    CHECK(JS::ubi::Node(lazyScript).coarseType() == JS::ubi::CoarseType::Script);

    js::jit::JitCode* jitCode = nullptr;
    CHECK(JS::ubi::Node(jitCode).coarseType() == JS::ubi::CoarseType::Script);

    JSString* str = nullptr;
    CHECK(JS::ubi::Node(str).coarseType() == JS::ubi::CoarseType::String);

    // Test that the default when coarseType() is not overridden is Other.

    JS::Symbol* sym = nullptr;
    CHECK(JS::ubi::Node(sym).coarseType() == JS::ubi::CoarseType::Other);

    return true;
}
END_TEST(test_ubiCoarseType)
