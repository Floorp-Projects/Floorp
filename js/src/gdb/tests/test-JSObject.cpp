#include "gdb-tests.h"

FRAGMENT(JSObject, simple) {
  JS::Rooted<JSObject *> glob(cx, JS_GetGlobalObject(cx));
  JS::Rooted<JSObject *> plain(cx, JS_NewObject(cx, 0, 0, 0));
  JS::Rooted<JSObject *> func(cx, (JSObject *) JS_NewFunction(cx, (JSNative) 1, 0, 0,
                                                              JS_GetGlobalObject(cx), "dys"));
  JS::Rooted<JSObject *> anon(cx, (JSObject *) JS_NewFunction(cx, (JSNative) 1, 0, 0,
                                                              JS_GetGlobalObject(cx), 0));
  JS::Rooted<JSFunction *> funcPtr(cx, JS_NewFunction(cx, (JSNative) 1, 0, 0,
                                                      JS_GetGlobalObject(cx), "formFollows"));

  JSObject &plainRef = *plain;
  JSFunction &funcRef = *funcPtr;
  JSObject *plainRaw = plain;
  JSObject *funcRaw = func;

  breakpoint();

  (void) glob;
  (void) plain;
  (void) func;
  (void) anon;
  (void) funcPtr;
  (void) &plainRef;
  (void) &funcRef;
  (void) plainRaw;
  (void) funcRaw;
}

FRAGMENT(JSObject, null) {
  JS::Rooted<JSObject *> null(cx, NULL);
  JSObject *nullRaw = null;

  breakpoint();

  (void) null;
  (void) nullRaw;
}
