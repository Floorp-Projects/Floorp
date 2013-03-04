load(libdir + "parallelarray-helpers.js");

// since we divide things into chunks of 32, and filter uses some
// bitsets, test that all that logic works fine if the number of items
// is not evenly divisible by 32:
testFilter(range(0, 617), function(i) { return (i % 2) == 0; });
