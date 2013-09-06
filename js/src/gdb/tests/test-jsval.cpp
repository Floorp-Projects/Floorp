#include "gdb-tests.h"
#include "jsapi.h"

FRAGMENT(jsval, simple) {
  JS::Rooted<jsval> fortytwo(cx, INT_TO_JSVAL(42));
  JS::Rooted<jsval> negone(cx, INT_TO_JSVAL(-1));
  JS::Rooted<jsval> undefined(cx, JSVAL_VOID);
  JS::Rooted<jsval> null(cx, JSVAL_NULL);
  JS::Rooted<jsval> js_true(cx, JSVAL_TRUE);
  JS::Rooted<jsval> js_false(cx, JSVAL_FALSE);
  JS::Rooted<jsval> elements_hole(cx, js::MagicValue(JS_ELEMENTS_HOLE));

  JS::Rooted<jsval> empty_string(cx);
  empty_string.setString(JS_NewStringCopyZ(cx, ""));
  JS::Rooted<jsval> friendly_string(cx);
  friendly_string.setString(JS_NewStringCopyZ(cx, "Hello!"));

  JS::Rooted<jsval> global(cx);
  global.setObject(*JS::CurrentGlobalOrNull(cx));

  // Some interesting value that floating-point won't munge.
  JS::Rooted<jsval> onehundredthirtysevenonehundredtwentyeighths(cx, DOUBLE_TO_JSVAL(137.0 / 128.0));

  breakpoint();

  (void) fortytwo;
  (void) negone;
  (void) undefined;
  (void) js_true;
  (void) js_false;
  (void) null;
  (void) elements_hole;
  (void) empty_string;
  (void) friendly_string;
  (void) global;
}
