function testEquals() {
  var p = new ParallelArray;
  assertEq(p, p);
  // Test we always rewrap shape-internal PAs
  var p2 = new ParallelArray([2,2], function (i,j) { return i+j; });
  assertEq(p2[0] !== p2[0], true);
  assertEq(p2[1] !== p2[1], true);
  var p3 = new ParallelArray([new ParallelArray([0]), new ParallelArray([1])]);
  assertEq(p3[0] !== p3[0], true);
}

// FIXME(bug 844991) logical shape not implemented
// if (getBuildConfiguration().parallelJS) testEquals();
