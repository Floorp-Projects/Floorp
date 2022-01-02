/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"
#include "jsapi-tests/tests.h"
#include "vm/EnvironmentObject.h"

BEGIN_TEST(testForceLexicalInitialization) {
  // Attach an uninitialized lexical to a scope and ensure that it's
  // set to undefined
  js::RootedGlobalObject g(cx, cx->global());
  JS::Rooted<js::GlobalLexicalEnvironmentObject*> env(
      cx, js::GlobalLexicalEnvironmentObject::create(cx, g));

  JS::RootedValue uninitialized(cx, JS::MagicValue(JS_UNINITIALIZED_LEXICAL));
  js::RootedPropertyName name(cx, Atomize(cx, "foopi", 4)->asPropertyName());
  JS::RootedId id(cx, NameToId(name));
  unsigned attrs = JSPROP_ENUMERATE | JSPROP_PERMANENT;

  CHECK(NativeDefineDataProperty(cx, env, id, uninitialized, attrs));

  // Verify that "foopi" is uninitialized
  const JS::Value v = env->getSlot(env->lookup(cx, id)->slot());
  CHECK(v.isMagic(JS_UNINITIALIZED_LEXICAL));

  ForceLexicalInitialization(cx, env);

  // Verify that "foopi" has been initialized to undefined
  const JS::Value v2 = env->getSlot(env->lookup(cx, id)->slot());
  CHECK(v2.isUndefined());

  return true;
}
END_TEST(testForceLexicalInitialization)
