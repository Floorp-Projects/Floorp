#include "gdb-tests.h"

FRAGMENT(Root, null) {
  js::Rooted<JSObject *> null(cx, NULL);

  breakpoint();

  (void) null;
}

void callee(JS::Handle<JSObject *> obj, JS::MutableHandle<JSObject *> mutableObj)
{
  breakpoint();
}

FRAGMENT(Root, handle) {
  js::Rooted<JSObject *> global(cx, JS_GetGlobalObject(cx));
  callee(global, &global);
  (void) global;
}

FRAGMENT(Root, HeapSlot) {
  js::Rooted<jsval> plinth(cx, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "plinth")));
  js::Rooted<JSObject *> array(cx, JS_NewArrayObject(cx, 1, plinth.address()));

  breakpoint();

  (void) plinth;
  (void) array;
}
