
function testReduce() {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.reduce(function (v, p) { return v*p; });
  assertEq(r, 120);
}

testReduce();
