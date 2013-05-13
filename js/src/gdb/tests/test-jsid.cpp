#include "gdb-tests.h"

FRAGMENT(jsid, simple) {
  JS::Rooted<JSString *> string(cx, JS_NewStringCopyZ(cx, "moon"));
  JS::Rooted<JSString *> interned(cx, JS_InternJSString(cx, string));
  JS::Rooted<jsid> string_id(cx, INTERNED_STRING_TO_JSID(cx, interned));
  jsid int_id = INT_TO_JSID(1729);
  jsid void_id = JSID_VOID;
  JS::Rooted<jsid> object_id(cx, OBJECT_TO_JSID(JS_GetGlobalForScopeChain(cx)));

  breakpoint();

  (void) string;
  (void) interned;
  (void) string_id;
  (void) int_id;
  (void) void_id;
  (void) object_id;
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
