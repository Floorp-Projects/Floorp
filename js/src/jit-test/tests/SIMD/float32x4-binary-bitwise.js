load(libdir + "simd.js");

setJitCompilerOption("ion.warmup.trigger", 50);

var helpers = (function() {
    var i32 = new Int32Array(2);
    var f32 = new Float32Array(i32.buffer);
    return {
        and: function(x, y) {
            f32[0] = x;
            f32[1] = y;
            i32[0] = i32[0] & i32[1];
            return f32[0];
        },
        or: function(x, y) {
            f32[0] = x;
            f32[1] = y;
            i32[0] = i32[0] | i32[1];
            return f32[0];
        },
        xor: function(x, y) {
            f32[0] = x;
            f32[1] = y;
            i32[0] = i32[0] ^ i32[1];
            return f32[0];
        },
    }
})();

function f() {
    var f1 = SIMD.Float32x4(1, 2, 3, 4);
    var f2 = SIMD.Float32x4(4, 3, 2, 1);
    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Float32x4.and(f1, f2), binaryX4(helpers.and, f1, f2));
        assertEqX4(SIMD.Float32x4.or(f1, f2),  binaryX4(helpers.or, f1, f2));
        assertEqX4(SIMD.Float32x4.xor(f1, f2), binaryX4(helpers.xor, f1, f2));
    }
}

f();
