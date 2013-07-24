function testLength() {
  // Test higher dimension shape up to 8D
  var shape = [];
  for (var i = 0; i < 8; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function () { return 0; });
    // Length should be outermost dimension
    assertEq(p.length, shape[0]);
  }
}

if (getBuildConfiguration().parallelJS)
  testLength();
