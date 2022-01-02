/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/CallAndConstruct.h"  // JS::Construct
#include "jsapi-tests/tests.h"

BEGIN_TEST(testDifferentNewTargetInvokeConstructor) {
  JS::RootedValue func(cx);
  JS::RootedValue otherFunc(cx);

  EVAL("(function() { /* This is a different new.target function */ })",
       &otherFunc);

  EVAL(
      "(function(expected) { if (expected !== new.target) throw new "
      "Error('whoops'); })",
      &func);

  JS::RootedValueArray<1> args(cx);
  args[0].set(otherFunc);

  JS::RootedObject obj(cx);

  JS::RootedObject newTarget(cx, &otherFunc.toObject());

  CHECK(JS::Construct(cx, func, newTarget, args, &obj));

  // It should fail, though, if newTarget is not a constructor
  JS::RootedValue plain(cx);
  EVAL("({})", &plain);
  args[0].set(plain);
  newTarget = &plain.toObject();
  CHECK(!JS::Construct(cx, func, newTarget, args, &obj));

  return true;
}
END_TEST(testDifferentNewTargetInvokeConstructor)
