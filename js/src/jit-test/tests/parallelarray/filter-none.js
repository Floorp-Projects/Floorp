load(libdir + "parallelarray-helpers.js");
if (getBuildConfiguration().parallelJS)
  compareAgainstArray(range(0, minItemsTestingThreshold), "filter",
                      function() { return false; });
