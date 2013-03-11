load(libdir + "parallelarray-helpers.js");

function testMap() {
  // Test mapping higher dimensional
  var p = new ParallelArray([2,2], function (i,j) { return i+j; });
  var m = p.map(function(x) { return x; });
  var p2 = new ParallelArray(p.shape[0], function (i) { return p.get(i); });
  assertEqParallelArray(m, p2);
}

testMap();

