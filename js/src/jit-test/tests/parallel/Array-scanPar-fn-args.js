function testScan() {
  // Test reduce elemental fun args
  var p = [1,2,3,4];
  var r = p.scanPar(function (a, b) {
    assertEq(a >= 1 && a <= 4, true);
    assertEq(b >= 1 && b <= 4, true);
    return a;
  });
}

if (getBuildConfiguration().parallelJS)
  testScan();
