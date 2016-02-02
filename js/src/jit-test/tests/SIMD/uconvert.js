load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 30);

// Testing Uint32 <-> Float32 conversions.
// These conversions deserve special attention because SSE doesn't provide
// simple conversion instructions.

// Convert an Uint32Array to a Float32Array using scalar conversions.
function cvt_utof_scalar(u32s, f32s) {
    assertEq(u32s.length, f32s.length);
    for (var i = 0; i < u32s.length; i++) {
        f32s[i] = u32s[i];
    }
}

// Convert an Uint32Array to a Float32Array using simd conversions.
function cvt_utof_simd(u32s, f32s) {
    assertEq(u32s.length, f32s.length);
    for (var i = 0; i < u32s.length; i += 4) {
        SIMD.Float32x4.store(f32s, i, SIMD.Float32x4.fromUint32x4(SIMD.Uint32x4.load(u32s, i)));
    }
}

// Convert a Float32Array to an Uint32Array using scalar conversions.
function cvt_ftou_scalar(f32s, u32s) {
    assertEq(f32s.length, u32s.length);
    for (var i = 0; i < f32s.length; i++) {
        u32s[i] = f32s[i];
    }
}

// Convert a Float32Array to an Uint32Array using simd conversions.
function cvt_ftou_simd(f32s, u32s) {
    assertEq(f32s.length, u32s.length);
    for (var i = 0; i < f32s.length; i += 4) {
        SIMD.Uint32x4.store(u32s, i, SIMD.Uint32x4.fromFloat32x4(SIMD.Float32x4.load(f32s, i)));
    }
}

function check(a, b) {
    assertEq(a.length, b.length);
    for (var i = 0; i < a.length; i++) {
        assertEq(a[i], b[i]);
    }
}

// Uint32x4 --> Float32x4 tests.
var src = new Uint32Array(8000);
var dst1 = new Float32Array(8000);
var dst2 = new Float32Array(8000);

for (var i = 0; i < 2000; i++) {
    src[i] = i;
    src[i + 2000] = 0x7fffffff - i;
    src[i + 4000] = 0x80000000 + i;
    src[i + 6000] = 0xffffffff - i;
}

for (var n = 0; n < 10; n++) {
    cvt_utof_scalar(src, dst1);
    cvt_utof_simd(src, dst2);
    check(dst1, dst2);
}

// Float32x4 --> Uint32x4 tests.
var fsrc = dst1;
var fdst1 = new Uint32Array(8000);
var fdst2 = new Uint32Array(8000);

// The 0xffffffff entries in fsrc round to 0x1.0p32f which throws.
// Go as high as 0x0.ffffffp32f.
for (var i = 0; i < 2000; i++) {
    fsrc[i + 6000] = 0xffffff7f - i;
}

for (var n = 0; n < 10; n++) {
    cvt_ftou_scalar(fsrc, fdst1);
    cvt_ftou_simd(fsrc, fdst2);
    check(fdst1, fdst2);
}
