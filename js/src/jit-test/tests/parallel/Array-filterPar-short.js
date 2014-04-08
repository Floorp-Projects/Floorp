load(libdir + "parallelarray-helpers.js");
if (getBuildConfiguration().parallelJS)
  assertArraySeqParResultsEq(range(0, 4), "filter", function(i) { return i % 2; });
