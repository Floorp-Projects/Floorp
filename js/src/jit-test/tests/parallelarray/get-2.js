load(libdir + "parallelarray-helpers.js");

function testGet() {
  var p = new ParallelArray([2,2,2], function(i,j,k) { return i+j+k; });
  assertEq(p.get(1,1,1), 1+1+1);
  var p2 = new ParallelArray([2], function(i) { return 1+1+i; });
  assertEqParallelArray(p.get(1,1), p2);
  var p3 = new ParallelArray([2,2], function(i,j) { return 1+i+j; });
  assertEqParallelArray(p.get(1), p3);
  assertEq(p.get(5,5), undefined);
}

if (getBuildConfiguration().parallelJS)
  testGet();
