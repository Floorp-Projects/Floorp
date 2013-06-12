load(libdir + "parallelarray-helpers.js");
if (getBuildConfiguration().parallelJS)
  compareAgainstArray(range(0, minItemsTestingThreshold), "filter", function(i) { return i <= 1 || i >= 1022; });
