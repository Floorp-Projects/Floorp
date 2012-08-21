
function testScatterIdentity() {
  var shape = [5];
  for (var i = 0; i < 7; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function(k) { return k; });
    var r = p.scatter([0,1,2,3,4]);
    assertEq(p.toString(), r.toString());
  }
}

testScatterIdentity();

