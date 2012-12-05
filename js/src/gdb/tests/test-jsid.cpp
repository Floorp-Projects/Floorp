#include "gdb-tests.h"

FRAGMENT(jsid, simple) {
  js::Rooted<JSString *> string(cx, JS_NewStringCopyZ(cx, "moon"));
  js::Rooted<JSString *> interned(cx, JS_InternJSString(cx, string));
  js::Rooted<jsid> string_id(cx, INTERNED_STRING_TO_JSID(cx, interned));
  jsid int_id = INT_TO_JSID(1729);
  jsid void_id = JSID_VOID;
  js::Rooted<jsid> object_id(cx, OBJECT_TO_JSID(JS_GetGlobalObject(cx)));
  jsid xml_id = JS_DEFAULT_XML_NAMESPACE_ID;

  breakpoint();

  (void) string;
  (void) interned;
  (void) string_id;
  (void) int_id;
  (void) void_id;
  (void) object_id;
  (void) xml_id;
}
