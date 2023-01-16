// |jit-test| --ion-warmup-threshold=50
setJitCompilerOption("offthread-compilation.enable", 0);
gcPreserveCode();

function test1() {
    var caughtInvalidArguments = false;
    var a = -1
    try {
        var buf = new Uint8ClampedArray(a);
        throw new Error("didn't throw");
    } catch (e) {
        assertEq(e instanceof RangeError, true,
                "expected RangeError, instead threw: " + e);
        caughtInvalidArguments = true;
    }
    assertEq(caughtInvalidArguments, true);
}
test1();

function test2() {
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
            assertEq(e instanceof RangeError, true,
                    "expected RangeError, instead threw: " + e);
            caughtInvalidArguments = true;
            break;
        }
    }
    assertEq(caughtInvalidArguments, true);
}
test2();

function test3() {
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
            assertEq(e instanceof RangeError, true,
                    "expected RangeError, instead threw: " + e);
            caughtInvalidArguments = true;
            break;
        }
    }
    assertEq(caughtInvalidArguments, true);
}
test3();
