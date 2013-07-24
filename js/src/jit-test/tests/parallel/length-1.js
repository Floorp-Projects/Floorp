function testLength() {
  var a = [1,2,3];
  var p = new ParallelArray(a);
  assertEq(p.length, a.length);
  // Test holes
  var a = [1,,3];
  var p = new ParallelArray(a);
  assertEq(p.length, a.length);
}

if (getBuildConfiguration().parallelJS)
  testLength();
