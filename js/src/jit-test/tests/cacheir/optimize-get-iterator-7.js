setJitCompilerOption("ion.forceinlineCaches", 1);

function f(arr) {
  var [a, b] = arr;
  return b;
}

for (var i = 0; i < 10_000; ++i) {
  assertEq(f([0, 1]), 1);
}
