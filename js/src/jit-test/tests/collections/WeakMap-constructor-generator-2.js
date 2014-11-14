// The argument to WeakMap can be a generator-expression.

var k1 = {};
var k2 = {};
var k3 = {};
var k4 = {};

var valueToKey = {
  1: k1,
  2: k2,
  "green": k3,
  "red": k4
};

var arr = [1, 2, "green", "red"];
var m = new WeakMap([valueToKey[v], v] for (v of arr));

for (var i = 0; i < 4; i++)
  assertEq(m.get(valueToKey[arr[i]]), arr[i]);
