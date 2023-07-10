// Test that the generic call trampoline handles constructors correctly.

function foo(f) {
  return new f(1);
}

with ({}) {}

function f1(x) { this.x = x; }
function f2(y) { this.y = y; }
let f3 = f1.bind({});
let f4 = f1.bind({}, 2);
let f5 = Uint8Array;

for (var i = 0; i < 500; i++) {
  assertEq(foo(f1).x, 1);
  assertEq(foo(f2).y, 1);
  assertEq(foo(f3).x, 1);
  assertEq(foo(f4).x, 2);
  assertEq(foo(f5)[0], 0);
}
