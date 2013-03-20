load(libdir + "parallelarray-helpers.js");

function kernel(n) {
  // in parallel mode just recurse infinitely.
  if (n > 10 && inParallelSection())
    return kernel(n);

  return n+1;
}

function testMap() {
  var p = new ParallelArray(range(0, 2048));
  p.map(kernel, { mode: "par", expect: "disqualified" });
}

testMap();

