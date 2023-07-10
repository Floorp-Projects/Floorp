// Test that the generic call tramponline handles cross-realm calls.

function foo(f) {
  return f(1);
}
function foo_call(f) {
  return f.call({}, 1);
}

with ({}) {}

var g = newGlobal({sameCompartmentAs: this});
g.eval(`
function f1(x) { return x; }
var f2 = f1.bind({});
var f3 = Math.log;
`);

var f1 = g.f1;
var f2 = g.f2;
var f3 = g.f3;

var f4 = f1.bind({});
var f5 = f2.bind({});
var f6 = f3.bind({});

for (var i = 0; i < 500; i++) {
  assertEq(foo(f1), 1);
  assertEq(foo(f2), 1);
  assertEq(foo(f3), 0);
  assertEq(foo(f4), 1);
  assertEq(foo(f5), 1);
  assertEq(foo(f6), 0);
  assertEq(foo_call(f1), 1);
  assertEq(foo_call(f2), 1);
  assertEq(foo_call(f3), 0);
  assertEq(foo_call(f4), 1);
  assertEq(foo_call(f5), 1);
  assertEq(foo_call(f6), 0);
}
