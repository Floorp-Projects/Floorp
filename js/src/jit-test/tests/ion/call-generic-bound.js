// Test that the generic call trampoline calls bound functions correctly,
// including nested bound functions.

function foo(f) {
  return f(0);
}

function f1(x) { return x; }
var f2 = f1.bind({});
var f3 = f2.bind({}, 1);
var f4 = f3.bind({});

var f5 = Math.log.bind({});
var f6 = f5.bind({},1);
var f7 = f6.bind({});

let boundThis = {};
function f8() { return this; }
let f9 = f8.bind(boundThis);
let f10 = f9.bind({});

with ({}) {}
for (var i = 0; i < 500; i++) {
  assertEq(foo(f1), 0);
  assertEq(foo(f2), 0);
  assertEq(foo(f3), 1);
  assertEq(foo(f4), 1);
  assertEq(foo(f5), -Infinity);
  assertEq(foo(f6), 0);
  assertEq(foo(f7), 0);
  assertEq(foo(f8), this);
  assertEq(foo(f9), boundThis);
  assertEq(foo(f10), boundThis);
}
