load(libdir + "parallelarray-helpers.js");
if (getBuildConfiguration().parallelJS)
  testFilter(range(0, 1024), function() { return false; });
