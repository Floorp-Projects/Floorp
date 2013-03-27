function testScan() {
  // Test reduce elemental fun args
  var p = new ParallelArray([1,2,3,4]);
  var r = p.reduce(function (a, b) {
    assertEq(a >= 1 && a <= 4, true);
    assertEq(b >= 1 && b <= 4, true);
    return a;
  });
  assertEq(r >= 1 && r <= 4, true);
}

if (getBuildConfiguration().parallelJS) testScan();
