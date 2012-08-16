function testFlatten() {
  var shape = [5];
  for (var i = 0; i < 7; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function(i,j) { return i+j; });
    var flatShape = ([shape[0] * shape[1]]).concat(shape.slice(2));
    assertEq(p.flatten().shape.toString(), flatShape.toString());
  }
}

testFlatten();
