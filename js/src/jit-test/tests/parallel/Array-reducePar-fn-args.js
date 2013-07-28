load(libdir + "parallelarray-helpers.js");

function testReduce() {
  // Test reduce elemental fun args
  var N = 64;
  var p = range(1, N+1);
  var r = p.reducePar(function (a, b) {
    assertEq(a >= 1 && a <= N, true);
    assertEq(b >= 1 && b <= N, true);
    return a;
  });
  assertEq(r >= 1 && r <= N, true);
}

if (getBuildConfiguration().parallelJS)
  testReduce();
