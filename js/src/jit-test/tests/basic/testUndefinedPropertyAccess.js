function testUndefinedPropertyAccess() {
    var x = [1,2,3];
    var y = {};
    var a = { foo: 1 };
    y.__proto__ = x;
    var z = [x, x, x, y, y, y, y, a, a, a];
    var s = "";
    for (var i = 0; i < z.length; ++i)
        s += z[i].foo;
    return s;
}
assertEq(testUndefinedPropertyAccess(), "undefinedundefinedundefinedundefinedundefinedundefinedundefined111");
checkStats({
    traceCompleted: 3
});
