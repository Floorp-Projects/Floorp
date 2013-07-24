load(libdir + "parallelarray-helpers.js")

function buildComprehension() {
  // 1D comprehension
  var p = Array.buildPar(10, function (idx) { return idx; });
  var a = [0,1,2,3,4,5,6,7,8,9];
  assertEqArray(p, a);
}

if (getBuildConfiguration().parallelJS)
  buildComprehension();
