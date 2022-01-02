function testSlowArrayPop() {
    var a = [];
    for (var i = 0; i < 9; i++)
        a[i] = [0];
    a[8].__defineGetter__("0", function () { return 'xyzzy'; });

    var last;
    for (var i = 0; i < 9; i++)
        last = a[i].pop();  // reenters interpreter in getter
    return last;
}
assertEq(testSlowArrayPop(), 'xyzzy');
