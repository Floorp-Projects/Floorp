
var global = 0;

Object.defineProperty(Object.prototype, 0, {set: function() { global++; }});

for (var x = 0; x < 20; ++x)
  [1,2];
assertEq(global, 0);

Object.defineProperty(Object.prototype, 1, {set: function() { global++; }});

for (var x = 0; x < 20; ++x)
  [1,2];
assertEq(global, 0);

Object.defineProperty(Object.prototype, "b", {set: function() { global++; }});

for (var x = 0; x < 20; ++x) {
  var s = { a:0, b:1, 0: 2, 1: 3 };
}
assertEq(global, 0);

assertEq([42][0], 42);
assertEq([42].length, 1);
