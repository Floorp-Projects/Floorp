#include "gdb-tests.h"

#include "js/String.h"
#include "js/Symbol.h"

FRAGMENT(JSSymbol, simple) {
  using namespace JS;

  RootedString hello(cx, JS_NewStringCopyZ(cx, "Hello!"));

  Rooted<Symbol*> unique(cx, NewSymbol(cx, nullptr));
  Rooted<Symbol*> unique_with_desc(cx, NewSymbol(cx, hello));
  Rooted<Symbol*> registry(cx, GetSymbolFor(cx, hello));
  Rooted<Symbol*> well_known(cx, GetWellKnownSymbol(cx, SymbolCode::iterator));

  breakpoint();

  use(unique);
  use(unique_with_desc);
  use(registry);
  use(well_known);
}
