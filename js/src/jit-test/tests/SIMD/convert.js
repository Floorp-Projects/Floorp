load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

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
    var f4 = SIMD.float32x4(1, 2, 3, 4);
    var i4 = SIMD.int32x4(1, 2, 3, 4);
    var BitOrZero = (x) => x | 0;
    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.float32x4.fromInt32x4(i4), unaryX4(BitOrZero, f4, Math.fround));
        assertEqX4(SIMD.float32x4.fromInt32x4Bits(i4), unaryX4(cast.fromInt32Bits, f4, Math.fround));
        assertEqX4(SIMD.int32x4.fromFloat32x4(f4), unaryX4(Math.fround, i4, BitOrZero));
        assertEqX4(SIMD.int32x4.fromFloat32x4Bits(f4), unaryX4(cast.fromFloat32Bits, i4, BitOrZero));
    }
}

f();

