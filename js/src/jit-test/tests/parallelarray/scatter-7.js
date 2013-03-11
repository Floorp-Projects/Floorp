load(libdir + "parallelarray-helpers.js");

function testScatterIdentity() {
  var shape = [5];
  for (var i = 0; i < 7; i++) {
    shape.push(i+1);
    var p = new ParallelArray(shape, function(k) { return k; });
    var r = p.scatter([0,1,2,3,4]);
    var p2 = new ParallelArray([p[0], p[1], p[2], p[3], p[4]]);
    assertEqParallelArray(p2, r);
  }
}

testScatterIdentity();

