
function buildSimple() {
    var a = [1,2,3,4,5];
    var p = new ParallelArray(a);
    var e = a.join(",");
    // correct result
    assertEq(p.toString(),e);
    a[0] = 9;
    // no sharing
    assertEq(p.toString(),e);
}

buildSimple();
