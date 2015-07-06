load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function Int32x4FromTypeBits(type, vec) {
    if (type == SIMD.Int32x4)
        return vec;
    if (type == SIMD.Float32x4)
        return SIMD.Int32x4.fromFloat32x4Bits(vec);
    throw 'unimplemented';
}

function selectBits(type, mask, ifTrue, ifFalse) {
    var Int32x4 = SIMD.Int32x4;
    var tv = Int32x4FromTypeBits(type, ifTrue);
    var fv = Int32x4FromTypeBits(type, ifFalse);
    var tr = Int32x4.and(mask, tv);
    var fr = Int32x4.and(Int32x4.not(mask), fv);
    var orApplied = Int32x4.or(tr, fr);
    var converted = type == Int32x4 ? orApplied : type.fromInt32x4Bits(orApplied);
    return simdToArray(converted);
}

function select(type, mask, ifTrue, ifFalse) {
    var arr = [];
    for (var i = 0; i < 4; i++) {
        var selector = SIMD.Int32x4.extractLane(mask, i);
        arr.push(type.extractLane(selector === -1 ? ifTrue : ifFalse, i));
    }
    return arr;
}

function f() {
    var f1 = SIMD.Float32x4(1, 2, 3, 4);
    var f2 = SIMD.Float32x4(NaN, Infinity, 3.14, -0);

    var i1 = SIMD.Int32x4(2, 3, 5, 8);
    var i2 = SIMD.Int32x4(13, 37, 24, 42);

    var TTFT = SIMD.Int32x4(-1, -1, 0, -1);
    var TFTF = SIMD.Int32x4(-1, 0, -1, 0);

    var mask = SIMD.Int32x4(0xdeadbeef, 0xbaadf00d, 0x00ff1ce, 0xdeadc0de);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Float32x4.select(TTFT, f1, f2), select(SIMD.Float32x4, TTFT, f1, f2));
        assertEqX4(SIMD.Float32x4.select(TFTF, f1, f2), select(SIMD.Float32x4, TFTF, f1, f2));
        assertEqX4(SIMD.Int32x4.select(TFTF, i1, i2), select(SIMD.Int32x4, TFTF, i1, i2));
        assertEqX4(SIMD.Int32x4.select(TTFT, i1, i2), select(SIMD.Int32x4, TTFT, i1, i2));

        assertEqX4(SIMD.Float32x4.selectBits(mask, f1, f2), selectBits(SIMD.Float32x4, mask, f1, f2));
        assertEqX4(SIMD.Int32x4.selectBits(mask, i1, i2), selectBits(SIMD.Int32x4, mask, i1, i2));
    }
}

f();

