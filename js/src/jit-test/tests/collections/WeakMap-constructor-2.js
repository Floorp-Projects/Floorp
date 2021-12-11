// The WeakMap constructor can take an argument that is an array of pairs.

var k1 = {};
var v1 = 42;
var k2 = {};
var v2 = 43;
var k3 = {};

var arr = [[k1, v1], [k2, v2]];

var m = new WeakMap(arr);

assertEq(m.has(k1), true);
assertEq(m.has(k2), true);
assertEq(m.has(k3), false);
assertEq(m.get(k1), v1);
assertEq(m.get(k2), v2);
assertEq(m.get(k3), undefined);

var arraylike1 = {
  0: k1,
  1: v1
};
var arraylike2 = {
  0: k2,
  1: v2
};
arr = [arraylike1, arraylike2];

m = new WeakMap(arr);

assertEq(m.has(k1), true);
assertEq(m.has(k2), true);
assertEq(m.has(k3), false);
assertEq(m.get(k1), v1);
assertEq(m.get(k2), v2);
assertEq(m.get(k3), undefined);
