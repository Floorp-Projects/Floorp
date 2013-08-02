load(libdir + "parallelarray-helpers.js");

if (getBuildConfiguration().parallelJS) {
  assertParallelExecSucceeds(
    function (m) {
      return Array.buildPar(minItemsTestingThreshold,
                            function (i) { return arguments.length; },
                            m);
    },
    function (r) {
      assertStructuralEq(Array.build(minItemsTestingThreshold,
                                     function (i) { return arguments.length }),
                         r);
    });
}
