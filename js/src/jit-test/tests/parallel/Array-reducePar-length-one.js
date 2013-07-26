function testReduceOne() {
    // Note: parallel execution with only 1 element will generally
    // bailout, so don't use assertParallelArrayModesEq() here.
    var p = [1];
    var r = p.reducePar(function (v, p) { return v*p; });
    assertEq(r, 1);
}

if (getBuildConfiguration().parallelJS)
  testReduceOne();
