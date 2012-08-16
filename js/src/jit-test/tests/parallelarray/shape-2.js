function testShape() {
  // Test higher dimension shape up to 8D
  var shape = [];
  for (var i = 0; i < 8; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function () { return 0; });
    // Test shape identity and shape
    assertEq(p.shape, p.shape);
    assertEq(p.shape !== shape, true);
    assertEq(p.shape.toString(), shape.toString());
  }
}

testShape();
