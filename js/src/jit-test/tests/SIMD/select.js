load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function int32x4FromTypeBits(type, vec) {
    if (type == SIMD.int32x4)
        return vec;
    if (type == SIMD.float32x4)
        return SIMD.int32x4.fromFloat32x4Bits(vec);
    throw 'unimplemented';
}

function bitselect(type, mask, ifTrue, ifFalse) {
    var int32x4 = SIMD.int32x4;
    var tv = int32x4FromTypeBits(type, ifTrue);
    var fv = int32x4FromTypeBits(type, ifFalse);
    var tr = int32x4.and(mask, tv);
    var fr = int32x4.and(int32x4.not(mask), fv);
    var orApplied = int32x4.or(tr, fr);
    var converted = type == int32x4 ? orApplied : type.fromInt32x4Bits(orApplied);
    return simdToArray(converted);
}

function f() {
    var f1 = SIMD.float32x4(1, 2, 3, 4);
    var f2 = SIMD.float32x4(NaN, Infinity, 3.14, -0);

    var i1 = SIMD.int32x4(2, 3, 5, 8);
    var i2 = SIMD.int32x4(13, 37, 24, 42);

    var TTFT = SIMD.int32x4(-1, -1, 0, -1);
    var TFTF = SIMD.int32x4(-1, 0, -1, 0);

    var mask = SIMD.int32x4(0xdeadbeef, 0xbaadf00d, 0x00ff1ce, 0xdeadc0de);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.float32x4.select(TTFT, f1, f2), [f1.x, f1.y, f2.z, f1.w]);
        assertEqX4(SIMD.float32x4.select(TFTF, f1, f2), [f1.x, f2.y, f1.z, f2.w]);
        assertEqX4(SIMD.int32x4.select(TFTF, i1, i2), [i1.x, i2.y, i1.z, i2.w]);
        assertEqX4(SIMD.int32x4.select(TTFT, i1, i2), [i1.x, i1.y, i2.z, i1.w]);

        assertEqX4(SIMD.float32x4.bitselect(mask, f1, f2), bitselect(SIMD.float32x4, mask, f1, f2));
        assertEqX4(SIMD.int32x4.bitselect(mask, i1, i2), bitselect(SIMD.int32x4, mask, i1, i2));
    }
}

f();

