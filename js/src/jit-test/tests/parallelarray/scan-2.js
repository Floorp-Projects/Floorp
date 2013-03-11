
function testScanOne() {
  function f(v, p) { return v*p; }
  var p = new ParallelArray([1]);
  var s = p.scan(f);
  assertEq(s[0], p.reduce(f));
}

testScanOne();
