// Test that the generic call trampoline implements fun_call correctly,
// even nested fun_call.

function foo(f) {
  var call = f.call;
  return call.call(call, call, call, f, {}, 1);
}

function f1(x) { return x; }
var f2 = f1.bind({});
var f3 = Math.log;
var f4 = { call: () => { return 2;} }

with ({}) {}
for (var i = 0; i < 2000; i++) {
  assertEq(foo(f4), 2);
  assertEq(foo(f1), 1);
  assertEq(foo(f2), 1);
  assertEq(foo(f3), 0);
}
