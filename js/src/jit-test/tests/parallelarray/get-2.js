function testGet() {
  var p = new ParallelArray([2,2,2], function(i,j,k) { return i+j+k; });
  assertEq(p.get([1,1,1]), 1+1+1);
  var p2 = new ParallelArray([2], function(i) { return 1+1+i; });
  assertEq(p.get([1,1]).toString(), p2.toString());
  var p3 = new ParallelArray([2,2], function(i,j) { return 1+i+j; });
  assertEq(p.get([1]).toString(), p3.toString());
}

testGet();
