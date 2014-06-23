#include "gdb-tests.h"
#include "jsapi.h"

FRAGMENT(jsid, simple) {
  JS::Rooted<JSString *> string(cx, JS_NewStringCopyZ(cx, "moon"));
  JS::Rooted<JSString *> interned(cx, JS_InternJSString(cx, string));
  JS::Rooted<jsid> string_id(cx, INTERNED_STRING_TO_JSID(cx, interned));
  jsid int_id = INT_TO_JSID(1729);
  JS::Rooted<jsid> unique_symbol_id(
      cx, SYMBOL_TO_JSID(JS::NewSymbol(cx, interned)));
  JS::Rooted<jsid> registry_symbol_id(
      cx, SYMBOL_TO_JSID(JS::GetSymbolFor(cx, interned)));
  JS::Rooted<jsid> well_known_symbol_id(
      cx, SYMBOL_TO_JSID(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  jsid void_id = JSID_VOID;
  jsid empty_id = JSID_EMPTY;

  breakpoint();

  (void) string_id;
  (void) int_id;
  (void) unique_symbol_id;
  (void) registry_symbol_id;
  (void) well_known_symbol_id;
  (void) void_id;
  (void) empty_id;
}

void
jsid_handles(JS::Handle<jsid> jsid_handle,
             JS::MutableHandle<jsid> mutable_jsid_handle)
{
  // Prevent the linker from unifying this function with others that are
  // equivalent in machine code but not type.
  fprintf(stderr, "Called " __FILE__ ":jsid_handles\n");
  breakpoint();
}

FRAGMENT(jsid, handles) {
  JS::Rooted<JSString *> string(cx, JS_NewStringCopyZ(cx, "shovel"));
  JS::Rooted<JSString *> interned(cx, JS_InternJSString(cx, string));
  JS::Rooted<jsid> string_id(cx, INTERNED_STRING_TO_JSID(cx, interned));
  jsid_handles(string_id, &string_id);
}
