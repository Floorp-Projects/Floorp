// Test no assert or crash from outer recorders (bug 465145)
function testBug465145() {
    this.__defineSetter__("x", function(){});
    this.watch("x", function(){});
    y = this;
    for (var z = 0; z < 2; ++z) { x = y };
    this.__defineSetter__("x", function(){});
    for (var z = 0; z < 2; ++z) { x = y };
}

function testTrueShiftTrue() {
    var a = new Array(5);
    for (var i=0;i<5;++i) a[i] = "" + (true << true);
    return a.join(",");
}
assertEq(testTrueShiftTrue(), "2,2,2,2,2");
