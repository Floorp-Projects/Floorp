load(libdir + "parallelarray-helpers.js");

function inc(n) {
  if (n > 1024)
    throw n;
  return n + 1;
}

compareAgainstArray(range(0, 512), "map", inc);
