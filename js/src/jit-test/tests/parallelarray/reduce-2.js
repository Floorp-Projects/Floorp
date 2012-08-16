
function testReduceOne() {
  var p = new ParallelArray([1]);
  var r = p.reduce(function (v, p) { return v*p; });
  assertEq(r, 1);
}

testReduceOne();
