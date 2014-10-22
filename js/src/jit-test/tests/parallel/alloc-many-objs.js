load(libdir + "parallelarray-helpers.js");

function testMap() {
  // At least on my machine, this test is successful, whereas
  // `alloc-too-many-objs.js` fails to run in parallel because of
  // issues around GC.

  var nums = range(0, 10);

  assertParallelModesCommute(["seq", "par"], function(m) {
    print(m.mode+" "+m.expect);
    nums.mapPar(function (v) {
      var x = [];
      for (var i = 0; i < 20000; i++) {
        x[i] = {from: v};
      }
      return x;
    }, m)
  });
}

if (getBuildConfiguration().parallelJS)
  testMap();

