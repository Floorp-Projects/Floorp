function testBasic() {
    var thisVal = {};
    var arr = [1, 2, 3];
    var f = function() {
        assertEq(this, thisVal);
        assertEq(arguments.length, 0);
        return 456;
    };
    for (var i = 0; i < 100; i++) {
        // Scripted callee.
        assertEq(f.apply(thisVal), 456);
        assertEq(f.apply(thisVal), 456);
        assertEq(f.apply(thisVal, null), 456);
        assertEq(f.apply(thisVal, undefined), 456);

        // Native callee.
        assertEq(Math.abs.apply(), NaN);
        assertEq(Math.abs.apply(null), NaN);
        assertEq(Math.abs.apply(null, null), NaN);
        assertEq(Array.prototype.join.apply(arr), "1,2,3");
        assertEq(Array.prototype.join.apply(arr, null), "1,2,3");
        assertEq(Array.prototype.join.apply(arr, undefined), "1,2,3");
    }
}
testBasic();

function testUndefinedGuard() {
    var f = function() { return arguments.length; }
    var arr = [-5, 5];
    for (var i = 0; i < 100; i++) {
        var args = i < 50 ? undefined : arr;
        assertEq(f.apply(null, args), i < 50 ? 0 : 2);
        assertEq(Math.abs.apply(null, args), i < 50 ? NaN : 5);
    }
}
testUndefinedGuard();
