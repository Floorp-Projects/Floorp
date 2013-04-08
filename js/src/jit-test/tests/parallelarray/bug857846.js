function testNegativeZeroScatter() {
  // Don't crash.
  var p = new ParallelArray([0]);
  var r = p.scatter([-0], 0, undefined, 1);
}

testNegativeZeroScatter();
