/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/FreeOp.h"
#include "gc/Marking.h"
#include "jsapi-tests/tests.h"
#include "util/Text.h"
#include "vm/JSAtom.h"
#include "vm/StringType.h"

BEGIN_TEST(testAtomizedIsNotPinned) {
  /* Try to pick a string that won't be interned by other tests in this runtime.
   */
  static const char someChars[] = "blah blah blah? blah blah blah";
  JS::Rooted<JSAtom*> atom(cx,
                           js::Atomize(cx, someChars, js_strlen(someChars)));
  CHECK(!JS_StringHasBeenPinned(cx, atom));

  JS::RootedString string(cx, JS_AtomizeAndPinString(cx, someChars));
  CHECK(string);
  CHECK(string == atom);

  CHECK(JS_StringHasBeenPinned(cx, atom));
  return true;
}
END_TEST(testAtomizedIsNotPinned)

struct StringWrapperStruct {
  JSString* str;
  bool strOk;
} sw;

BEGIN_TEST(testPinAcrossGC) {
  sw.str = JS_AtomizeAndPinString(
      cx, "wrapped chars that another test shouldn't be using");
  sw.strOk = false;
  CHECK(sw.str);
  JS_AddFinalizeCallback(cx, FinalizeCallback, nullptr);
  JS_GC(cx);
  CHECK(sw.strOk);
  return true;
}

static void FinalizeCallback(JSFreeOp* fop, JSFinalizeStatus status,
                             void* data) {
  if (status == JSFINALIZE_GROUP_START) {
    sw.strOk = js::gc::IsMarkedUnbarriered(fop->runtime(), sw.str);
  }
}
END_TEST(testPinAcrossGC)
