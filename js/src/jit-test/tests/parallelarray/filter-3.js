load(libdir + "parallelarray-helpers.js");

function testFilterSome() {
  var p = new ParallelArray([0,1,2,3,4]);
  var evenBelowThree = p.map(function (i) { return ((i%2) === 0) && (i < 3); });
  var r = p.filter(evenBelowThree);
  assertEqParallelArray(r, new ParallelArray([0,2]));
  var p = new ParallelArray([5,2], function (i,j) { return i; });
  var evenBelowThree = p.map(function (i) { return ((i[0]%2) === 0) && (i[0] < 3); });
  var r = p.filter(evenBelowThree);
  assertEqParallelArray(r, new ParallelArray([p[0], p[2]]));
}

testFilterSome();
