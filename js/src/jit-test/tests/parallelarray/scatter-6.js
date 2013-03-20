
function testScatter() {
  // Test scatter on higher dimension
  var shape = [5];
  for (var i = 0; i < 7; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function(k) { return k; });
    var r = p.scatter([0,1,0,3,4], 9, function (a,b) { return a+b; }, 10);
    assertEq(r.length, 10);
  }
}

if (getBuildConfiguration().parallelJS) testScatter();
