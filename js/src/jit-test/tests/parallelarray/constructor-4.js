load(libdir + "parallelarray-helpers.js");

function buildPA() {
  // Construct copying from PA
  var p1 = new ParallelArray([1,2,3,4]);
  var p2 = new ParallelArray(p1);
  assertEqParallelArray(p1, p2);

  var p1d = new ParallelArray([2,2], function(i,j) { return i + j; });
  var p2d = new ParallelArray(p1d);
  assertEq(p1d.toString(), p2d.toString());
}

// FIXME(bug 844882) self-hosted object not array-like, exposes internal properties
// buildPA();
