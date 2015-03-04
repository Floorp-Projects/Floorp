load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 40);

function f() {
    var f32 = new Float32Array(16);
    for (var i = 0; i < 16; i++)
        f32[i] = i + 1;

    var f64 = new Float64Array(f32.buffer);
    var i32 = new Int32Array(f32.buffer);
    var u32 = new Uint32Array(f32.buffer);
    var i16 = new Int16Array(f32.buffer);
    var u16 = new Uint16Array(f32.buffer);
    var i8  = new Int8Array(f32.buffer);
    var u8  = new Uint8Array(f32.buffer);

    var f4 = SIMD.float32x4(42, 43, 44, 45);

    function check() {
        assertEq(f32[0], 42);
        assertEq(f32[1], 43);
        assertEq(f32[2], 44);
        assertEq(f32[3], 45);

        f32[0] = 1;
        f32[1] = 2;
        f32[2] = 3;
        f32[3] = 4;
    }

    for (var i = 0; i < 150; i++) {
        SIMD.float32x4.store(f64, 0, f4);
        check();
        SIMD.float32x4.store(f32, 0, f4);
        check();
        SIMD.float32x4.store(i32, 0, f4);
        check();
        SIMD.float32x4.store(u32, 0, f4);
        check();
        SIMD.float32x4.store(i16, 0, f4);
        check();
        SIMD.float32x4.store(u16, 0, f4);
        check();
        SIMD.float32x4.store(i8, 0, f4);
        check();
        SIMD.float32x4.store(u8, 0, f4);
        check();

        var caught = false;
        try {
            SIMD.float32x4.store(i8, (i < 149) ? 0 : (16 << 2) - (4 << 2) + 1, f4);
            check();
        } catch (e) {
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }
}

f();

