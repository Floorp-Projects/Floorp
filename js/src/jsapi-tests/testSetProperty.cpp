/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

BEGIN_TEST(testSetProperty_InheritedGlobalSetter) {
  // This is a JSAPI test because jsapi-test globals can be set up to not have
  // a resolve hook and therefore can use the property cache in some cases
  // where the shell can't.
  MOZ_RELEASE_ASSERT(!JS_GetClass(global)->getResolve());

  CHECK(JS::InitRealmStandardClasses(cx));

  CHECK(JS_DefineProperty(cx, global, "HOTLOOP", 8, 0));
  EXEC(
      "var n = 0;\n"
      "var global = this;\n"
      "function f() { n++; }\n"
      "Object.defineProperty(Object.prototype, 'x', {set: f});\n"
      "for (var i = 0; i < HOTLOOP; i++)\n"
      "    global.x = i;\n");
  EXEC(
      "if (n != HOTLOOP)\n"
      "    throw 'FAIL';\n");
  return true;
}

const JSClass* getGlobalClass(void) override {
  static const JSClassOps noResolveGlobalClassOps = {nullptr,  // add
                                                     nullptr,  // delete
                                                     nullptr,  // enumerate
                                                     nullptr,  // newEnumerate
                                                     nullptr,  // resolve
                                                     nullptr,  // mayResolve
                                                     nullptr,  // finalize
                                                     nullptr,  // call
                                                     nullptr,  // hasInstance
                                                     nullptr,  // construct
                                                     JS_GlobalObjectTraceHook};

  static const JSClass noResolveGlobalClass = {
      "testSetProperty_InheritedGlobalSetter_noResolveGlobalClass",
      JSCLASS_GLOBAL_FLAGS, &noResolveGlobalClassOps};

  return &noResolveGlobalClass;
}
END_TEST(testSetProperty_InheritedGlobalSetter)
