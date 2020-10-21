// A `var` is `undefined` on entering a function body in strict mode too.

"use strict";

load(libdir + "asserts.js");

function f(a = class C{}) {
  var x;
  return x;
}
assertEq(f(), undefined);

function* g1(a = class C {}) {
  var x;
  assertEq(x, undefined);
}
g1().next();

function* g2(a = class C {}) {
  x;
  let x;
}
assertThrowsInstanceOf(() => g2().next(), ReferenceError);


