function testHolePushing() {
    var a = ["foobar", "baz"];
    for (var i = 0; i < 5; i++)
        a = [, "overwritten", "new"];
    var s = "[";
    for (i = 0; i < a.length; i++) {
        s += (i in a) ? a[i] : "<hole>";
        if (i != a.length - 1)
            s += ",";
    }
    return s + "], " + (0 in a);
}
assertEq(testHolePushing(), "[<hole>,overwritten,new], false");
