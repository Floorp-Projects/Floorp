// The argument to WeakMap can be a iterable object.

load(libdir + "iteration.js");

var k1 = {};
var v1 = 42;
var k2 = {};
var v2 = 43;
var k3 = {};

var done = false;

var iterable = {};
iterable[Symbol.iterator] = function*() {
  yield [k1, v1];
  yield [k2, v2];
  done = true;
};
var m = new WeakMap(iterable);

assertEq(done, true);  // the constructor consumes the argument
assertEq(m.has(k1), true);
assertEq(m.has(k2), true);
assertEq(m.has(k3), false);
assertEq(m.get(k1), v1);
assertEq(m.get(k2), v2);
assertEq(m.get(k3), undefined);

