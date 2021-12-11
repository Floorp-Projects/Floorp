function f(a,b) {
    var o1 = {x: 1};
    var o2 = {x: 1};
    for (var i = 0; i < 100; i++) {
        var res = a ? (b ? o1 : o2) : null;
        assertEq(res, o1);
    }
}
f(true, true);
