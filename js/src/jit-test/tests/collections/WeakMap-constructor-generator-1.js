// The argument to WeakMap can be a generator.

var k1 = {};
var v1 = 42;
var k2 = {};
var v2 = 43;
var k3 = {};

var done = false;

function data() {
  yield [k1, v1];
  yield [k2, v2];
  done = true;
};

var m = new WeakMap(data());

assertEq(done, true);  // the constructor consumes the argument
assertEq(m.has(k1), true);
assertEq(m.has(k2), true);
assertEq(m.has(k3), false);
assertEq(m.get(k1), v1);
assertEq(m.get(k2), v2);
assertEq(m.get(k3), undefined);

done = false;

function* data2() {
  yield [k1, v1];
  yield [k2, v2];
  done = true;
};

m = new WeakMap(data2());

assertEq(done, true);  // the constructor consumes the argument
assertEq(m.has(k1), true);
assertEq(m.has(k2), true);
assertEq(m.has(k3), false);
assertEq(m.get(k1), v1);
assertEq(m.get(k2), v2);
assertEq(m.get(k3), undefined);
