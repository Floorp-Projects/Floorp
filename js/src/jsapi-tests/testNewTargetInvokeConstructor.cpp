/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/CallAndConstruct.h"  // JS::Construct
#include "jsapi-tests/tests.h"

BEGIN_TEST(testNewTargetInvokeConstructor) {
  JS::RootedValue func(cx);

  EVAL(
      "(function(expected) { if (expected !== new.target) throw new "
      "Error('whoops'); })",
      &func);

  JS::RootedValueArray<1> args(cx);
  args[0].set(func);

  JS::RootedObject obj(cx);

  CHECK(JS::Construct(cx, func, args, &obj));

  return true;
}
END_TEST(testNewTargetInvokeConstructor)
