load(libdir + "parallelarray-helpers.js");

function inc(n) {
  if (n > 1024)
    throw n;
  return n + 1;
}

if (getBuildConfiguration().parallelJS)
  compareAgainstArray(range(0, minItemsTestingThreshold), "map", inc);
