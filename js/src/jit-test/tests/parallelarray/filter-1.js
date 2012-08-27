load(libdir + "parallelarray-helpers.js");

function testFilterAll() {
  // Test filtering everything (leaving everything in)
  var p = new ParallelArray([0,1,2,3,4]);
  var all = p.map(function (i) { return true; });
  var r = p.filter(all);
  assertEqParallelArray(r, p);
  var p = new ParallelArray([5,2], function(i,j) { return i+j; });
  var r = p.filter(all);
  assertEqParallelArray(r, new ParallelArray(p));
}

testFilterAll();
