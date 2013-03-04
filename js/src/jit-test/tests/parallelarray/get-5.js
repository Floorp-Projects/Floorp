function testGet() {
  // Test .get on array-like
  var p = new ParallelArray([1,2,3,4]);
  assertEq(p.get({ 0: 1, length: 1 }), 2);
  var p2 = new ParallelArray([2,2], function(i,j) { return i+j; });
  assertEq(p2.get({ 0: 1, 1: 0, length: 2 }), 1);
}

testGet();
