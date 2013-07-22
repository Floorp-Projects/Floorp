load(libdir + "parallelarray-helpers.js");

function testMap() {
  assertArraySeqParResultsEq(range(0, 64), "map", function (v) {
    var x = [];
    var N = 2;
    for (var i = 0; i < 10; i++) {
      if ((i % N) == 0) {
        x[i] = {f1: v};
      } else if ((i % N) == 1) {
        x[i] = {f1: v, f2: v, f3: v,
                f4: v, f5: v, f6: v,
                f7: v, f8: v, f9: v};
      }
    }
    return x;
  });
}

if (getBuildConfiguration().parallelJS)
  testMap();

