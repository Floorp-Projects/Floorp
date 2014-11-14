// When the argument to WeakMap contains a key multiple times, the last value
// is retained.

var k1 = {};
var v1 = 42;
var k2 = {};
var v2 = 43;
var k3 = {};
var v3 = 44;
var k4 = {};

var wrong1 = 8;
var wrong2 = 9;
var wrong3 = 10;

var arr = [[k1, wrong1], [k2, v2], [k3, wrong2], [k1, wrong3], [k3, v3], [k1, v1]];

var m = new WeakMap(arr);

assertEq(m.has(k1), true);
assertEq(m.has(k2), true);
assertEq(m.has(k3), true);
assertEq(m.has(k4), false);
assertEq(m.get(k1), v1);
assertEq(m.get(k2), v2);
assertEq(m.get(k3), v3);
assertEq(m.get(k4), undefined);
