function testMethodInitSafety() {
    function f() { return 'fail'; }
    function g() { return 'ok'; }

    var s;
    var arr = [f, f, f, f, g];
    assertEq(arr.length > RUNLOOP, true);
    for (var i = 0; i < arr.length; i++) {
        var x = {m: arr[i]};
        s = x.m();
    }
    return s;
}
assertEq(testMethodInitSafety(), "ok");
