/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>

#include "gc/GC.h"

#include "js/RootingAPI.h"
#include "js/StableStringChars.h"
#include "js/String.h"  // JS::StringToLinearString

#include "jsapi-tests/tests.h"

#include "vm/JSContext.h"
#include "vm/StringType.h"

#include "vm/JSContext-inl.h"

static bool SameChars(JSContext* cx, JSString* str1, JSString* str2,
                      size_t offset) {
  JS::AutoCheckCannotGC nogc(cx);

  const JS::Latin1Char* chars1 =
      JS::StringToLinearString(cx, str1)->latin1Chars(nogc);
  const JS::Latin1Char* chars2 =
      JS::StringToLinearString(cx, str2)->latin1Chars(nogc);

  return chars1 == chars2 + offset;
}

BEGIN_TEST(testDeduplication_ASSC) {
  // Test with a long enough string to avoid inline chars allocation.
  const char text[] =
      "Andthebeastshallcomeforthsurroundedbyaroilingcloudofvengeance."
      "Thehouseoftheunbelieversshallberazedandtheyshallbescorchedtoth"
      "eearth.Theirtagsshallblinkuntiltheendofdays.";

  // Create a string to deduplicate later strings to.
  JS::RootedString original(cx);
  JS::RootedString str(cx);
  JS::RootedString dep(cx);
  JS::RootedString depdep(cx);
  JS::RootedString str2(cx);
  JS::RootedString dep2(cx);
  JS::RootedString depdep2(cx);

  {
    // This test checks the behavior when GC is performed after allocating
    // all the following strings.
    // GC shouldn't happen in between them, even in compacting jobs.
    js::gc::AutoSuppressGC suppress(cx);

    original = JS_NewStringCopyZ(cx, text);
    CHECK(original);

    // Create a chain of dependent strings, with a base string whose contents
    // match `original`'s.
    str = JS_NewStringCopyZ(cx, text);
    CHECK(str);

    dep = JS_NewDependentString(cx, str, 10, 100);
    CHECK(dep);

    depdep = JS_NewDependentString(cx, dep, 10, 80);
    CHECK(depdep);

    // Repeat. This one will not be prevented from deduplication.
    str2 = JS_NewStringCopyZ(cx, text);
    CHECK(str2);

    dep2 = JS_NewDependentString(cx, str2, 10, 100);
    CHECK(dep2);

    depdep2 = JS_NewDependentString(cx, dep2, 10, 80);
    CHECK(depdep2);
  }

  // Initializing an AutoStableStringChars with `depdep` should prevent the
  // owner of its chars (`str`) from deduplication.
  JS::AutoStableStringChars stable(cx);
  CHECK(stable.init(cx, depdep));

  const JS::Latin1Char* chars = stable.latin1Chars();
  CHECK(memcmp(chars, text + 20, 80 * sizeof(JS::Latin1Char)) == 0);

  // `depdep` should share chars with `str` but not with `original`.
  CHECK(SameChars(cx, depdep, str, 20));
  CHECK(!SameChars(cx, depdep, original, 20));

  // Same for `depdep2`.
  CHECK(SameChars(cx, depdep2, str2, 20));
  CHECK(!SameChars(cx, depdep2, original, 20));

  // Do a minor GC that will deduplicate `str2` to `original`, and would have
  // deduplicated `str` as well if it weren't prevented by the
  // AutoStableStringChars.
  cx->minorGC(JS::GCReason::API);

  // `depdep` should still share chars with `str` but not with `original`.
  CHECK(SameChars(cx, depdep, str, 20));
  CHECK(!SameChars(cx, depdep, original, 20));

  // `depdep2` should now share chars with both `str` and `original`.
  CHECK(SameChars(cx, depdep2, str2, 20));
  CHECK(SameChars(cx, depdep2, original, 20));

  return true;
}
END_TEST(testDeduplication_ASSC)
