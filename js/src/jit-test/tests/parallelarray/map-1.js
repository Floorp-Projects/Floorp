load(libdir + "parallelarray-helpers.js");

function testMap() {
  var p = new ParallelArray([0,1,2,3,4]);
  var m = p.map(function (v) { return v+1; });
  var p2 = new ParallelArray([1,2,3,4,5]);
  assertEqParallelArray(m, p2);
}

testMap();
