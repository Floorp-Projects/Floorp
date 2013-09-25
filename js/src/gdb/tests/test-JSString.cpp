#include "gdb-tests.h"
#include "jsatom.h"
#include "jscntxt.h"

// When JSGC_ANALYSIS is #defined, Rooted<JSFlatString*> needs the definition
// of JSFlatString in order to figure out its ThingRootKind
#include "vm/String.h"

FRAGMENT(JSString, simple) {
  JS::Rooted<JSString *> empty(cx, JS_NewStringCopyN(cx, nullptr, 0));
  JS::Rooted<JSString *> x(cx, JS_NewStringCopyN(cx, "x", 1));
  JS::Rooted<JSString *> z(cx, JS_NewStringCopyZ(cx, "z"));

  // I expect this will be a non-inlined string.
  JS::Rooted<JSString *> stars(cx, JS_NewStringCopyZ(cx,
                                                     "*************************"
                                                     "*************************"
                                                     "*************************"
                                                     "*************************"));

  // This may well be an inlined string.
  JS::Rooted<JSString *> xz(cx, JS_ConcatStrings(cx, x, z));

  // This will probably be a rope.
  JS::Rooted<JSString *> doubleStars(cx, JS_ConcatStrings(cx, stars, stars));

  // Ensure we're not confused by typedefs for pointer types.
  JSString *xRaw = x;

  breakpoint();

  (void) empty;
  (void) x;
  (void) z;
  (void) stars;
  (void) xz;
  (void) doubleStars;
  (void) xRaw;
}

FRAGMENT(JSString, null) {
  JS::Rooted<JSString *> null(cx, nullptr);
  JSString *nullRaw = null;

  breakpoint();

  (void) null;
  (void) nullRaw;
}

FRAGMENT(JSString, subclasses) {
  JS::Rooted<JSFlatString *> flat(cx, JS_FlattenString(cx, JS_NewStringCopyZ(cx, "Hi!")));

  breakpoint();

  (void) flat;
}

FRAGMENT(JSString, atom) {
  JSAtom *molybdenum = js::Atomize(cx, "molybdenum", 10);
  breakpoint();

  (void) molybdenum;
}
