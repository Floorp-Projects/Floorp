/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"
#include "util/StringBuffer.h"
#include "vm/JSAtomUtils.h"  // AtomizeString

BEGIN_TEST(testStringBuffer_finishString) {
  JSString* str = JS_NewStringCopyZ(cx, "foopy");
  CHECK(str);

  JS::Rooted<JSAtom*> atom(cx, js::AtomizeString(cx, str));
  CHECK(atom);

  js::StringBuffer buffer(cx);
  CHECK(buffer.append("foopy"));

  JS::Rooted<JSAtom*> finishedAtom(cx, buffer.finishAtom());
  CHECK(finishedAtom);
  CHECK_EQUAL(atom, finishedAtom);
  return true;
}
END_TEST(testStringBuffer_finishString)
