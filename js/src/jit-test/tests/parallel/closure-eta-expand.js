load(libdir + "parallelarray-helpers.js");

function testClosureCreationAndInvocation() {
  var a = range(1, 65);
  function etaadd1(v) { return (function (x) { return x+1; })(v); };
  // eta-expansion is (or at least can be) treated as call with unknown target
  for (var i in MODES) {
    var m = a.mapPar(etaadd1, MODES[i]);
    assertEq(m[1], 3); // (\x.x+1) 2 == 3
  }
}

if (getBuildConfiguration().parallelJS)
  testClosureCreationAndInvocation();
