load(libdir + "parallelarray-helpers.js");

if (getBuildConfiguration().parallelJS) {
  assertArraySeqParResultsEq(range(0, 8), "map", function(e) { return [] === 0.1 });
  assertArraySeqParResultsEq(range(0, 8), "map", function(e) { return [] !== 0.1 });
}
