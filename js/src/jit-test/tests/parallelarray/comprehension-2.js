load(libdir + "parallelarray-helpers.js");
load(libdir + "eqArrayHelper.js");

function buildMultidim() {
  // 2D comprehension
  var p = new ParallelArray([2,2], function (i,j) { return i + j; });
  var a = new ParallelArray([0,1,1,2]).partition(2);
  assertEqArray(p.shape, [2,2]);
  assertEqParallelArray(p, a);
}

if (getBuildConfiguration().parallelJS)
  buildMultidim();
