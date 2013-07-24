load(libdir + "parallelarray-helpers.js");

var SIZE = 4096;

function testMap() {
  var q = {f: 22};
  assertArraySeqParResultsEq(range(0, SIZE), "map", function(e) {
    return e + q.f;
  });
}

if (getBuildConfiguration().parallelJS)
  testMap();
