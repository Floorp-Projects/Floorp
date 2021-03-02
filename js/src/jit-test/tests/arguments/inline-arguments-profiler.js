function foo() { return arguments; }
function bar(x) { return foo(x); }

// Turning on the profiler uses up a register and increases register pressure.
enableGeckoProfiling();
with ({}) {}
for (var i = 0; i < 100; i++) {
  var result = bar(3);
  assertEq(result.length, 1);
  assertEq(result[0], 3);
}
