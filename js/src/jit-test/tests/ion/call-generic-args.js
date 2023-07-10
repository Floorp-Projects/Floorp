// Test that the generic call trampoline passes arguments correctly.

function foo(f) {
  return f(1);
}

with ({}) {}

function f1() { return arguments; }
function f2(x) { return [x]; }
function f3(x,y) { return [x,y]; }
function f4(x,y,z) { return [x,y,z]; }

let f5 = f4.bind({});
let f6 = f4.bind({}, 2);
let f7 = f4.bind({}, 2, 3);

function assertArrayEq(a,b) {
  assertEq(a.length, b.length);
  for (var i = 0; i < a.length; i++) {
    assertEq(a[i], b[i]);
  }
}

for (var i = 0; i < 500; i++) {
  assertArrayEq(foo(f1), [1]);
  assertArrayEq(foo(f2), [1]);
  assertArrayEq(foo(f3), [1, undefined]);
  assertArrayEq(foo(f4), [1, undefined, undefined]);
  assertArrayEq(foo(f5), [1, undefined, undefined]);
  assertArrayEq(foo(f6), [2, 1, undefined]);
  assertArrayEq(foo(f7), [2, 3, 1]);
}
