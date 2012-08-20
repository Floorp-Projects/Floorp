function testFlatten() {
  var p = new ParallelArray([2,2], function(i,j) { return i+j; });
  var p2 = new ParallelArray([0,1,1,2]);
  assertEq(p.flatten().toString(), p2.toString());
}

testFlatten();
