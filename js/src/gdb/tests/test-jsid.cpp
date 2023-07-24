#include "gdb-tests.h"

#include "js/Id.h"  // jsid, JS::PropertyKey
#include "js/String.h"
#include "js/Symbol.h"

FRAGMENT(jsid, simple) {
  const char* chars = "moon";
  JS::Rooted<JSString*> string(cx, JS_NewStringCopyZ(cx, chars));
  JS::Rooted<JSString*> interned(cx, JS_AtomizeAndPinString(cx, chars));
  JS::Rooted<jsid> string_id(cx, JS::PropertyKey::fromPinnedString(interned));
  JS::Rooted<jsid> int_id(cx, JS::PropertyKey::Int(1729));
  JS::Rooted<jsid> unique_symbol_id(
      cx, JS::PropertyKey::Symbol(JS::NewSymbol(cx, interned)));
  JS::Rooted<jsid> registry_symbol_id(
      cx, JS::PropertyKey::Symbol(JS::GetSymbolFor(cx, interned)));
  JS::Rooted<jsid> well_known_symbol_id(
      cx, JS::GetWellKnownSymbolKey(cx, JS::SymbolCode::iterator));
  jsid void_id = JS::PropertyKey::Void();

  breakpoint();

  use(string_id);
  use(int_id);
  use(unique_symbol_id);
  use(registry_symbol_id);
  use(well_known_symbol_id);
  use(void_id);
}

void jsid_handles(JS::Handle<jsid> jsid_handle,
                  JS::MutableHandle<jsid> mutable_jsid_handle) {
  // Prevent the linker from unifying this function with others that are
  // equivalent in machine code but not type.
  fprintf(stderr, "Called " __FILE__ ":jsid_handles\n");
  breakpoint();
}

FRAGMENT(jsid, handles) {
  const char* chars = "shovel";
  JS::Rooted<JSString*> string(cx, JS_NewStringCopyZ(cx, chars));
  JS::Rooted<JSString*> interned(cx, JS_AtomizeAndPinString(cx, chars));
  JS::Rooted<jsid> string_id(cx, JS::PropertyKey::fromPinnedString(interned));
  jsid_handles(string_id, &string_id);
}
