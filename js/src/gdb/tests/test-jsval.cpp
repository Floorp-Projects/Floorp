#include "gdb-tests.h"

#include "js/GlobalObject.h"
#include "js/String.h"
#include "js/Symbol.h"
#include "vm/BigIntType.h"

FRAGMENT(jsval, simple) {
  using namespace JS;

  RootedValue fortytwo(cx, Int32Value(42));
  RootedValue fortytwoD(cx, DoubleValue(42));
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
  RootedValue bi(cx, BigIntValue(BigInt::zero(cx)));

  RootedValue global(cx);
  global.setObject(*CurrentGlobalOrNull(cx));

  // Some interesting value that floating-point won't munge.
  RootedValue onehundredthirtysevenonehundredtwentyeighths(
      cx, DoubleValue(137.0 / 128.0));

  breakpoint();

  use(fortytwo);
  use(fortytwoD);
  use(negone);
  use(undefined);
  use(js_true);
  use(js_false);
  use(null);
  use(elements_hole);
  use(empty_string);
  use(friendly_string);
  use(symbol);
  use(bi);
  use(global);
}
