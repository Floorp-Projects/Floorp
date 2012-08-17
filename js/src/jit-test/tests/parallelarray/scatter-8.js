
function testScatter() {
  var shape = [5];
  for (var i = 0; i < 7; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function(k) { return k; });
    var r = p.scatter([1,0,3,2,4]);
    var p2 = new ParallelArray([p[1], p[0], p[3], p[2], p[4]]);
    assertEq(p2.toString(), r.toString());
  }
}

testScatter();

