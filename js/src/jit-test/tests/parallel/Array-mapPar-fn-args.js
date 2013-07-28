function testMap() {
  // Test map elemental fun args
  var p = [1,2,3,4];
  var m = p.mapPar(function(e) {
    assertEq(e >= 1 && e <= 4, true);
  });
  var m = p.mapPar(function(e,i) {
    assertEq(e >= 1 && e <= 4, true);
    assertEq(i >= 0 && i < 4, true);
  });
  var m = p.mapPar(function(e,i,c) {
    assertEq(e >= 1 && e <= 4, true);
    assertEq(i >= 0 && i < 4, true);
    assertEq(c, p);
  });
}

if (getBuildConfiguration().parallelJS)
  testMap();
