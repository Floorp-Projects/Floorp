// WeakMap can take an argument that is an array of singleton arrays.

var k1 = {};
var k2 = {};
var k3 = {};

var arr = [[k1], [k2]];

var m = new WeakMap(arr);

assertEq(m.has(k1), true);
assertEq(m.has(k2), true);
assertEq(m.has(k3), false);
assertEq(m.get(k1), undefined);
assertEq(m.get(k2), undefined);
assertEq(m.get(k3), undefined);

var arraylike1 = {
  0: k1,
  1: undefined
};
var arraylike2 = {
  0: k2,
};

arr = [arraylike1, arraylike2];

m = new WeakMap(arr);

assertEq(m.has(k1), true);
assertEq(m.has(k2), true);
assertEq(m.has(k3), false);
assertEq(m.get(k1), undefined);
assertEq(m.get(k2), undefined);
assertEq(m.get(k3), undefined);
