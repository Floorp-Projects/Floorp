/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testSetProperty_NativeGetterStubSetter)
{
    JS::RootedObject obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    CHECK(obj);

    CHECK(JS_DefineProperty(cx, global, "globalProp", obj, JSPROP_ENUMERATE,
                            JS_STUBGETTER, JS_STUBSETTER));

    CHECK(JS_DefineProperty(cx, obj, "prop", JS::UndefinedHandleValue,
                            JSPROP_SHARED | JSPROP_PROPOP_ACCESSORS,
                            JS_PROPERTYOP_GETTER(NativeGet), JS_STUBSETTER));

    EXEC("'use strict';                                     \n"
         "var error, passed = false;                        \n"
         "try                                               \n"
         "{                                                 \n"
         "  this.globalProp.prop = 42;                      \n"
         "  throw new Error('setting property succeeded!'); \n"
         "}                                                 \n"
         "catch (e)                                         \n"
         "{                                                 \n"
         "  error = e;                                      \n"
         "  if (e instanceof TypeError)                     \n"
         "    passed = true;                                \n"
         "}                                                 \n"
         "                                                  \n"
         "if (!passed)                                      \n"
         "  throw error;                                    \n");

    EXEC("var error, passed = false;                        \n"
         "try                                               \n"
         "{                                                 \n"
         "  this.globalProp.prop = 42;                      \n"
         "  if (this.globalProp.prop === 17)                \n"
         "    passed = true;                                \n"
         "  else                                            \n"
         "    throw new Error('bad value after set!');      \n"
         "}                                                 \n"
         "catch (e)                                         \n"
         "{                                                 \n"
         "  error = e;                                      \n"
         "}                                                 \n"
         "                                                  \n"
         "if (!passed)                                      \n"
         "  throw error;                                    \n");

    return true;
}
static bool
NativeGet(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::MutableHandleValue vp)
{
    vp.set(INT_TO_JSVAL(17));
    return true;
}
END_TEST(testSetProperty_NativeGetterStubSetter)

BEGIN_TEST(testSetProperty_InheritedGlobalSetter)
{
    // This is a JSAPI test because jsapi-test globals do not have a resolve
    // hook and therefore can use the property cache in some cases where the
    // shell can't.
    MOZ_ASSERT(JS_GetClass(global)->resolve == &JS_ResolveStub);

    CHECK(JS_DefineProperty(cx, global, "HOTLOOP", 8, 0));
    EXEC("var n = 0;\n"
         "var global = this;\n"
         "function f() { n++; }\n"
         "Object.defineProperty(Object.prototype, 'x', {set: f});\n"
         "for (var i = 0; i < HOTLOOP; i++)\n"
         "    global.x = i;\n");
    EXEC("if (n != HOTLOOP)\n"
         "    throw 'FAIL';\n");
    return true;
}
END_TEST(testSetProperty_InheritedGlobalSetter)

