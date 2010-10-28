function testSlowArrayPop() {
    var a = [];
    for (var i = 0; i < RUNLOOP; i++)
        a[i] = [0];
    a[RUNLOOP-1].__defineGetter__("0", function () { return 'xyzzy'; });

    var last;
    for (var i = 0; i < RUNLOOP; i++)
        last = a[i].pop();  // reenters interpreter in getter
    return last;
}
assertEq(testSlowArrayPop(), 'xyzzy');
