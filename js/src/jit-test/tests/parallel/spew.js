load(libdir + "parallelarray-helpers.js");

var spew = getSelfHostedValue("ParallelSpew");
if (getBuildConfiguration().parallelJS && spew) {
  assertParallelExecSucceeds(
    function (m) { Array.buildPar(minItemsTestingThreshold,
                                  function (i) { spew(i + ""); }); },
    function (r) { return true; });
}
