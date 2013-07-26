function testShape() {
  var a = [1,2,3];
  var p = new ParallelArray(a);
  assertEq(p.shape.length, 1);
  assertEq(p.shape[0], a.length);
}

if (getBuildConfiguration().parallelJS) testShape();
