#include "gdb-tests.h"
#include "jsapi.h"
#include "js/GlobalObject.h"
#include "js/Object.h"  // JS::GetClass

FRAGMENT(JSObject, simple) {
  AutoSuppressHazardsForTest noanalysis;

  JS::Rooted<JSObject*> glob(cx, JS::CurrentGlobalOrNull(cx));
  JS::Rooted<JSObject*> plain(cx, JS_NewPlainObject(cx));
  JS::Rooted<JSObject*> objectProto(cx, JS::GetRealmObjectPrototype(cx));
  JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
  JS::Rooted<JSObject*> func(
      cx, (JSObject*)JS_NewFunction(cx, (JSNative)1, 0, 0, "dys"));
  JS::Rooted<JSObject*> anon(
      cx, (JSObject*)JS_NewFunction(cx, (JSNative)1, 0, 0, nullptr));
  JS::Rooted<JSFunction*> funcPtr(
      cx, JS_NewFunction(cx, (JSNative)1, 0, 0, "formFollows"));

  JSObject& plainRef = *plain;
  JSFunction& funcRef = *funcPtr;
  JSObject* plainRaw = plain;
  JSObject* funcRaw = func;

  // JS_NewObject will now assert if you feed it a bad class name, so mangle
  // the name after construction.
  char namebuf[20] = "goodname";
  static JSClass cls{namebuf};
  JS::RootedObject badClassName(cx, JS_NewObject(cx, &cls));
  strcpy(namebuf, "\xc7X");

  breakpoint();

  use(glob);
  use(plain);
  use(objectProto);
  use(func);
  use(anon);
  use(funcPtr);
  use(&plainRef);
  use(&funcRef);
  use(JS::GetClass((JSObject*)&funcRef));
  use(plainRaw);
  use(funcRaw);
}

FRAGMENT(JSObject, null) {
  JS::Rooted<JSObject*> null(cx, nullptr);
  JSObject* nullRaw = null;

  breakpoint();

  use(null);
  use(nullRaw);
}
