function testMap() {
  // Test overflow
  var p = new ParallelArray([1 << 30]);
  var m = p.map(function(x) { return x * 4; });
  assertEq(m.get(0), (1 << 30) * 4);
}

testMap();

