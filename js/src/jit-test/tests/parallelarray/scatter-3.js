load(libdir + "parallelarray-helpers.js");

function testScatter() {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([1,0,3,2,4]);
  var p2 = new ParallelArray([2,1,4,3,5]);
  assertEqParallelArray(r, p2);
}

if (getBuildConfiguration().parallelJS) testScatter();

