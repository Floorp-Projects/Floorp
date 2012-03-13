
function testFilterNone() {
    var none = function () { return false; };
    var p = new ParallelArray([0,1,2,3,4]);
    var r = p.filter(none);
    assertEq(r.toString(), "");
}

testFilterNone();
