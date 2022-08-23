/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Realm.h"
#include "js/RootingAPI.h"
#include "jsapi-tests/tests.h"
#include "vm/CommonPropertyNames.h"
#include "vm/JSAtomState.h"
#include "vm/JSContext.h"

BEGIN_TEST(testAtomizeWithoutActiveZone) {
  // Tests for JS_AtomizeAndPinString when called without an active zone.

  MOZ_ASSERT(cx->zone());

  JS::RootedString testAtom1(cx, JS_AtomizeString(cx, "test1234@!"));
  CHECK(testAtom1);

  JS::RootedString testAtom2(cx);
  {
    JSAutoNullableRealm ar(cx, nullptr);
    MOZ_ASSERT(!cx->zone());

    // Permanent atom.
    JSString* atom = JS_AtomizeAndPinString(cx, "boolean");
    CHECK(atom);
    CHECK_EQUAL(atom, cx->names().boolean);

    // Static string.
    atom = JS_AtomizeAndPinString(cx, "8");
    CHECK(atom);
    CHECK_EQUAL(atom, cx->staticStrings().getUint(8));

    // Existing atom.
    atom = JS_AtomizeAndPinString(cx, "test1234@!");
    CHECK(atom);
    CHECK_EQUAL(atom, testAtom1);

    // New atom.
    testAtom2 = JS_AtomizeAndPinString(cx, "asdflkjsdf987_@");
    CHECK(testAtom2);
  }

  MOZ_ASSERT(cx->zone());
  JSString* atom = JS_AtomizeString(cx, "asdflkjsdf987_@");
  CHECK(atom);
  CHECK_EQUAL(atom, testAtom2);

  return true;
}
END_TEST(testAtomizeWithoutActiveZone)
