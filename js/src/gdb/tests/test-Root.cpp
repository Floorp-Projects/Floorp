#include "gdb-tests.h"

FRAGMENT(Root, null) {
  JS::Rooted<JSObject *> null(cx, NULL);

  breakpoint();

  (void) null;
}

void callee(JS::Handle<JSObject *> obj, JS::MutableHandle<JSObject *> mutableObj)
{
  // Prevent the linker from unifying this function with others that are
  // equivalent in machine code but not type.
  fprintf(stderr, "Called " __FILE__ ":callee\n");
  breakpoint();
}

FRAGMENT(Root, handle) {
  JS::Rooted<JSObject *> global(cx, JS::CurrentGlobalOrNull(cx));
  callee(global, &global);
  (void) global;
}

FRAGMENT(Root, HeapSlot) {
  JS::Rooted<jsval> plinth(cx, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "plinth")));
  JS::Rooted<JSObject *> array(cx, JS_NewArrayObject(cx, 1, plinth.address()));

  breakpoint();

  (void) plinth;
  (void) array;
}
