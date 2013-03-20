load(libdir + "eqArrayHelper.js");

function testShape() {
  // Test higher dimension shape up to 8D
  var shape = [];
  for (var i = 0; i < 8; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function () { return 0; });
    // Test shape identity and shape
    assertEqArray(p.shape, shape);
    assertEq(p.shape !== shape, true);
  }
}

if (getBuildConfiguration().parallelJS) testShape();
