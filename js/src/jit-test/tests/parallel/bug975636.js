load(libdir + "parallelarray-helpers.js");
if (getBuildConfiguration().parallelJS) {
  function f(x) { return [] << 0; }
  var a = Array.buildPar(9, f);
  var b = Array.build(9, f);
  assertEqArray(a, b);
}
