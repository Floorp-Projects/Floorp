load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var i1 = SIMD.Int32x4(1, 2, 3, 4);
    var i2 = SIMD.Int32x4(5, 6, 7, 8);

    var leet = Math.fround(13.37);
    var f1 = SIMD.Float32x4(-.5, -0, Infinity, leet);
    var f2 = SIMD.Float32x4(42, .5, 23, -10);

    // computes all rotations of a given array
    function *gen(arr) {
        var previous = arr.slice().splice(0, 4);
        var i = 4;
        for (var j = 0; j < 8; j++) {
            yield previous.slice();
            previous = previous.splice(1, previous.length - 1);
            previous.push(arr[i]);
            i = (i + 1) % arr.length;
        }
    }

    var compI = [];
    var baseI = [];
    for (var i = 0; i < 8; i++)
        baseI.push(SIMD.Int32x4.extractLane(i < 4 ? i1 : i2, i % 4));
    for (var k of gen(baseI))
        compI.push(k);

    var compF = [];
    var baseF = [];
    for (var i = 0; i < 8; i++)
        baseF.push(SIMD.Float32x4.extractLane(i < 4 ? f1 : f2, i % 4));
    for (var k of gen(baseF))
        compF.push(k);

    for (var i = 0; i < 150; i++) {
        // Variable lanes
        var r = SIMD.Float32x4.shuffle(f1, f2, i % 8, (i + 1) % 8, (i + 2) % 8, (i + 3) % 8);
        assertEqX4(r, compF[i % 8]);

        // Constant lanes
        assertEqX4(SIMD.Float32x4.shuffle(f1, f2, 3, 2, 4, 5), [leet, Infinity, 42, .5]);

        // Variable lanes
        var r = SIMD.Int32x4.shuffle(i1, i2, i % 8, (i + 1) % 8, (i + 2) % 8, (i + 3) % 8);
        assertEqX4(r, compI[i % 8]);

        // Constant lanes
        assertEqX4(SIMD.Int32x4.shuffle(i1, i2, 3, 2, 4, 5), [4, 3, 5, 6]);
    }
}

function testBailouts(uglyDuckling) {
    var i1 = SIMD.Int32x4(1, 2, 3, 4);
    var i2 = SIMD.Int32x4(5, 6, 7, 8);

    for (var i = 0; i < 150; i++) {
        // Test bailouts
        var value = i == 149 ? uglyDuckling : 3;
        var caught = false;
        try {
            assertEqX4(SIMD.Int32x4.shuffle(i1, i2, value, 2, 4, 5), [4, 3, 5, 6]);
        } catch(e) {
            print(e);
            caught = true;
            assertEq(i, 149);
            assertEq(e instanceof TypeError, true);
        }
        assertEq(i < 149 || caught, true);
    }
}

f();
testBailouts(-1);
testBailouts(8);
testBailouts(2.5);
testBailouts(undefined);
testBailouts(null);
testBailouts({});
testBailouts('one');
testBailouts(true);
