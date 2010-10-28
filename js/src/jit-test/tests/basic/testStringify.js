function testStringify() {
    var t = true, f = false, u = undefined, n = 5, d = 5.5, s = "x";
    var a = [];
    for (var i = 0; i < 10; ++i) {
        a[0] = "" + t;
        a[1] = t + "";
        a[2] = "" + f;
        a[3] = f + "";
        a[4] = "" + u;
        a[5] = u + "";
        a[6] = "" + n;
        a[7] = n + "";
        a[8] = "" + d;
        a[9] = d + "";
        a[10] = "" + s;
        a[11] = s + "";
    }
    return a.join(",");
}
assertEq(testStringify(), "true,true,false,false,undefined,undefined,5,5,5.5,5.5,x,x");
