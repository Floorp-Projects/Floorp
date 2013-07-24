function testGetNoCraziness() {
  // .get shouldn't do prototype walks
  ParallelArray.prototype[42] = "foo";
  var p = new ParallelArray([1,2,3,4]);
  assertEq(p.get(42), undefined);
}

if (getBuildConfiguration().parallelJS)
  testGetNoCraziness();
