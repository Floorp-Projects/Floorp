function testReduceOne() {
    // Note: parallel execution with only 1 element will generally
    // bailout, so don't use assertParallelArrayModesEq() here.
    var p = new ParallelArray([1]);
    var r = p.reduce(function (v, p) { return v*p; });
    assertEq(r, 1);
}

testReduceOne();
