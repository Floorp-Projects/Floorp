if (getBuildConfiguration().parallelJS) {
  function assertParallelExecSucceeds(opFunction) {
    for (var i = 0; i < 100; ++i) {
      opFunction({mode:"compile"});
    }
  }
  function assertArraySeqParResultsEq(arr, op, func) {
    assertParallelExecSucceeds(
      function (m) { 
        return arr[op + "Par"].apply(arr, [func, m]); 
      }
    );
  }
  function range(n, m) {
    var result = [];
    for (var i = n; i < m; i++)
      result.push(i);
    return result;
  }
  assertArraySeqParResultsEq(range(0, 512), "map", function(e) { return e+'x'; });
}
