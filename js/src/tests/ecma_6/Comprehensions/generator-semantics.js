// Interaction of eval with generator expressions.
function a1() {
  var a = 10;
  var g = (for (y of [0]) eval('var a=42;'));
  g.next();
  return a;
}
assertEq(a1(), 10);

function a2() {
  var a = 10;
  (for (y of [0]) eval('a=42')).next();
  return a;
}
assertEq(a2(), 42)

// Arguments and this.
function b1() {
  return [for (arg of (for (i of [0, 1, 2]) arguments[i])) arg];
}
assertDeepEq(b1('a', 'b', 'c'), ['a', 'b', 'c']);

function b2() {
  return [for (x of (for (i of [0]) this)) x];
}
var b2o = { b2: b2 }
assertDeepEq(b2o.b2(), [b2o])

// Assignment to eval or arguments.
function c1() {
  return [for (arg of (for (i of [0, 1, 2]) arguments = i)) arg];
}
assertDeepEq(c1(), [0, 1, 2]);

function c2() {
  "use strict";
  return eval('[for (arg of (for (i of [0, 1, 2]) arguments = i)) arg]');
}
assertThrows(c2, SyntaxError);

function c3() {
  return [for (arg of (for (i of [0, 1, 2]) eval = i)) arg];
}
assertDeepEq(c3(), [0, 1, 2]);

function c4() {
  "use strict";
  return eval('[for (arg of (for (i of [0, 1, 2]) eval = i)) arg]');
}
assertThrows(c4, SyntaxError);

reportCompare(null, null, "test");
