function testNot() {
    var a = new Object(), b = null, c = "foo", d = "", e = 5, f = 0, g = 5.5, h = -0, i = true, j = false, k = undefined;
    var r;
    for (var i = 0; i < 10; ++i)
        r = [!a, !b, !c, !d, !e, !f, !g, !h, !i, !j, !k];
    return r.join(",");
}
assertEq(testNot(), "false,true,false,true,false,true,false,true,false,true,true");
