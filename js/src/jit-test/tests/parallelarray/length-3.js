function testLength() {
  // Test length immutability.
  var p = new ParallelArray([1,2,3,4]);
  p.length = 0;
  assertEq(p[0], 1);
  assertEq(p.length, 4);
}

// FIXME(bug 844988) immutability not enforced
// testLength();
