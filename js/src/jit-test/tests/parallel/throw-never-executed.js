load(libdir + "parallelarray-helpers.js");

function inc(n) {
  if (n > 1024)
    throw n;
  return n + 1;
}

if (getBuildConfiguration().parallelJS)
  assertArraySeqParResultsEq(range(0, 512), "map", inc);
