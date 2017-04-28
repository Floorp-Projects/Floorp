var caughtInvalidArguments = false;
var a = -1
try {
    var buf = new Uint8ClampedArray(a);
    throw new Error("didn't throw");
} catch (e) {
    assertEq(e instanceof TypeError, true,
             "expected TypeError, instead threw: " + e);
    caughtInvalidArguments = true;
}
assertEq(caughtInvalidArguments, true);

var caughtInvalidArguments = false;
var i = 0;
while (true) {
    i = (i + 1) | 0;
    var a = inIon() ? -1 : 300;
    try {
        var buf = new Uint8ClampedArray(a);
        assertEq(buf.length, 300);
    } catch (e) {
        assertEq(a, -1);
        assertEq(e instanceof TypeError, true,
                "expected TypeError, instead threw: " + e);
        caughtInvalidArguments = true;
        break;
    }
}
assertEq(caughtInvalidArguments, true);

var caughtInvalidArguments = false;
var i = 0;
while (true) {
    i = (i + 1) | 0;
    var a = inIon() ? -1 : 0;
    try {
        var buf = new Uint8ClampedArray(a);
        assertEq(buf.length, 0);
    } catch (e) {
        assertEq(a, -1);
        assertEq(e instanceof TypeError, true,
                "expected TypeError, instead threw: " + e);
        caughtInvalidArguments = true;
        break;
    }
}
assertEq(caughtInvalidArguments, true);
