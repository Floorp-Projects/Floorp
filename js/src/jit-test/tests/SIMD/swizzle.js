if (!this.hasOwnProperty("SIMD"))
  quit();

load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var i4 = SIMD.Int32x4(1, 2, 3, 4);

    var leet = Math.fround(13.37);
    var f4 = SIMD.Float32x4(-.5, -0, Infinity, leet);

    var compI = [
        [1,2,3,4],
        [2,3,4,1],
        [3,4,1,2],
        [4,1,2,3]
    ];

    var compF = [
        [-.5, -0, Infinity, leet],
        [-0, Infinity, leet, -.5],
        [Infinity, leet, -.5, -0],
        [leet, -.5, -0, Infinity]
    ];

    for (var i = 0; i < 150; i++) {
        // Variable lanes
        var r = SIMD.Float32x4.swizzle(f4, i % 4, (i + 1) % 4, (i + 2) % 4, (i + 3) % 4);
        assertEqX4(r, compF[i % 4]);

        // Constant lanes
        assertEqX4(SIMD.Float32x4.swizzle(f4, 3, 2, 1, 0), [leet, Infinity, -0, -.5]);

        // Variable lanes
        var r = SIMD.Int32x4.swizzle(i4, i % 4, (i + 1) % 4, (i + 2) % 4, (i + 3) % 4);
        assertEqX4(r, compI[i % 4]);

        // Constant lanes
        assertEqX4(SIMD.Int32x4.swizzle(i4, 3, 2, 1, 0), [4, 3, 2, 1]);
    }
}

function testBailouts(uglyDuckling) {
    var i4 = SIMD.Int32x4(1, 2, 3, 4);
    for (var i = 0; i < 150; i++) {
        // Test bailouts
        var value = i == 149 ? uglyDuckling : 0;
        var caught = false;
        try {
            assertEqX4(SIMD.Int32x4.swizzle(i4, value, 3, 2, 0), [1, 4, 3, 1]);
        } catch(e) {
            print(e);
            caught = true;
            assertEq(i, 149);
            assertEq(e instanceof TypeError, true);
        }
        assertEq(i < 149 || caught, true);
    }
}

function testInt32x4SwizzleBailout() {
    // Test out-of-bounds non-constant indices. This is expected to throw.
    var i4 = SIMD.Int32x4(1, 2, 3, 4);
    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Int32x4.swizzle(i4, i, 3, 2, 0), [i + 1, 4, 3, 1]);
    }
}

f();
testBailouts(-1);
testBailouts(4);
testBailouts(2.5);
testBailouts(undefined);
testBailouts(null);
testBailouts({});
testBailouts('one');
testBailouts(true);

try {
    testInt32x4SwizzleBailout();
    throw 'not caught';
} catch(e) {
    assertEq(e instanceof TypeError, true);
}
