
gczeal(9, 2)
function testScatterConflict() {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([0,1,0,3,(0)], 9, function (a,b) { return a+b; });
  function assertEqParallelArray(a, b)
    assertEq(a instanceof ParallelArray, true);
  assertEqParallelArray(r, new ParallelArray([4,2,(false),4,5]));
}
if (getBuildConfiguration().parallelJS) {
  testScatterConflict();
}
