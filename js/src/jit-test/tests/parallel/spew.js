load(libdir + "parallelarray-helpers.js");

try {
    var spew = getSelfHostedValue("ParallelSpew");
} catch (e) {}
if (getBuildConfiguration().parallelJS && spew) {
  assertParallelExecSucceeds(
    function (m) { Array.buildPar(minItemsTestingThreshold,
                                  function (i) { spew(i + ""); }); },
    function (r) { return true; });
}
