/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Id.h"  // JS::PropertyKey, JS::GetWellKnownSymbolKey, JS::ToGetterId, JS::ToSetterId
#include "js/String.h"  // JSString, JS_AtomizeString, JS_StringEqualsAscii
#include "js/Symbol.h"  // JS::Symbol, JS::SymbolCode
#include "jsapi-tests/tests.h"

BEGIN_TEST(testPropertyKeyGetterAndSetter) {
  JS::Rooted<JSString*> str(cx, JS_AtomizeString(cx, "prop"));
  CHECK(str);

  JS::Rooted<JS::PropertyKey> strId(cx, JS::PropertyKey::NonIntAtom(str));
  MOZ_ASSERT(strId.isString());

  JS::Rooted<JS::PropertyKey> symId(
      cx, JS::GetWellKnownSymbolKey(cx, JS::SymbolCode::iterator));
  MOZ_ASSERT(symId.isSymbol());

  JS::Rooted<JS::PropertyKey> numId(cx, JS::PropertyKey::Int(42));
  MOZ_ASSERT(numId.isInt());

  bool match;

  JS::Rooted<JS::PropertyKey> strGetterId(cx);
  CHECK(JS::ToGetterId(cx, strId, &strGetterId));
  CHECK(strGetterId.isString());
  CHECK(JS_StringEqualsAscii(cx, strGetterId.toString(), "get prop", &match));
  CHECK(match);

  JS::Rooted<JS::PropertyKey> strSetterId(cx);
  CHECK(JS::ToSetterId(cx, strId, &strSetterId));
  CHECK(strSetterId.isString());
  CHECK(JS_StringEqualsAscii(cx, strSetterId.toString(), "set prop", &match));
  CHECK(match);

  JS::Rooted<JS::PropertyKey> symGetterId(cx);
  CHECK(JS::ToGetterId(cx, symId, &symGetterId));
  CHECK(symGetterId.isString());
  CHECK(JS_StringEqualsAscii(cx, symGetterId.toString(),
                             "get [Symbol.iterator]", &match));
  CHECK(match);

  JS::Rooted<JS::PropertyKey> symSetterId(cx);
  CHECK(JS::ToSetterId(cx, symId, &symSetterId));
  CHECK(symSetterId.isString());
  CHECK(JS_StringEqualsAscii(cx, symSetterId.toString(),
                             "set [Symbol.iterator]", &match));
  CHECK(match);

  JS::Rooted<JS::PropertyKey> numGetterId(cx);
  CHECK(JS::ToGetterId(cx, numId, &numGetterId));
  CHECK(numGetterId.isString());
  CHECK(JS_StringEqualsAscii(cx, numGetterId.toString(), "get 42", &match));
  CHECK(match);

  JS::Rooted<JS::PropertyKey> numSetterId(cx);
  CHECK(JS::ToSetterId(cx, numId, &numSetterId));
  CHECK(numSetterId.isString());
  CHECK(JS_StringEqualsAscii(cx, numSetterId.toString(), "set 42", &match));
  CHECK(match);

  return true;
}
END_TEST(testPropertyKeyGetterAndSetter)
