load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function select(type, mask, ifTrue, ifFalse) {
    var arr = [];
    for (var i = 0; i < 4; i++) {
        var selector = SIMD.Bool32x4.extractLane(mask, i);
        arr.push(type.extractLane(selector ? ifTrue : ifFalse, i));
    }
    return arr;
}

function f() {
    var f1 = SIMD.Float32x4(1, 2, 3, 4);
    var f2 = SIMD.Float32x4(NaN, Infinity, 3.14, -0);

    var i1 = SIMD.Int32x4(2, 3, 5, 8);
    var i2 = SIMD.Int32x4(13, 37, 24, 42);

    var TTFT = SIMD.Bool32x4(true, true, false, true);
    var TFTF = SIMD.Bool32x4(true, false, true, false);

    var mask = SIMD.Int32x4(0xdeadbeef, 0xbaadf00d, 0x00ff1ce, 0xdeadc0de);

    for (var i = 0; i < 150; i++) {
        assertEqX4(SIMD.Float32x4.select(TTFT, f1, f2), select(SIMD.Float32x4, TTFT, f1, f2));
        assertEqX4(SIMD.Float32x4.select(TFTF, f1, f2), select(SIMD.Float32x4, TFTF, f1, f2));

        assertEqX4(SIMD.Int32x4.select(TFTF, i1, i2), select(SIMD.Int32x4, TFTF, i1, i2));
        assertEqX4(SIMD.Int32x4.select(TTFT, i1, i2), select(SIMD.Int32x4, TTFT, i1, i2));
    }
}

f();
