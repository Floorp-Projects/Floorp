function testScanOne() {
  function f(v, p) { throw "This should never be called."; }
  var p = new ParallelArray([1]);
  var s = p.scan(f);
  assertEq(s.get(0), 1);
  assertEq(s.get(0), p.reduce(f));
}

if (getBuildConfiguration().parallelJS) testScanOne();
