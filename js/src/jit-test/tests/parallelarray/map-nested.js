load(libdir + "parallelarray-helpers.js")

function test() {
  var pa0 = new ParallelArray(range(0, minItemsTestingThreshold));

  assertParallelExecSucceeds(
    function(m) new ParallelArray(minItemsTestingThreshold, function (x) {
      return pa0.map(function(y) { return x * 1000 + y; });
    }, m),
    function(pa1) {
      for (var x = 0; x < minItemsTestingThreshold; x++) {
        var pax = pa1.get(x);
        for (var y = 0; y < minItemsTestingThreshold; y++) {
          assertEq(pax.get(y), x * 1000 + y);
        }
      }
    });
}

if (getBuildConfiguration().parallelJS)
  test();
