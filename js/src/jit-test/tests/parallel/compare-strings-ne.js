load(libdir + "parallelarray-helpers.js");

if (getBuildConfiguration().parallelJS)
  assertArraySeqParResultsEq(["a", "b", "c", "d", "e", "f", "g", "h",
                              "i", "j", "k", "l", "m", "n", "o", "p",
                              "q", "r", "s", "t", "u", "v", "w", "x",
                              "y", "z"], "map", function(e) { return e != "u" && e != "x"; });
