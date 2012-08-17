
function testScatterIdentity() {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([0,1,2,3,4]);
  assertEq(p.toString(), r.toString());
}

testScatterIdentity();

