load(libdir + "parallelarray-helpers.js");

function testScatterIdentity() {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([0,1,2,3,4]);
  assertEqParallelArray(p, r);
}

if (getBuildConfiguration().parallelJS) testScatterIdentity();

