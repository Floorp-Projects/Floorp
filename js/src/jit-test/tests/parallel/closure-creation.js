load(libdir + "parallelarray-helpers.js");

function testClosureCreation() {
  var a = range(0, 64);
  var makeadd1 = function (v) { return function (x) { return x+1; }; };
  for (var i in MODES) {
    var m = a.mapPar(makeadd1, MODES[i]);
    assertEq(m[1](2), 3); // (\x.x+1) 2 == 3
  }
}

if (getBuildConfiguration().parallelJS)
  testClosureCreation();
