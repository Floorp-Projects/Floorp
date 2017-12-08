/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var c = 0;

function f(a) {
  const b = a;
  try {
    eval('"use strict"; b = 1 + a; c = 1');
    assertEq(0, 1);
  } catch (e) {
    assertEq(e.name, 'TypeError');
    assertEq(0, c);
    assertEq(a, b);
  }
}

var w = 42;
f(w);

c = 2;
try {
  eval('"use strict"; function g(x) { const y = x; y = 1 + x; } c = 3');
} catch (e) {
  assertEq(0, 1);
}

c = 4;
try {
  eval('"use strict"; const z = w; z = 1 + w; c = 5');
  assertEq(0, 1);
} catch (e) {
  assertEq(e.name, 'TypeError');
  assertEq(4, c);
  assertEq('z' in this, false);
}

reportCompare(0, 0, "ok");
