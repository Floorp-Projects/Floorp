load(libdir + "parallelarray-helpers.js");

function testFlatten() {
  var p = new ParallelArray([2,2], function(i,j) { return i+j; });
  var p2 = new ParallelArray([0,1,1,2]);
  assertEqParallelArray(p.flatten(), p2);
}

if (getBuildConfiguration().parallelJS)
  testFlatten();
