load(libdir + "parallelarray-helpers.js");

function testScatterConflict() {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([0,1,0,3,4], 9, function (a,b) { return a+b; });
  assertEqParallelArray(r, new ParallelArray([4,2,9,4,5]));
}

if (getBuildConfiguration().parallelJS) testScatterConflict();
