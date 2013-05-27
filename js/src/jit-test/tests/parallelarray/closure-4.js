load(libdir + "parallelarray-helpers.js");

function testClosureCreationAndInvocation() {
  var a = range(1, 65);
  var p = new ParallelArray(a);
  function makeaddv(v) { return function (x) { return x+v; }; };
  for (var i in MODES) {
    var m = p.map(makeaddv, MODES[i]);
    assertEq(m.get(1)(1), 3); // (\x.x+v){v=2} 1 == 3
    assertEq(m.get(2)(2), 5); // (\x.x+v){v=3} 2 == 5
  }
}

if (getBuildConfiguration().parallelJS)
  testClosureCreationAndInvocation();
