function testMap() {
  // Test mapping higher dimensional
  var p = new ParallelArray([2,2], function (i,j) { return i+j; });
  var m = p.map(function(x) { return x.toString(); });
  assertEq(m.toString(), "<<0,1>,<1,2>>");
}

testMap();

