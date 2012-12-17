#include "gdb-tests.h"
#include "jsatom.h"

// When JSGC_ANALYSIS is #defined, Rooted<JSFlatString*> needs the definition
// of JSFlatString in order to figure out its ThingRootKind
#include "vm/String.h"

FRAGMENT(JSString, simple) {
  js::Rooted<JSString *> empty(cx, JS_NewStringCopyN(cx, NULL, 0));
  js::Rooted<JSString *> x(cx, JS_NewStringCopyN(cx, "x", 1));
  js::Rooted<JSString *> z(cx, JS_NewStringCopyZ(cx, "z"));

  // I expect this will be a non-inlined string.
  js::Rooted<JSString *> stars(cx, JS_NewStringCopyZ(cx,
                                                     "*************************"
                                                     "*************************"
                                                     "*************************"
                                                     "*************************"));

  // This may well be an inlined string.
  js::Rooted<JSString *> xz(cx, JS_ConcatStrings(cx, x, z));

  // This will probably be a rope.
  js::Rooted<JSString *> doubleStars(cx, JS_ConcatStrings(cx, stars, stars));

  breakpoint();

  (void) empty;
  (void) x;
  (void) z;
  (void) stars;
  (void) xz;
  (void) doubleStars;
}

FRAGMENT(JSString, null) {
  js::Rooted<JSString *> null(cx, NULL);

  breakpoint();

  (void) null;
}

FRAGMENT(JSString, subclasses) {
  js::Rooted<JSFlatString *> flat(cx, JS_FlattenString(cx, JS_NewStringCopyZ(cx, "Hi!")));

  breakpoint();

  (void) flat;
}

FRAGMENT(JSString, atom) {
  JSAtom *molybdenum = js::Atomize(cx, "molybdenum", 10);
  breakpoint();

  (void) molybdenum;
}
