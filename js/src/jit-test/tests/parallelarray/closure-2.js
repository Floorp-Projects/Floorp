load(libdir + "parallelarray-helpers.js");

function testClosureCreation() {
  var a = range(0, 64);
  var p = new ParallelArray(a);
  var makeadd1 = function (v) { return function (x) { return x+1; }; };
  for (var i in MODES) {
    var m = p.map(makeadd1, MODES[i]);
    assertEq(m.get(1)(2), 3); // (\x.x+1) 2 == 3
  }
}

if (getBuildConfiguration().parallelJS)
  testClosureCreation();
