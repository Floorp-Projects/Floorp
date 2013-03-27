load(libdir + "parallelarray-helpers.js");

function testScatter() {
  var shape = [5];
  for (var i = 0; i < 7; i++) {
    shape.push(2);
    var p = new ParallelArray(shape, function(k) { return k; });
    var r = p.scatter([1,0,3,2,4]);
    var p2 = new ParallelArray([p.get(1), p.get(0), p.get(3), p.get(2), p.get(4)]);
    assertEqParallelArray(p2, r);
  }
}

if (getBuildConfiguration().parallelJS) testScatter();

