load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 30);

var cast = (function() {
    var i32 = new Int32Array(1);
    var f32 = new Float32Array(i32.buffer);
    return {
        fromInt32Bits(x) {
            i32[0] = x;
            return f32[0];
        },

        fromFloat32Bits(x) {
            f32[0] = x;
            return i32[0];
        }
    }
})();

function f() {
    // No bailout here.
    var f4 = SIMD.Float32x4(1, 2, 3, 4);
    var i4 = SIMD.Int32x4(1, 2, 3, 4);
    var BitOrZero = (x) => x | 0;
    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Float32x4.fromInt32x4(i4), unaryX4(BitOrZero, f4, Math.fround));
        assertEqX4(SIMD.Float32x4.fromInt32x4Bits(i4), unaryX4(cast.fromInt32Bits, f4, Math.fround));
        assertEqX4(SIMD.Int32x4.fromFloat32x4(f4), unaryX4(Math.fround, i4, BitOrZero));
        assertEqX4(SIMD.Int32x4.fromFloat32x4Bits(f4), unaryX4(cast.fromFloat32Bits, i4, BitOrZero));
    }
}

function uglyDuckling(val) {
    // We bail out when i == 149 because the conversion will return
    // 0x80000000 and the input actually wasn't in bounds.
    val = Math.fround(val);
    for (var i = 0; i < 150; i++) {
        var caught = false;
        try {
            var v = SIMD.Float32x4(i < 149 ? 0 : val, 0, 0, 0)
            SIMD.Int32x4.fromFloat32x4(v);
        } catch(e) {
            assertEq(e instanceof RangeError, true);
            assertEq(i, 149);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }
}

function dontBail() {
    // On x86, the conversion will return 0x80000000, which will imply that we
    // check the input values. However, we shouldn't bail out in this case.
    for (var i = 0; i < 150; i++) {
        var v = SIMD.Float32x4(i < 149 ? 0 : -Math.pow(2, 31), 0, 0, 0)
        SIMD.Int32x4.fromFloat32x4(v);
    }
}

f();

dontBail();
dontBail();

uglyDuckling(Math.pow(2, 31));
uglyDuckling(NaN);
uglyDuckling(-Math.pow(2, 32));
