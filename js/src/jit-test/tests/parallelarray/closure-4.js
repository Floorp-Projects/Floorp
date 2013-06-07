load(libdir + "parallelarray-helpers.js");

function testClosureCreationAndInvocation() {
  var a = range(1, minItemsTestingThreshold+1);
  var p = new ParallelArray(a);
  function makeaddv(v) { return function (x) { return x+v; }; };
  assertParallelExecSucceeds(
    function(m) p.map(makeaddv, m),
    function(r) {
      assertEq(r.get(1)(1), 3); // (\x.x+v){v=2} 1 == 3
      assertEq(r.get(2)(2), 5); // (\x.x+v){v=3} 2 == 5
    });
}

if (getBuildConfiguration().parallelJS)
  testClosureCreationAndInvocation();
