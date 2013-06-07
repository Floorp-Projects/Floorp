load(libdir + "parallelarray-helpers.js");

function testClosureCreation() {
  var a = range(1, minItemsTestingThreshold+1);
  var p = new ParallelArray(a);
  var makeadd1 = function (v) { return function (x) { return x+1; }; };
  assertParallelExecSucceeds(
    function(m) p.map(makeadd1, m),
    function(r) {
      assertEq(r.get(1)(2), 3); // (\x.x+1) 2 == 3
    }
  );
}

if (getBuildConfiguration().parallelJS)
  testClosureCreation();
