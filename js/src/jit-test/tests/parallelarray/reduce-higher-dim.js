
function testReduce() {
  // Test reduce on higher dimensional
  // XXX Can we test this better?
  var shape = [2];
  for (var i = 0; i < 7; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function () { return i+1; });
    var r = p.reduce(function (a, b) { return a; });
    assertEq(r.shape.length, i + 1);
  }
}

testReduce();
