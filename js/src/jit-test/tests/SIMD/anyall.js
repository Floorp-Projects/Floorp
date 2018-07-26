load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function all(B, n) {
    var a = B.splat(true);
    for (var i = 0; i < n; i++) {
        var b = B.replaceLane(a, i, false);
        assertEq(B.allTrue(b), false);
        var c = B.replaceLane(b, i, true);
        assertEq(B.allTrue(c), true);
    }
}

function any(B, n) {
    var a = B.splat(false);
    for (var i = 0; i < n; i++) {
        var b = B.replaceLane(a, i, true);
        assertEq(B.anyTrue(b), true);
        var c = B.replaceLane(b, i, false);
        assertEq(B.anyTrue(c), false);
    }
}

function f() {
    for (var j = 0; j < 200; j++) {
        all(SIMD.Bool64x2, 2)
        any(SIMD.Bool64x2, 2)
        all(SIMD.Bool32x4, 4)
        any(SIMD.Bool32x4, 4)
        all(SIMD.Bool16x8, 8)
        any(SIMD.Bool16x8, 8)
        all(SIMD.Bool8x16, 16)
        any(SIMD.Bool8x16, 16)
    }
}

f()
