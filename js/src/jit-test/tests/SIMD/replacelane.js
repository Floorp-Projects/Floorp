load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var f4 = SIMD.Float32x4(1, 2, 3, 4);
    var i4 = SIMD.Int32x4(1, 2, 3, 4);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Int32x4.replaceLane(i4, 0, 42), [42, 2, 3, 4]);
        assertEqX4(SIMD.Int32x4.replaceLane(i4, 1, 42), [1, 42, 3, 4]);
        assertEqX4(SIMD.Int32x4.replaceLane(i4, 2, 42), [1, 2, 42, 4]);
        assertEqX4(SIMD.Int32x4.replaceLane(i4, 3, 42), [1, 2, 3, 42]);

        assertEqX4(SIMD.Float32x4.replaceLane(f4, 0, 42), [42, 2, 3, 4]);
        assertEqX4(SIMD.Float32x4.replaceLane(f4, 1, 42), [1, 42, 3, 4]);
        assertEqX4(SIMD.Float32x4.replaceLane(f4, 2, 42), [1, 2, 42, 4]);
        assertEqX4(SIMD.Float32x4.replaceLane(f4, 3, 42), [1, 2, 3, 42]);
    }
}

f();

function e() {
    var f4 = SIMD.Float32x4(1, 2, 3, 4);
    var i4 = SIMD.Int32x4(1, 2, 3, 4);

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
            let x = SIMD.Int32x4.replaceLane(i4, i < 149 ? 0 : 4, 42);
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
            let x = SIMD.Int32x4.replaceLane(i4, i < 149 ? 0 : 1.1, 42);
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
            let x = SIMD.Float32x4.replaceLane(f4, i < 149 ? 0 : 4, 42);
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
            let x = SIMD.Float32x4.replaceLane(f4, i < 149 ? 0 : 1.1, 42);
        } catch(e) {
            assertEq(e instanceof TypeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }
}

e();
