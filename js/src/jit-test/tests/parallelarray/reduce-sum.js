load(libdir + "parallelarray-helpers.js");

function testReduce() {
  function sum(v, p) { return v+p; }
  compareAgainstArray(range(1, 513), "reduce", sum);
}

testReduce();
