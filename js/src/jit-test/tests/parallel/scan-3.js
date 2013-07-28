function testScan() {
  // Test reduce on higher dimensional
  // XXX Can we test this better?
  function f(a, b) { return a; }
  var shape = [2];
  for (var i = 0; i < 7; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function () { return i+1; });
    var r = p.reduce(f);
    var s = p.scan(f)
    for (var j = 0; j < s.length; j++)
      assertEq(s.get(j).shape.length, i + 1);
    assertEq(r.shape.length, i + 1);
  }
}

if (getBuildConfiguration().parallelJS) testScan();
