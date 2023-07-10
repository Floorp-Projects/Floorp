// Test that the generic call trampoline calls methods correctly.

function foo(o) {
  return o.f(1);
}

function id(x) { return x; }

var o1 = { f: id }
var o2 = { f: id.bind({}) }
var o3 = { f: id.bind({}, 2) }
var o4 = [0,0,0,1,0,0,0,0,1,0,0,0];
o4.f = [].indexOf;
var o5 = [0,0,0,1,0,0,0,1,0,0];
o5.f = [].lastIndexOf;

with ({}) {}
for (var i = 0; i < 500; i++) {
  assertEq(foo(o1), 1);
  assertEq(foo(o2), 1);
  assertEq(foo(o3), 2);
  assertEq(foo(o4), 3);
  assertEq(foo(o5), 7);
}
