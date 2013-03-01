load(libdir + "parallelarray-helpers.js");

compareAgainstArray(range(0, 512), "map", function(e) { return e+1; });
