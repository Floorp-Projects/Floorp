load(libdir + "parallelarray-helpers.js");

if (getBuildConfiguration().parallelJS) compareAgainstArray(range(0, 512), "map", function(e) { return e+1; });
