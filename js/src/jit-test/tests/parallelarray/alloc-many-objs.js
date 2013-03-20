load(libdir + "parallelarray-helpers.js");

function testMap() {
  // At least on my machine, this test is successful, whereas
  // `alloc-too-many-objs.js` fails to run in parallel because of
  // issues around GC.

  var nums = new ParallelArray(range(0, 10));

  assertParallelArrayModesCommute(["seq", "par"], function(m) {
    print(m.mode+" "+m.expect);
    nums.map(function (v) {
      var x = [];
      for (var i = 0; i < 45000; i++) {
        x[i] = {from: v};
      }
      return x;
    }, m)
  });
}

if (getBuildConfiguration().parallelJS)
  testMap();

