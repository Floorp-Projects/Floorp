load(libdir + "parallelarray-helpers.js");

function testScatterDefault() {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([0,2,4], 9);
  assertEqParallelArray(r, new ParallelArray([1,9,2,9,3]));
}

if (getBuildConfiguration().parallelJS) testScatterDefault();

