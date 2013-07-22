function testScanOne() {
  function f(v, p) { throw "This should never be called."; }
  var p = [1];
  var s = p.scanPar(f);
  assertEq(s[0], 1);
  assertEq(s[0], p.reducePar(f));
}

if (getBuildConfiguration().parallelJS)
  testScanOne();
