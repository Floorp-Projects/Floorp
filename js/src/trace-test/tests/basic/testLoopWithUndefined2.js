function loopWithUndefined2(t, dostuff, val) {
    var a = new Array(6);
    for (var i = 0; i < 6; i++) {
        if (dostuff) {
            val = 1;
            a[i] = (t > val);
        } else {
            a[i] = (val == undefined);
        }
    }
    return a;
}
function testLoopWithUndefined2() {
    var a = loopWithUndefined2(5.0, true, 2);
    var b = loopWithUndefined2(5.0, true);
    var c = loopWithUndefined2(5.0, false, 8);
    var d = loopWithUndefined2(5.0, false);
    return [a[0], b[0], c[0], d[0]].join(",");
}
assertEq(testLoopWithUndefined2(), "true,true,false,true");
