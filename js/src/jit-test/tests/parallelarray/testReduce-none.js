
function testReduceNone() {
    var p = new ParallelArray([]);
    var r = p.reduce(function (v, p) { return v*p; });
    assertEq(r, undefined);
}

testReduceNone();
