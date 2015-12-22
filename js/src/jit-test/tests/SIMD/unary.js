load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

var notf = (function() {
    var i32 = new Int32Array(1);
    var f32 = new Float32Array(i32.buffer);
    return function(x) {
        f32[0] = x;
        i32[0] = ~i32[0];
        return f32[0];
    }
})();

function f() {
    var f4 = SIMD.Float32x4(1, 2, 3, 4);
    var i4 = SIMD.Int32x4(1, 2, 3, 4);
    var b4 = SIMD.Bool32x4(true, false, true, false);
    var BitOrZero = (x) => x | 0;
    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Float32x4.not(f4), unaryX4(notf, f4, Math.fround));
        assertEqX4(SIMD.Float32x4.neg(f4), unaryX4((x) => -x, f4, Math.fround));
        assertEqX4(SIMD.Float32x4.abs(f4), unaryX4(Math.abs, f4, Math.fround));
        assertEqX4(SIMD.Float32x4.sqrt(f4), unaryX4(Math.sqrt, f4, Math.fround));

        assertEqX4(SIMD.Float32x4.reciprocalApproximation(f4), unaryX4((x) => 1 / x, f4, Math.fround), assertNear);
        assertEqX4(SIMD.Float32x4.reciprocalSqrtApproximation(f4), unaryX4((x) => 1 / Math.sqrt(x), f4, Math.fround), assertNear);

        assertEqX4(SIMD.Int32x4.not(i4), unaryX4((x) => ~x, i4, BitOrZero));
        assertEqX4(SIMD.Int32x4.neg(i4), unaryX4((x) => -x, i4, BitOrZero));

        assertEqX4(SIMD.Bool32x4.not(b4), unaryX4((x) => !x, b4, (x) => x ));
    }
}

f();
