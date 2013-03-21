load(libdir + "parallelarray-helpers.js");

function testScatter() {
  // Ignore the rest of the scatter vector if longer than source
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([1,0,3,2,4,1,2,3]);
  var p2 = new ParallelArray([2,1,4,3,5]);
  assertEqParallelArray(r, p2);
}

testScatter();
