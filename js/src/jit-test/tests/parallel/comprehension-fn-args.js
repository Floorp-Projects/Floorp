
function buildComprehension() {
  // Test kernel function arguments
  var shape = [];
  for (var i = 0; i < 8; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function () {
      assertEq(arguments.length, shape.length);
      for (var j = 0; j < shape.length; j++)
        assertEq(arguments[j] >= 0 && arguments[j] < shape[j], true);
    });
  }
}

if (getBuildConfiguration().parallelJS)
  buildComprehension();
