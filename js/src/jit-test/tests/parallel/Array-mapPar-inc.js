load(libdir + "parallelarray-helpers.js");

if (getBuildConfiguration().parallelJS)
  assertArraySeqParResultsEq(range(0, 512), "map", function(e) { return e+1; });
