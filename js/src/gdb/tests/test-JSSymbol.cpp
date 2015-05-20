#include "gdb-tests.h"
#include "jsapi.h"

FRAGMENT(JSSymbol, simple) {
  using namespace JS;

  RootedString hello(cx, JS_NewStringCopyZ(cx, "Hello!"));

  Rooted<Symbol*> unique(cx, NewSymbol(cx, nullptr));
  Rooted<Symbol*> unique_with_desc(cx, NewSymbol(cx, hello));
  Rooted<Symbol*> registry(cx, GetSymbolFor(cx, hello));
  Rooted<Symbol*> well_known(cx, GetWellKnownSymbol(cx, SymbolCode::iterator));

  breakpoint();

  (void) unique;
  (void) unique_with_desc;
  (void) registry;
  (void) well_known;
}
