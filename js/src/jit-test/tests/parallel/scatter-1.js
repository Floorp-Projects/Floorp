
function testScatter() {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([0,1,0,3,4], 9, function (a,b) { return a+b; }, 10);
  assertEq(r.length, 10);
}

if (getBuildConfiguration().parallelJS) testScatter();
