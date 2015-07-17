#include "gdb-tests.h"
#include "jsapi.h"

FRAGMENT(jsval, simple) {
  using namespace JS;

  RootedValue fortytwo(cx, Int32Value(42));
  RootedValue negone(cx, Int32Value(-1));
  RootedValue undefined(cx, UndefinedValue());
  RootedValue null(cx, NullValue());
  RootedValue js_true(cx, BooleanValue(true));
  RootedValue js_false(cx, BooleanValue(false));
  RootedValue elements_hole(cx, js::MagicValue(JS_ELEMENTS_HOLE));

  RootedValue empty_string(cx);
  empty_string.setString(JS_NewStringCopyZ(cx, ""));
  RootedString hello(cx, JS_NewStringCopyZ(cx, "Hello!"));
  RootedValue friendly_string(cx, StringValue(hello));
  RootedValue symbol(cx, SymbolValue(GetSymbolFor(cx, hello)));

  RootedValue global(cx);
  global.setObject(*CurrentGlobalOrNull(cx));

  // Some interesting value that floating-point won't munge.
  RootedValue onehundredthirtysevenonehundredtwentyeighths(cx, DoubleValue(137.0 / 128.0));

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
  (void) symbol;
  (void) global;
}
