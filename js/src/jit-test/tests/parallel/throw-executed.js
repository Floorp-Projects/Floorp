load(libdir + "parallelarray-helpers.js");

function test() {
  var x = new Error();
  function inc(n) {
    if (inParallelSection()) // wait until par execution, then throw
      throw x;
    return n + 1;
  }
  var x = range(0, 2048);

  assertParallelExecWillBail(
    m => x.mapPar(inc, m));
}

if (getBuildConfiguration().parallelJS) test();

