function testMap() {
  // Test overflow
  var p = [1 << 30];
  var m = p.mapPar(function(x) { return x * 4; });
  assertEq(m[0], (1 << 30) * 4);
}

if (getBuildConfiguration().parallelJS)
  testMap();

