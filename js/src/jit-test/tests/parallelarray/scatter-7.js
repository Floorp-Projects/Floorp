load(libdir + "parallelarray-helpers.js");

function testScatterIdentity() {
  var shape = [5];
  for (var i = 0; i < 7; i++) {
    shape.push(2);
    var p = new ParallelArray(shape, function(k) { return k; });
    var r = p.scatter([0,1,2,3,4]);
    var p2 = new ParallelArray([p.get(0), p.get(1), p.get(2), p.get(3), p.get(4)]);
    assertEqParallelArray(p2, r);
  }
}

testScatterIdentity();

