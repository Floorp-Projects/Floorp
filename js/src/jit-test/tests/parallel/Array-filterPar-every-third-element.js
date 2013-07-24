load(libdir + "parallelarray-helpers.js");
if (getBuildConfiguration().parallelJS)
  assertArraySeqParResultsEq(range(0, 1024), "filter", function(e, i) {
    return (i % 3) != 0;
  });

