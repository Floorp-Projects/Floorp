function testElement() {
  // No crazy prototype walking for indexed properties
  ParallelArray.prototype[42] = "foo";
  ParallelArray.prototype.bar = "bar";
  var p = new ParallelArray([1,2,3,4]);
  assertEq(p[42], undefined);
  assertEq(42 in p, false);
  assertEq("bar" in p, true);
  // Don't inherit any indexed properties
  for (var i in p)
    assertEq(i !== 42, true);
  for (var i in p) {
    if (i % 1 !== 0)
      assertEq(i, "bar");
  }
  ParallelArray.prototype[0] = "foo";
  var p2 = new ParallelArray([,2]);
  // ParallelArrays have no holes, so 0 must be 'in' p2
  assertEq(0 in p2, true);
  assertEq(p2[0], undefined);
}

// FIXME(bug 844882) self-hosted object not array-like, exposes internal properties
// if (getBuildConfiguration().parallelJS)
//   testElement();
