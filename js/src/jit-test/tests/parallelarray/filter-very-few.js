load(libdir + "parallelarray-helpers.js");
testFilter(range(0, 1024), function(i) { return i <= 1 || i >= 1022; });
