function testToString() {
  var p = new ParallelArray();
  assertEq(p.toString(), "");
}

if (getBuildConfiguration().parallelJS) testToString();
