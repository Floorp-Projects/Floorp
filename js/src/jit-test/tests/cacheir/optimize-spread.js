setJitCompilerOption("ion.forceinlineCaches", 1);

function testOptimizeSpread() {
  function f(a, b) {
    return a + b;
  }
  function g(...rest) {
    return f(...rest);
  }

  for (var i = 0; i < 20; ++i) {
    var v = g(1, 2);
    assertEq(v, 3);
  }
}
for (var i = 0; i < 2; ++i) testOptimizeSpread();
