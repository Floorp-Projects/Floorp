#include "gdb-tests.h"

FRAGMENT(jsval, simple) {
  js::Rooted<jsval> fortytwo(cx, INT_TO_JSVAL(42));
  js::Rooted<jsval> negone(cx, INT_TO_JSVAL(-1));
  js::Rooted<jsval> undefined(cx, JSVAL_VOID);
  js::Rooted<jsval> null(cx, JSVAL_NULL);
  js::Rooted<jsval> js_true(cx, JSVAL_TRUE);
  js::Rooted<jsval> js_false(cx, JSVAL_FALSE);
  js::Rooted<jsval> array_hole(cx, js::MagicValue(JS_ARRAY_HOLE));

  js::Rooted<jsval> empty_string(cx);
  empty_string.setString(JS_NewStringCopyZ(cx, ""));
  js::Rooted<jsval> friendly_string(cx);
  friendly_string.setString(JS_NewStringCopyZ(cx, "Hello!"));

  js::Rooted<jsval> global(cx);
  global.setObject(*JS_GetGlobalObject(cx));

  // Some interesting value that floating-point won't munge.
  js::Rooted<jsval> onehundredthirtysevenonehundredtwentyeighths(cx, DOUBLE_TO_JSVAL(137.0 / 128.0));

  breakpoint();

  (void) fortytwo;
  (void) negone;
  (void) undefined;
  (void) js_true;
  (void) js_false;
  (void) null;
  (void) array_hole;
  (void) empty_string;
  (void) friendly_string;
  (void) global;
}
