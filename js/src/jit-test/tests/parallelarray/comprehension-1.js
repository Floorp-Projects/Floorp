load(libdir + "parallelarray-helpers.js")

function buildComprehension() {
  // 1D comprehension
  var p = new ParallelArray(10, function (idx) { return idx; });
  var a = new ParallelArray([0,1,2,3,4,5,6,7,8,9]);
  assertEqParallelArray(p, a);
}

if (getBuildConfiguration().parallelJS)
  buildComprehension();
