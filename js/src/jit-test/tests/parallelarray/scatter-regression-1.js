load(libdir + "parallelarray-helpers.js");

// The bug this is testing for:
// the input p and the scatter vector have length 4
// and the output p2 has length 3.  Even though p2
// is shorter than the input lengths, we still need
// to scan the entirety of the scatter vector, because
// it may hold valid targets at distant indices.
function testScatter() {
  var p = new ParallelArray([2,3,5,17]);
  var r = p.scatter([0,0,2,1], 9, function (x,y) { return x * y; }, 3);
  var p2 = new ParallelArray([6,17,5]);
  assertEqParallelArray(r, p2);
}

testScatter();

