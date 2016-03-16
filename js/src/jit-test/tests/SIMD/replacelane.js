load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var f4 = SIMD.Float32x4(1, 2, 3, 4);
    var i4 = SIMD.Int32x4(1, 2, 3, 4);
    var b4 = SIMD.Bool32x4(true, false, true, false);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Int32x4.replaceLane(i4, 0, 42), [42, 2, 3, 4]);
        assertEqX4(SIMD.Int32x4.replaceLane(i4, 1, 42), [1, 42, 3, 4]);
        assertEqX4(SIMD.Int32x4.replaceLane(i4, 2, 42), [1, 2, 42, 4]);
        assertEqX4(SIMD.Int32x4.replaceLane(i4, 3, 42), [1, 2, 3, 42]);

        assertEqX4(SIMD.Float32x4.replaceLane(f4, 0, 42), [42, 2, 3, 4]);
        assertEqX4(SIMD.Float32x4.replaceLane(f4, 1, 42), [1, 42, 3, 4]);
        assertEqX4(SIMD.Float32x4.replaceLane(f4, 2, 42), [1, 2, 42, 4]);
        assertEqX4(SIMD.Float32x4.replaceLane(f4, 3, 42), [1, 2, 3, 42]);

        assertEqX4(SIMD.Bool32x4.replaceLane(b4, 0, false), [false, false, true, false]);
        assertEqX4(SIMD.Bool32x4.replaceLane(b4, 1, true), [true, true, true, false]);
        assertEqX4(SIMD.Bool32x4.replaceLane(b4, 2, false), [true, false, false, false]);
        assertEqX4(SIMD.Bool32x4.replaceLane(b4, 3, true), [true, false, true, true]);
    }
}

f();

function e() {
    var f4 = SIMD.Float32x4(1, 2, 3, 4);
    var i4 = SIMD.Int32x4(1, 2, 3, 4);
    var b4 = SIMD.Bool32x4(true, false, true, false);

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Int32x4.replaceLane(i < 149 ? i4 : f4, 0, 42);
        } catch(e) {
            assertEq(e instanceof TypeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Int32x4.replaceLane(i < 149 ? i4 : b4, 0, 42);
        } catch(e) {
            assertEq(e instanceof TypeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Int32x4.replaceLane(i4, i < 149 ? 0 : 4, 42);
        } catch(e) {
            assertEq(e instanceof RangeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Int32x4.replaceLane(i4, i < 149 ? 0 : 1.1, 42);
        } catch(e) {
            assertEq(e instanceof RangeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Float32x4.replaceLane(i < 149 ? f4 : i4, 0, 42);
        } catch(e) {
            assertEq(e instanceof TypeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Float32x4.replaceLane(i < 149 ? f4 : b4, 0, 42);
        } catch(e) {
            assertEq(e instanceof TypeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Float32x4.replaceLane(f4, i < 149 ? 0 : 4, 42);
        } catch(e) {
            assertEq(e instanceof RangeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Float32x4.replaceLane(f4, i < 149 ? 0 : 1.1, 42);
        } catch(e) {
            assertEq(e instanceof RangeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Bool32x4.replaceLane(i < 149 ? b4 : i4, 0, true);
        } catch(e) {
            assertEq(e instanceof TypeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Bool32x4.replaceLane(i < 149 ? b4 : f4, 0, true);
        } catch(e) {
            assertEq(e instanceof TypeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Bool32x4.replaceLane(b4, i < 149 ? 0 : 4, true);
        } catch(e) {
            assertEq(e instanceof RangeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

    for (let i = 0; i < 150; i++) {
        let caught = false;
        try {
            let x = SIMD.Bool32x4.replaceLane(b4, i < 149 ? 0 : 1.1, true);
        } catch(e) {
            assertEq(e instanceof RangeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }

}

e();
